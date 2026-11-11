/********************************************************************************
 * Copyright (c) 2025, 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/
#include "score/mw/com/impl/bindings/lola/service_discovery/client/service_discovery_client.h"

#include "score/mw/com/impl/bindings/lola/service_discovery/test/destructor_notifier.h"
#include "score/mw/com/impl/bindings/lola/service_discovery/test/service_discovery_client_test_fixtures.h"
#include "score/mw/com/impl/bindings/lola/service_discovery/test/service_discovery_client_test_resources.h"
#include "score/mw/com/impl/configuration/lola_service_id.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/i_service_discovery.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <string>

namespace score::mw::com::impl::lola::test
{
namespace
{

using namespace ::testing;

const LolaServiceId kServiceId{1U};
const auto kInstanceSpecifierString = InstanceSpecifier::Create(std::string{"/bla/blub/specifier"}).value();
ConfigurationStore kConfigStoreQm1{
    kInstanceSpecifierString,
    make_ServiceIdentifierType("foo"),
    QualityType::kASIL_QM,
    kServiceId,
    LolaServiceInstanceId{1U},
};
ConfigurationStore kConfigStoreQm2{
    kInstanceSpecifierString,
    make_ServiceIdentifierType("foo"),
    QualityType::kASIL_QM,
    kServiceId,
    LolaServiceInstanceId{2U},
};
ConfigurationStore kConfigStoreFindAny{kInstanceSpecifierString,
                                       make_ServiceIdentifierType("foo"),
                                       QualityType::kASIL_QM,
                                       kServiceId,
                                       std::optional<LolaServiceInstanceId>{}};

HandleType kHandleQm1{kConfigStoreQm1.GetHandle()};
HandleType kHandleQm2{kConfigStoreQm2.GetHandle()};
HandleType kHandleFindAnyQm1{
    kConfigStoreFindAny.GetHandle(ServiceInstanceId{kConfigStoreQm1.lola_instance_id_.value()})};
HandleType kHandleFindAnyQm2{
    kConfigStoreFindAny.GetHandle(ServiceInstanceId{kConfigStoreQm2.lola_instance_id_.value()})};

class ServiceDiscoveryClientSearchReplacementFixture : public ServiceDiscoveryClientFixture,
                                                       public WithParamInterface<bool>
{
};

TEST_P(ServiceDiscoveryClientSearchReplacementFixture, NewSearchDoesNotReceiveRemovalQueuedForStoppedSearch)
{
    RecordProperty("given", "a stopped search with an unread removal event and a reoffered service");
    RecordProperty("when", "a new search starts before the worker processes the old removal");
    RecordProperty("then", "the new search does not report the reoffered service as unavailable");

    std::promise<void> read_removal{};
    auto read_removal_future = read_removal.get_future();
    std::promise<void> removal_read{};
    std::promise<void> process_removal{};
    auto process_removal_future = process_removal.get_future();
    std::promise<void> removal_processed{};
    std::promise<void> read_reoffer{};
    auto read_reoffer_future = read_reoffer.get_future();

    EXPECT_CALL(inotify_instance_mock_, Read())
        .WillOnce([this, &read_removal_future, &removal_read, &process_removal_future] {
            read_removal_future.wait();
            auto events = inotify_instance_->Read();
            removal_read.set_value();
            process_removal_future.wait();
            return events;
        })
        .WillOnce([this, &removal_processed, &read_reoffer_future] {
            removal_processed.set_value();
            read_reoffer_future.wait();
            return inotify_instance_->Read();
        })
        .WillRepeatedly(DoDefault());
    EXPECT_CALL(inotify_instance_mock_, RemoveWatch(_)).Times(0);

    WhichContainsAServiceDiscoveryClient().WithAnOfferedService(kConfigStoreQm1.GetInstanceIdentifier());
    const auto old_search = make_FindServiceHandle(1U);
    std::atomic<std::size_t> unavailable_notifications{0U};
    std::promise<void> unavailable{};
    const auto replace_search = [&] {
        EXPECT_TRUE(service_discovery_client_->StopFindService(old_search).has_value());
        EXPECT_TRUE(service_discovery_client_
                        ->StopOfferService(kConfigStoreQm1.GetInstanceIdentifier(),
                                           IServiceDiscovery::QualityTypeSelector::kBoth)
                        .has_value());
        read_removal.set_value();
        removal_read.get_future().wait();
        EXPECT_TRUE(service_discovery_client_->OfferService(kConfigStoreQm1.GetInstanceIdentifier()).has_value());
        WithAnActiveStartFindService(kConfigStoreQm1.GetInstanceIdentifier(),
                                     make_FindServiceHandle(2U),
                                     [&unavailable_notifications, &unavailable](auto handles, auto) {
                                         if (handles.empty())
                                         {
                                             if (unavailable_notifications.fetch_add(1U) == 0U)
                                             {
                                                 unavailable.set_value();
                                             }
                                         }
                                     });
        process_removal.set_value();
    };
    if (GetParam())
    {
        WithAnActiveStartFindService(
            kConfigStoreQm1.GetInstanceIdentifier(), old_search, [&replace_search](auto, auto) {
                replace_search();
            });
    }
    else
    {
        WithAnActiveStartFindService(kConfigStoreQm1.GetInstanceIdentifier(), old_search);
        replace_search();
    }
    removal_processed.get_future().wait();
    EXPECT_EQ(unavailable_notifications.load(), 0U);
    EXPECT_TRUE(
        service_discovery_client_
            ->StopOfferService(kConfigStoreQm1.GetInstanceIdentifier(), IServiceDiscovery::QualityTypeSelector::kBoth)
            .has_value());
    read_reoffer.set_value();
    EXPECT_EQ(unavailable.get_future().wait_for(std::chrono::seconds{5}), std::future_status::ready);
    std::atomic<std::size_t> stale_availability{0U};
    WithAnActiveStartFindService(
        kConfigStoreQm1.GetInstanceIdentifier(), make_FindServiceHandle(3U), [&stale_availability](auto handles, auto) {
            if (!handles.empty())
            {
                ++stale_availability;
            }
        });
    service_discovery_client_.reset();
    EXPECT_EQ(unavailable_notifications.load(), 1U);
    EXPECT_EQ(stale_availability.load(), 0U);
}

INSTANTIATE_TEST_SUITE_P(ExternalAndReentrant, ServiceDiscoveryClientSearchReplacementFixture, Values(false, true));

TEST_F(ServiceDiscoveryClientFixture, ExistingSearchReceivesWithdrawalBeforeReoffer)
{
    RecordProperty("given", "an ongoing search with an unread withdrawal event");
    RecordProperty("when", "the service is reoffered before the withdrawal is processed");
    RecordProperty("then", "the existing search is notified of the withdrawal");

    std::promise<void> read_removal{};
    auto read_removal_future = read_removal.get_future();
    std::promise<void> removal_read{};
    std::promise<void> process_removal{};
    auto process_removal_future = process_removal.get_future();
    std::promise<void> removal_processed{};
    EXPECT_CALL(inotify_instance_mock_, Read())
        .WillOnce([this, &read_removal_future, &removal_read, &process_removal_future] {
            read_removal_future.wait();
            auto events = inotify_instance_->Read();
            removal_read.set_value();
            process_removal_future.wait();
            return events;
        })
        .WillOnce([this, &removal_processed] {
            removal_processed.set_value();
            return inotify_instance_->Read();
        })
        .WillRepeatedly(DoDefault());

    WhichContainsAServiceDiscoveryClient().WithAnOfferedService(kConfigStoreQm1.GetInstanceIdentifier());
    std::atomic<std::size_t> unavailable_notifications{0U};
    WithAnActiveStartFindService(kConfigStoreQm1.GetInstanceIdentifier(),
                                 make_FindServiceHandle(1U),
                                 [&unavailable_notifications](auto handles, auto) {
                                     if (handles.empty())
                                     {
                                         ++unavailable_notifications;
                                     }
                                 });
    ASSERT_TRUE(
        service_discovery_client_
            ->StopOfferService(kConfigStoreQm1.GetInstanceIdentifier(), IServiceDiscovery::QualityTypeSelector::kBoth)
            .has_value());
    read_removal.set_value();
    removal_read.get_future().wait();
    ASSERT_TRUE(service_discovery_client_->OfferService(kConfigStoreQm1.GetInstanceIdentifier()).has_value());
    process_removal.set_value();
    removal_processed.get_future().wait();
    service_discovery_client_.reset();
    EXPECT_EQ(unavailable_notifications.load(), 1U);
}

TEST_F(ServiceDiscoveryClientFixture, StartingSearchFromHandlerDoesNotDestroyExecutingHandler)
{
    RecordProperty("given", "a handler that stops its own search");
    RecordProperty("when", "the handler starts another search before returning");
    RecordProperty("then", "the executing handler remains alive until it returns");

    WhichContainsAServiceDiscoveryClient().WithAnOfferedService(kConfigStoreQm1.GetInstanceIdentifier());
    auto lifetime = std::make_shared<int>(0);
    const std::pair<ServiceDiscoveryClient*, std::weak_ptr<int>> context{service_discovery_client_.get(), lifetime};
    WithAnActiveStartFindService(
        kConfigStoreQm1.GetInstanceIdentifier(),
        make_FindServiceHandle(1U),
        [lifetime = std::move(lifetime), &context](auto, auto handle) {
            EXPECT_TRUE(context.first->StopFindService(handle).has_value());
            EXPECT_TRUE(context.first
                            ->StartFindService(
                                make_FindServiceHandle(2U),
                                [](auto, auto) {},
                                EnrichedInstanceIdentifier{kConfigStoreQm1.GetInstanceIdentifier()})
                            .has_value());
            EXPECT_FALSE(context.second.expired());
        });
}

TEST_F(ServiceDiscoveryClientFixture, CorrectlyAssociatesSubsearchWithCorrectDirectory)
{
    std::promise<void> services_offered_barrier{};
    auto services_offered_barrier_future = services_offered_barrier.get_future();
    std::promise<void> handler_destruction_barrier{};
    DestructorNotifier destructor_notifier{&handler_destruction_barrier};

    EXPECT_CALL(inotify_instance_mock_, Read())
        .WillOnce([this, &services_offered_barrier_future] {
            services_offered_barrier_future.wait();
            return inotify_instance_->Read();
        })
        .WillRepeatedly([] {
            return score::cpp::static_vector<os::InotifyEvent, os::InotifyInstance::max_events>{};
        });

    // With a handler
    StrictMock<MockFunction<void(ServiceHandleContainer<HandleType>, FindServiceHandle)>> find_service_handler{};

    // Where the first invocation does nothing and the second calling StopFindService
    EXPECT_CALL(find_service_handler, Call(_, _))
        .WillOnce(DoDefault())
        .WillOnce(Invoke([this](const auto& handles, const auto& find_service_handle) {
            ASSERT_TRUE(handles.empty());
            const auto result = service_discovery_client_->StopFindService(find_service_handle);
            ASSERT_TRUE(result.has_value());
        }));

    // Given a ServiceDiscoveryClient with mocked inotify instance with an offered service instance and a
    // DestructorNotifier object which will set the value of the handler_destruction_barrier promise on destruction
    WhichContainsAServiceDiscoveryClient().WithAnOfferedService(kConfigStoreQm1.GetInstanceIdentifier());

    // when calling StartFindService
    FindServiceHandle expected_handle{make_FindServiceHandle(1U)};
    const auto start_find_service_result = service_discovery_client_->StartFindService(
        expected_handle,
        [&find_service_handler, destructor_notifier = std::move(destructor_notifier)](
            ServiceHandleContainer<HandleType> containers, FindServiceHandle handle) noexcept {
            find_service_handler.AsStdFunction()(containers, handle);
        },
        EnrichedInstanceIdentifier{kConfigStoreFindAny.GetInstanceIdentifier()});
    EXPECT_TRUE(start_find_service_result.has_value());

    // and a stop offer waiting in the event queue
    EXPECT_TRUE(
        service_discovery_client_
            ->StopOfferService(kConfigStoreQm1.GetInstanceIdentifier(), IServiceDiscovery::QualityTypeSelector::kBoth)
            .has_value());
    services_offered_barrier.set_value();

    // Then the handler passed to StartFindService should not crash and be called two times
    handler_destruction_barrier.get_future().wait();
}

TEST_F(ServiceDiscoveryClientFixture, DoesNotCallHandlerIfFindServiceIsStoppedButEventInSameBatchFits)
{
    std::promise<void> services_offered_barrier{};
    auto services_offered_barrier_future = services_offered_barrier.get_future();
    std::promise<void> handler_destruction_barrier{};
    DestructorNotifier destructor_notifier{&handler_destruction_barrier};

    EXPECT_CALL(inotify_instance_mock_, Read())
        .WillOnce([this, &services_offered_barrier_future] {
            services_offered_barrier_future.wait();
            return inotify_instance_->Read();
        })
        .WillRepeatedly([] {
            return score::cpp::static_vector<os::InotifyEvent, os::InotifyInstance::max_events>{};
        });

    // With a handler
    StrictMock<MockFunction<void(ServiceHandleContainer<HandleType>, FindServiceHandle)>> find_service_handler{};

    // Where the first invocation does nothing and the second calling StopFindService
    EXPECT_CALL(find_service_handler, Call(_, _))
        .WillOnce(DoDefault())
        .WillOnce(WithArg<1>(Invoke([this](const auto& find_service_handle) {
            const auto result = service_discovery_client_->StopFindService(find_service_handle);
            ASSERT_TRUE(result.has_value());
        })));

    // Given a ServiceDiscoveryClient with mocked inotify instance with an offered service instance and a
    // DestructorNotifier object which will set the value of the handler_destruction_barrier promise on destruction
    WhichContainsAServiceDiscoveryClient().WithAnOfferedService(kConfigStoreQm1.GetInstanceIdentifier());

    // when calling StartFindService
    FindServiceHandle expected_handle{make_FindServiceHandle(1U)};
    const auto start_find_service_result = service_discovery_client_->StartFindService(
        expected_handle,
        [&find_service_handler, destructor_notifier = std::move(destructor_notifier)](
            ServiceHandleContainer<HandleType> containers, FindServiceHandle handle) noexcept {
            find_service_handler.AsStdFunction()(containers, handle);
        },
        EnrichedInstanceIdentifier{kConfigStoreFindAny.GetInstanceIdentifier()});
    EXPECT_TRUE(start_find_service_result.has_value());

    // and two additional events waiting in one batch after the search is started
    EXPECT_TRUE(
        service_discovery_client_
            ->StopOfferService(kConfigStoreQm1.GetInstanceIdentifier(), IServiceDiscovery::QualityTypeSelector::kBoth)
            .has_value());
    EXPECT_TRUE(service_discovery_client_->OfferService(kConfigStoreQm2.GetInstanceIdentifier()).has_value());
    services_offered_barrier.set_value();

    // Then the handler passed to StartFindService should not be called a third time (we stop waiting once the handler
    // is destroyed, indicated by the destructor of DestructorNotifier).
    handler_destruction_barrier.get_future().wait();
}

TEST_F(ServiceDiscoveryClientFixture, ReCallsCorrectHandlerForSpecificInstanceIDAfterReoffering)
{
    InSequence in_sequence{};

    StrictMock<MockFunction<void(ServiceHandleContainer<HandleType>, FindServiceHandle)>> find_service_handler_1{};
    StrictMock<MockFunction<void(ServiceHandleContainer<HandleType>, FindServiceHandle)>> find_service_handler_2{};

    std::promise<void> service_found_barrier_1{};
    std::promise<void> service_found_barrier_2{};
    std::promise<void> service_found_barrier_after_stop_offer{};
    std::promise<void> service_found_barrier_after_offer{};

    FindServiceHandle expected_handle_1{make_FindServiceHandle(1U)};
    FindServiceHandle expected_handle_2{make_FindServiceHandle(2U)};

    // Expecting that both handlers are called when the services are initially offered
    EXPECT_CALL(find_service_handler_1, Call(_, expected_handle_1))
        .WillOnce(WithArg<0>(Invoke([&service_found_barrier_1](auto container) {
            EXPECT_EQ(container.size(), 1);
            EXPECT_EQ(container.front(), kHandleQm1);
            service_found_barrier_1.set_value();
        })));
    EXPECT_CALL(find_service_handler_2, Call(_, expected_handle_2))
        .WillOnce(WithArg<0>(Invoke([&service_found_barrier_2](auto container) {
            EXPECT_EQ(container.size(), 1);
            EXPECT_EQ(container.front(), kHandleQm2);
            service_found_barrier_2.set_value();
        })));

    // and expecting that only the handler for the first service instance is called again when StopOfferService is
    // called on that instance
    EXPECT_CALL(find_service_handler_1, Call(_, expected_handle_1))
        .WillOnce(WithArg<0>(Invoke([&service_found_barrier_after_stop_offer](auto container) {
            EXPECT_EQ(container.size(), 0);
            service_found_barrier_after_stop_offer.set_value();
        })));

    // and expecting that only the handler for the first service instance is called again when OfferService is again
    // called on that instance
    EXPECT_CALL(find_service_handler_1, Call(_, expected_handle_1))
        .WillOnce(WithArg<0>(Invoke([&service_found_barrier_after_offer](auto container) {
            EXPECT_EQ(container.size(), 1);
            EXPECT_EQ(container.front(), kHandleQm1);
            service_found_barrier_after_offer.set_value();
        })));

    WhichContainsAServiceDiscoveryClient();

    const auto start_find_service_result_1 = service_discovery_client_->StartFindService(
        expected_handle_1,
        CreateWrappedMockFindServiceHandler(find_service_handler_1),
        EnrichedInstanceIdentifier{kConfigStoreQm1.GetInstanceIdentifier()});
    ASSERT_TRUE(start_find_service_result_1.has_value());

    const auto start_find_service_result_2 = service_discovery_client_->StartFindService(
        expected_handle_2,
        CreateWrappedMockFindServiceHandler(find_service_handler_2),
        EnrichedInstanceIdentifier{kConfigStoreQm2.GetInstanceIdentifier()});
    ASSERT_TRUE(start_find_service_result_2.has_value());

    EXPECT_TRUE(service_discovery_client_->OfferService(kConfigStoreQm1.GetInstanceIdentifier()).has_value());
    service_found_barrier_1.get_future().wait();

    EXPECT_TRUE(service_discovery_client_->OfferService(kConfigStoreQm2.GetInstanceIdentifier()).has_value());
    service_found_barrier_2.get_future().wait();

    ASSERT_TRUE(
        service_discovery_client_
            ->StopOfferService(kConfigStoreQm1.GetInstanceIdentifier(), IServiceDiscovery::QualityTypeSelector::kBoth)
            .has_value());
    service_found_barrier_after_stop_offer.get_future().wait();

    EXPECT_TRUE(service_discovery_client_->OfferService(kConfigStoreQm1.GetInstanceIdentifier()).has_value());
    service_found_barrier_after_offer.get_future().wait();
}

TEST_F(ServiceDiscoveryClientFixture, ReCallsCorrectHandlerForAnyInstanceIDsAfterReoffering)
{
    InSequence in_sequence{};

    std::promise<void> service_found_barrier_1{};
    std::promise<void> service_found_barrier_2{};
    std::promise<void> service_found_barrier_after_stop_offer{};
    std::promise<void> service_found_barrier_after_offer{};

    FindServiceHandle expected_handle{make_FindServiceHandle(1U)};

    StrictMock<MockFunction<void(ServiceHandleContainer<HandleType>, FindServiceHandle)>> find_service_handler{};

    // Expecting that the handler is called twice when the services are initially offered
    EXPECT_CALL(find_service_handler, Call(_, expected_handle))
        .WillOnce(WithArg<0>(Invoke([&service_found_barrier_1](auto container) {
            EXPECT_EQ(container.size(), 1);
            EXPECT_EQ(container[0], kHandleFindAnyQm1);
            service_found_barrier_1.set_value();
        })));
    EXPECT_CALL(find_service_handler, Call(_, expected_handle))
        .WillOnce(WithArg<0>(Invoke([&service_found_barrier_2](auto container) {
            EXPECT_EQ(container.size(), 2);
            EXPECT_THAT(container, ::testing::Contains(kHandleFindAnyQm1));
            EXPECT_THAT(container, ::testing::Contains(kHandleFindAnyQm2));
            service_found_barrier_2.set_value();
        })));

    // and expecting that the handler is called again containing only the second instance when StopOfferService is
    // called on the first instance
    EXPECT_CALL(find_service_handler, Call(_, expected_handle))
        .WillOnce(WithArg<0>(Invoke([&service_found_barrier_after_stop_offer](auto container) {
            EXPECT_EQ(container.size(), 1);
            EXPECT_EQ(container[0], kHandleFindAnyQm2);
            service_found_barrier_after_stop_offer.set_value();
        })));

    // and expecting that only the handler is called again when the first instance is offered again
    EXPECT_CALL(find_service_handler, Call(_, _))
        .WillOnce(WithArg<0>(Invoke([&service_found_barrier_after_offer](auto container) {
            EXPECT_EQ(container.size(), 2);
            EXPECT_THAT(container, ::testing::Contains(kHandleFindAnyQm1));
            EXPECT_THAT(container, ::testing::Contains(kHandleFindAnyQm2));
            service_found_barrier_after_offer.set_value();
        })));

    WhichContainsAServiceDiscoveryClient();

    const auto start_find_service_result = service_discovery_client_->StartFindService(
        expected_handle,
        CreateWrappedMockFindServiceHandler(find_service_handler),
        EnrichedInstanceIdentifier{kConfigStoreFindAny.GetInstanceIdentifier()});
    EXPECT_TRUE(start_find_service_result.has_value());

    EXPECT_TRUE(service_discovery_client_->OfferService(kConfigStoreQm1.GetInstanceIdentifier()).has_value());
    service_found_barrier_1.get_future().wait();

    EXPECT_TRUE(service_discovery_client_->OfferService(kConfigStoreQm2.GetInstanceIdentifier()).has_value());
    service_found_barrier_2.get_future().wait();

    ASSERT_TRUE(
        service_discovery_client_
            ->StopOfferService(kConfigStoreQm1.GetInstanceIdentifier(), IServiceDiscovery::QualityTypeSelector::kBoth)
            .has_value());
    service_found_barrier_after_stop_offer.get_future().wait();

    EXPECT_TRUE(service_discovery_client_->OfferService(kConfigStoreQm1.GetInstanceIdentifier()).has_value());
    service_found_barrier_after_offer.get_future().wait();
}

TEST_F(ServiceDiscoveryClientFixture, ReCallsCorrectHandlerForDifferentInstanceIDsAfterRestartingStartFindService)
{
    InSequence in_sequence{};

    std::promise<void> service_found_barrier_1{};
    std::promise<void> service_found_barrier_2{};
    std::promise<void> service_found_barrier_after_start_find_service{};

    FindServiceHandle expected_handle_1{make_FindServiceHandle(1U)};
    FindServiceHandle expected_handle_2{make_FindServiceHandle(2U)};

    FindServiceHandle expected_handle_1_second_start_find_service{make_FindServiceHandle(3U)};

    StrictMock<MockFunction<void(ServiceHandleContainer<HandleType>, FindServiceHandle)>> find_service_handler{};
    StrictMock<MockFunction<void(ServiceHandleContainer<HandleType>, FindServiceHandle)>>
        find_service_handler_after_restart{};

    // Expecting that the handler is called twice when the services are initially offered
    EXPECT_CALL(find_service_handler, Call(_, expected_handle_1))
        .WillOnce(WithArg<0>(Invoke([&service_found_barrier_1](auto container) {
            EXPECT_EQ(container.size(), 1);
            EXPECT_EQ(container[0], kHandleQm1);
            service_found_barrier_1.set_value();
        })));
    EXPECT_CALL(find_service_handler, Call(_, expected_handle_2))
        .WillOnce(WithArg<0>(Invoke([&service_found_barrier_2](auto container) {
            EXPECT_EQ(container.size(), 1);
            EXPECT_EQ(container[0], kHandleQm2);
            service_found_barrier_2.set_value();
        })));

    // and expecting that the handler is called again for the first instance when StartFindService is called again after
    // StopFindService
    EXPECT_CALL(find_service_handler_after_restart, Call(_, expected_handle_1_second_start_find_service))
        .WillOnce(WithArg<0>(Invoke([&service_found_barrier_after_start_find_service](auto container) {
            EXPECT_EQ(container.size(), 1);
            EXPECT_EQ(container[0], kHandleQm1);
            service_found_barrier_after_start_find_service.set_value();
        })));

    WhichContainsAServiceDiscoveryClient();

    const auto find_service_result = service_discovery_client_->StartFindService(
        expected_handle_1,
        CreateWrappedMockFindServiceHandler(find_service_handler),
        EnrichedInstanceIdentifier{kConfigStoreQm1.GetInstanceIdentifier()});
    ASSERT_TRUE(find_service_result.has_value());

    const auto find_service_result_2 = service_discovery_client_->StartFindService(
        expected_handle_2,
        CreateWrappedMockFindServiceHandler(find_service_handler),
        EnrichedInstanceIdentifier{kConfigStoreQm2.GetInstanceIdentifier()});
    ASSERT_TRUE(find_service_result_2.has_value());

    EXPECT_TRUE(service_discovery_client_->OfferService(kConfigStoreQm1.GetInstanceIdentifier()).has_value());
    service_found_barrier_1.get_future().wait();

    EXPECT_TRUE(service_discovery_client_->OfferService(kConfigStoreQm2.GetInstanceIdentifier()).has_value());
    service_found_barrier_2.get_future().wait();

    ASSERT_TRUE(service_discovery_client_->StopFindService(expected_handle_1).has_value());

    const auto find_service_result_3 = service_discovery_client_->StartFindService(
        expected_handle_1_second_start_find_service,
        CreateWrappedMockFindServiceHandler(find_service_handler_after_restart),
        EnrichedInstanceIdentifier{kConfigStoreQm1.GetInstanceIdentifier()});
    ASSERT_TRUE(find_service_result_3.has_value());
    service_found_barrier_after_start_find_service.get_future().wait();
}

TEST_F(ServiceDiscoveryClientFixture, ReCallsCorrectHandlerForAnyInstanceIDsAfterRestartingStartFindService)
{
    InSequence in_sequence{};

    std::promise<void> service_found_barrier_1{};
    std::promise<void> service_found_barrier_2{};
    std::promise<void> service_found_barrier_after_start_find_service{};

    FindServiceHandle expected_handle{make_FindServiceHandle(1U)};
    FindServiceHandle expected_handle_second_start_find_service{make_FindServiceHandle(2U)};

    StrictMock<MockFunction<void(ServiceHandleContainer<HandleType>, FindServiceHandle)>> find_service_handler{};
    StrictMock<MockFunction<void(ServiceHandleContainer<HandleType>, FindServiceHandle)>>
        find_service_handler_after_restart{};

    // Expecting that the handler is called twice when the services are initially offered
    EXPECT_CALL(find_service_handler, Call(_, expected_handle))
        .WillOnce(WithArg<0>(Invoke([&service_found_barrier_1](auto container) {
            EXPECT_EQ(container.size(), 1);
            EXPECT_EQ(container[0], kHandleFindAnyQm1);
            service_found_barrier_1.set_value();
        })));
    EXPECT_CALL(find_service_handler, Call(_, expected_handle))
        .WillOnce(WithArg<0>(Invoke([&service_found_barrier_2](auto container) {
            EXPECT_EQ(container.size(), 2);
            EXPECT_THAT(container, ::testing::Contains(kHandleFindAnyQm1));
            EXPECT_THAT(container, ::testing::Contains(kHandleFindAnyQm2));
            service_found_barrier_2.set_value();
        })));

    // and expecting that the handler is called again when StartFindService is called again after StopFindService
    EXPECT_CALL(find_service_handler_after_restart, Call(_, expected_handle_second_start_find_service))
        .WillOnce(WithArg<0>(Invoke([&service_found_barrier_after_start_find_service](auto container) {
            EXPECT_EQ(container.size(), 2);
            EXPECT_THAT(container, ::testing::Contains(kHandleFindAnyQm1));
            EXPECT_THAT(container, ::testing::Contains(kHandleFindAnyQm2));
            service_found_barrier_after_start_find_service.set_value();
        })));

    WhichContainsAServiceDiscoveryClient();

    const auto find_service_result = service_discovery_client_->StartFindService(
        expected_handle,
        CreateWrappedMockFindServiceHandler(find_service_handler),
        EnrichedInstanceIdentifier{kConfigStoreFindAny.GetInstanceIdentifier()});
    ASSERT_TRUE(find_service_result.has_value());

    EXPECT_TRUE(service_discovery_client_->OfferService(kConfigStoreQm1.GetInstanceIdentifier()).has_value());
    service_found_barrier_1.get_future().wait();

    EXPECT_TRUE(service_discovery_client_->OfferService(kConfigStoreQm2.GetInstanceIdentifier()).has_value());
    service_found_barrier_2.get_future().wait();

    ASSERT_TRUE(service_discovery_client_->StopFindService(expected_handle).has_value());

    const auto find_service_result_2 = service_discovery_client_->StartFindService(
        expected_handle_second_start_find_service,
        CreateWrappedMockFindServiceHandler(find_service_handler_after_restart),
        EnrichedInstanceIdentifier{kConfigStoreFindAny.GetInstanceIdentifier()});

    ASSERT_TRUE(find_service_result_2.has_value());
    service_found_barrier_after_start_find_service.get_future().wait();
}

TEST_F(ServiceDiscoveryClientFixture, FilesystemIsNotRecrawledIfExactSameSearchAlreadyExists)
{
    std::promise<void> barrier{};

    // Expecting per crawl and watch of the filesystem for a specific instance we expect one "new" watch, no more!
    EXPECT_CALL(inotify_instance_mock_, AddWatch(_, _)).Times(1);

    FindServiceHandle expected_handle_1{make_FindServiceHandle(1U)};
    FindServiceHandle expected_handle_2{make_FindServiceHandle(2U)};

    // Given a ServiceDiscoveryClient with mocked inotify instance with an offered service instance
    WhichContainsAServiceDiscoveryClient().WithAnOfferedService(kConfigStoreQm1.GetInstanceIdentifier());

    // When starting the same service discovery for above offer twice recursively
    testing::MockFunction<void(std::vector<HandleType>, FindServiceHandle)> inner_handler{};
    EXPECT_CALL(inner_handler, Call(_, _))
        .WillOnce([&barrier, &expected_handle_2](auto container, auto handle) noexcept {
            EXPECT_EQ(container.size(), 1);
            EXPECT_EQ(container.front(), kHandleQm1);
            EXPECT_EQ(handle, expected_handle_2);
            barrier.set_value();
        });

    testing::MockFunction<void(std::vector<HandleType>, FindServiceHandle)> outer_handler{};
    EXPECT_CALL(outer_handler, Call(_, _))
        .WillOnce([this, &expected_handle_2, &inner_handler](auto handles, auto) noexcept {
            ASSERT_EQ(handles.size(), 1U);
            const auto start_find_service_result = service_discovery_client_->StartFindService(
                expected_handle_2,
                [&inner_handler](auto container, auto handle) {
                    inner_handler.AsStdFunction()(container, handle);
                },
                EnrichedInstanceIdentifier{handles.front()});
            EXPECT_TRUE(start_find_service_result.has_value());
        });

    const auto start_find_service_result_1 = service_discovery_client_->StartFindService(
        expected_handle_1,
        [&outer_handler](auto container, auto handle) {
            outer_handler.AsStdFunction()(container, handle);
        },
        EnrichedInstanceIdentifier{kConfigStoreQm1.GetInstanceIdentifier()});
    ASSERT_TRUE(start_find_service_result_1.has_value());

    // Then the service is found both times
    barrier.get_future().wait();
}

TEST_F(ServiceDiscoveryClientFixture, SearchFromCachedSearchReceivesFollowupUpdates)
{
    std::promise<void> barrier{};
    std::promise<void> barrier2{};

    FindServiceHandle expected_handle_1{make_FindServiceHandle(1U)};
    FindServiceHandle expected_handle_2{make_FindServiceHandle(2U)};

    // Given a ServiceDiscoveryClient with mocked inotify instance with an offered service instance
    WhichContainsAServiceDiscoveryClient().WithAnOfferedService(kConfigStoreQm1.GetInstanceIdentifier());

    // Expect both service discoveries to receive the offer and stop-offer of the service instance
    testing::MockFunction<void(std::vector<HandleType>, FindServiceHandle)> inner_handler{};
    EXPECT_CALL(inner_handler, Call(_, _))
        .WillOnce([&barrier, &expected_handle_2](auto container, auto handle) noexcept {
            EXPECT_EQ(container.size(), 1);
            EXPECT_EQ(container.front(), kHandleQm1);
            EXPECT_EQ(handle, expected_handle_2);
            barrier.set_value();
        })
        .WillOnce([&barrier2, &expected_handle_2](auto container, auto handle) noexcept {
            EXPECT_EQ(container.size(), 0);
            EXPECT_EQ(handle, expected_handle_2);
            barrier2.set_value();
        });

    testing::MockFunction<void(std::vector<HandleType>, FindServiceHandle)> outer_handler{};
    EXPECT_CALL(outer_handler, Call(_, _))
        .WillOnce([this, &expected_handle_2, &inner_handler](auto handles, auto) noexcept {
            ASSERT_EQ(handles.size(), 1U);
            const auto start_find_service_result = service_discovery_client_->StartFindService(
                expected_handle_2,
                [&inner_handler](auto container, auto handle) {
                    inner_handler.AsStdFunction()(container, handle);
                },
                EnrichedInstanceIdentifier{handles.front()});
            EXPECT_TRUE(start_find_service_result.has_value());
        })
        .WillRepeatedly([](auto, auto) {});

    // When recursively starting the service discovery
    const auto start_find_service_result_1 = service_discovery_client_->StartFindService(
        expected_handle_1,
        [&outer_handler](auto container, auto handle) {
            outer_handler.AsStdFunction()(container, handle);
        },
        EnrichedInstanceIdentifier{kConfigStoreQm1.GetInstanceIdentifier()});
    ASSERT_TRUE(start_find_service_result_1.has_value());

    barrier.get_future().wait();

    // And then stopping the offer
    EXPECT_TRUE(
        service_discovery_client_
            ->StopOfferService(kConfigStoreQm1.GetInstanceIdentifier(), IServiceDiscovery::QualityTypeSelector::kBoth)
            .has_value());

    barrier2.get_future().wait();
}

TEST_F(ServiceDiscoveryClientFixture, FilesystemIsNotRecrawledIfAnySearchAlreadyExists)
{
    std::promise<void> barrier{};

    FindServiceHandle expected_handle_1{make_FindServiceHandle(1U)};
    FindServiceHandle expected_handle_2{make_FindServiceHandle(2U)};

    // Expecting per crawl and watch of the filesystem for any instance we expect two "new" watches, no more!
    EXPECT_CALL(inotify_instance_mock_, AddWatch(_, _)).Times(2);

    // Given a ServiceDiscoveryClient with mocked inotify instance and an offered service instance
    WhichContainsAServiceDiscoveryClient().WithAnOfferedService(kConfigStoreQm1.GetInstanceIdentifier());

    // When starting the same service discovery for above offer twice recursively
    testing::MockFunction<void(std::vector<HandleType>, FindServiceHandle)> inner_handler{};
    EXPECT_CALL(inner_handler, Call(_, _))
        .WillOnce([&barrier, &expected_handle_2](auto container, auto handle) noexcept {
            EXPECT_EQ(container.size(), 1);
            EXPECT_EQ(container.front(), kHandleFindAnyQm1);
            EXPECT_EQ(handle, expected_handle_2);
            barrier.set_value();
        });

    testing::MockFunction<void(std::vector<HandleType>, FindServiceHandle)> outer_handler{};
    EXPECT_CALL(outer_handler, Call(_, _))
        .WillOnce([this, &expected_handle_2, &inner_handler](auto handles, auto) noexcept {
            ASSERT_EQ(handles.size(), 1U);
            const auto start_find_service_result = service_discovery_client_->StartFindService(
                expected_handle_2,
                [&inner_handler](auto container, auto handle) {
                    inner_handler.AsStdFunction()(container, handle);
                },
                EnrichedInstanceIdentifier{handles.front()});
            EXPECT_TRUE(start_find_service_result.has_value());
        });

    const auto start_find_service_result_1 = service_discovery_client_->StartFindService(
        expected_handle_1,
        [&outer_handler](auto container, auto handle) {
            outer_handler.AsStdFunction()(container, handle);
        },
        EnrichedInstanceIdentifier{kConfigStoreFindAny.GetInstanceIdentifier()});
    ASSERT_TRUE(start_find_service_result_1.has_value());

    // Then the service is found both times
    barrier.get_future().wait();
}

}  // namespace
}  // namespace score::mw::com::impl::lola::test
