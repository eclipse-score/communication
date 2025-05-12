/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
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
#include "score/mw/com/impl/find_service_handle.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/i_service_discovery.h"

#include "score/mw/com/impl/instance_identifier.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

namespace score::mw::com::impl::lola::test
{
namespace
{

using ::testing::_;
using ::testing::AtLeast;
using ::testing::ByMove;
using ::testing::Contains;
using ::testing::DoDefault;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::WithArg;

const LolaServiceId kServiceId{1U};
const auto kInstanceSpecifierString = InstanceSpecifier::Create("/bla/blub/specifier").value();
ConfigurationStore kConfigStoreQm1{kInstanceSpecifierString,
                                   make_ServiceIdentifierType("foo"),
                                   QualityType::kASIL_QM,
                                   kServiceId,
                                   LolaServiceInstanceId{1U}};
ConfigurationStore kConfigStoreQm2{kInstanceSpecifierString,
                                   make_ServiceIdentifierType("foo"),
                                   QualityType::kASIL_QM,
                                   kServiceId,
                                   LolaServiceInstanceId{2U}};
ConfigurationStore kConfigStoreFindAny{kInstanceSpecifierString,
                                       make_ServiceIdentifierType("foo"),
                                       QualityType::kASIL_QM,
                                       kServiceId,
                                       score::cpp::optional<LolaServiceInstanceId>{}};

using ServiceDiscoveryClientStopFindServiceFixture = ServiceDiscoveryClientFixture;
TEST_F(ServiceDiscoveryClientStopFindServiceFixture, RemovesWatchOnStopFindService)
{
    std::promise<void> barrier{};
    EXPECT_CALL(inotify_instance_mock_, RemoveWatch(_)).WillOnce([this, &barrier](auto watch_descriptor) {
        barrier.set_value();
        return inotify_instance_->RemoveWatch(watch_descriptor);
    });

    WhichContainsAServiceDiscoveryClient();

    FindServiceHandle handle{make_FindServiceHandle(1U)};
    const auto start_find_service_result = service_discovery_client_->StartFindService(
        handle, [](auto, auto) noexcept {}, EnrichedInstanceIdentifier{kConfigStoreQm1.GetInstanceIdentifier()});
    ASSERT_TRUE(start_find_service_result.has_value());
    const auto stop_find_service_result = service_discovery_client_->StopFindService(handle);
    ASSERT_TRUE(stop_find_service_result.has_value());

    CreateRegularFile(
        filesystem_,
        GetFlagFilePrefix(kServiceId, kConfigStoreQm1.lola_instance_id_.value(), GetServiceDiscoveryPath()));

    barrier.get_future().wait();
}

TEST_F(ServiceDiscoveryClientStopFindServiceFixture, DoesNotCallHandlerIfFindServiceIsStopped)
{
    RecordProperty("ParentRequirement", "SCR-21792394");
    RecordProperty("Description",
                   "Stops the asynchronous search for available services.After the call has returned no further calls "
                   "to the user provided FindServiceHandler takes place.");
    RecordProperty("TestingTechnique", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    std::promise<void> handler_destruction_barrier{};
    bool handler_called{false};

    // Given a DestructorNotifier object which will set the value of the handler_destruction_barrier promise on
    // destruction
    DestructorNotifier destructor_notifier{&handler_destruction_barrier};

    // and a ServiceDiscoveryClient with a currently active StartFindService call for a specific instance
    FindServiceHandle expected_handle{make_FindServiceHandle(1U)};
    WhichContainsAServiceDiscoveryClient().WithAnActiveStartFindService(
        kConfigStoreQm1.GetInstanceIdentifier(),
        expected_handle,
        [&handler_called, destructor_notifier = std::move(destructor_notifier)](auto, auto) noexcept {
            handler_called = true;
        });

    // When calling StopFindService before calling OfferService
    EXPECT_TRUE(service_discovery_client_->StopFindService(expected_handle).has_value());
    EXPECT_TRUE(service_discovery_client_->OfferService(kConfigStoreQm1.GetInstanceIdentifier()).has_value());

    // Then the handler passed to StartFindService should never be called (we stop waiting once the handler is
    // destroyed, indicated by the destructor of DestructorNotifier).
    handler_destruction_barrier.get_future().wait();
    EXPECT_FALSE(handler_called);
}

TEST_F(ServiceDiscoveryClientStopFindServiceFixture, DoesNotCallHandlerIfFindServiceIsStoppedAnyInstanceIds)
{
    std::promise<void> handler_destruction_barrier{};
    bool handler_called{false};

    // Given a DestructorNotifier object which will set the value of the handler_destruction_barrier promise on
    // destruction
    DestructorNotifier destructor_notifier{&handler_destruction_barrier};

    // and a ServiceDiscoveryClient with a currently active find any StartFindService call
    FindServiceHandle expected_handle{make_FindServiceHandle(1U)};
    WhichContainsAServiceDiscoveryClient().WithAnActiveStartFindService(
        kConfigStoreFindAny.GetInstanceIdentifier(),
        expected_handle,
        [&handler_called, destructor_notifier = std::move(destructor_notifier)](auto, auto) noexcept {
            handler_called = true;
        });

    // and calling StopFindService before calling OfferService
    EXPECT_TRUE(service_discovery_client_->StopFindService(expected_handle).has_value());
    EXPECT_TRUE(service_discovery_client_->OfferService(kConfigStoreQm1.GetInstanceIdentifier()).has_value());

    // Then the handler passed to StartFindService should never be called (we stop waiting once the handler is
    // destroyed, indicated by the destructor of DestructorNotifier).
    handler_destruction_barrier.get_future().wait();
    EXPECT_FALSE(handler_called);
}

TEST_F(ServiceDiscoveryClientStopFindServiceFixture, CanCallStopFindServiceInsideHandler)
{
    InSequence in_sequence{};

    FindServiceHandle expected_handle{make_FindServiceHandle(1U)};

    StrictMock<MockFunction<void(ServiceHandleContainer<HandleType>, FindServiceHandle)>> find_service_handler{};
    std::promise<void> handler_destroyed{};
    DestructorNotifier destructor_notifier{&handler_destroyed};

    WhichContainsAServiceDiscoveryClient();

    // Expecting that the find service handler is called when the first service is offered.
    // and that StopFindService is called within that handler
    EXPECT_CALL(find_service_handler, Call(_, expected_handle))
        .WillOnce(Invoke([this](auto container, auto find_service_handle) {
            EXPECT_EQ(container.size(), 1);
            EXPECT_EQ(container[0], kConfigStoreFindAny.GetHandle(kConfigStoreQm1.lola_instance_id_.value()));
            const auto result = service_discovery_client_->StopFindService(find_service_handle);
            ASSERT_TRUE(result.has_value());

            ASSERT_TRUE(service_discovery_client_->OfferService(kConfigStoreQm2.GetInstanceIdentifier()).has_value());
        }));

    // Then the find service handler will not be called again when the second service is offered.
    EXPECT_CALL(find_service_handler, Call(_, expected_handle)).Times(0);

    // When calling StartFindService with a Find Any search
    const auto result = service_discovery_client_->StartFindService(
        expected_handle,
        [&find_service_handler, notifier = std::move(destructor_notifier)](
            ServiceHandleContainer<HandleType> containers, FindServiceHandle handle) noexcept {
            find_service_handler.AsStdFunction()(containers, handle);
        },
        EnrichedInstanceIdentifier{kConfigStoreFindAny.GetInstanceIdentifier()});
    ASSERT_TRUE(result.has_value());

    // and OfferService is called offering the first instance
    ASSERT_TRUE(service_discovery_client_->OfferService(kConfigStoreQm1.GetInstanceIdentifier()).has_value());

    // Unblock the worker thread to actually remove the search
    CreateRegularFile(
        filesystem_,
        GetFlagFilePrefix(kServiceId, kConfigStoreQm1.lola_instance_id_.value(), GetServiceDiscoveryPath()));

    // Wait for the handler to be destructed since after that we can be sure that it is no longer called.
    handler_destroyed.get_future().wait();
}

TEST_F(ServiceDiscoveryClientStopFindServiceFixture, StopFindServiceBlocksUntilHandlerFinishedWhenCalledOutsideHandler)
{
    InSequence in_sequence{};

    std::promise<void> service_found_barrier{};
    std::promise<void> search_stopped_barrier{};
    FindServiceHandle expected_handle{make_FindServiceHandle(1U)};

    StrictMock<MockFunction<void(ServiceHandleContainer<HandleType>, FindServiceHandle)>> find_service_handler{};

    WhichContainsAServiceDiscoveryClient();

    // Expecting that the find service handler is called when the first service is offered.
    // and that StopFindService is called within that handler
    EXPECT_CALL(find_service_handler, Call(_, expected_handle))
        .WillOnce(Invoke([&service_found_barrier, &search_stopped_barrier](auto, auto) {
            service_found_barrier.set_value();
            // Give some chance for missing synchronization to become obvious
            const auto future_status = search_stopped_barrier.get_future().wait_for(std::chrono::milliseconds{5});
            ASSERT_EQ(future_status, std::future_status::timeout) << "StopFindService did not wait as promised";
        }));

    // When calling StartFindService with a Find Any search
    const auto start_find_service_result = service_discovery_client_->StartFindService(
        expected_handle,
        CreateWrappedMockFindServiceHandler(find_service_handler),
        EnrichedInstanceIdentifier{kConfigStoreQm1.GetInstanceIdentifier()});
    ASSERT_TRUE(start_find_service_result.has_value());

    // and OfferService is called offering the first instance
    ASSERT_TRUE(service_discovery_client_->OfferService(kConfigStoreQm1.GetInstanceIdentifier()).has_value());
    service_found_barrier.get_future().wait();

    // StopFindService blocks until the ongoing invocation is finished
    const auto stop_find_service_result = service_discovery_client_->StopFindService(expected_handle);
    ASSERT_TRUE(stop_find_service_result.has_value());
    search_stopped_barrier.set_value();
}

TEST_F(ServiceDiscoveryClientStopFindServiceFixture, CallingStopFindServiceWithInvalidHandleDoesNotReturnError)
{
    // Given a ServiceDiscoveryClient with no active find service handler
    WhichContainsAServiceDiscoveryClient();

    // When calling StopFindService on a handle which does not correspond to an active find service handler
    const FindServiceHandle handle{make_FindServiceHandle(1U)};
    const auto stop_find_service_result = service_discovery_client_->StopFindService(handle);

    // Then the result will be valid
    EXPECT_TRUE(stop_find_service_result.has_value());
}

TEST_F(ServiceDiscoveryClientStopFindServiceFixture, CallingStopFindServiceWithInvalidHandleDoesNotRemovesAnyWatches)
{
    // Expecting that RemoveWatch is never called
    EXPECT_CALL(inotify_instance_mock_, RemoveWatch(_)).Times(0);

    // Given a ServiceDiscoveryClient with no active find service handler
    WhichContainsAServiceDiscoveryClient();

    // When calling StopFindService on a handle which does not correspond to an active find service handler
    const FindServiceHandle handle{make_FindServiceHandle(1U)};
    score::cpp::ignore = service_discovery_client_->StopFindService(handle);

    // Give time for the worker thread to process the StopFindService request
    std::this_thread::sleep_for(std::chrono::milliseconds{500U});
}

TEST_F(ServiceDiscoveryClientStopFindServiceFixture,
       CallingStopFindServiceWithStillActiveStartFindServiceCallsWillNotRemoveWatch)
{
    // Expecting that RemoveWatch is never called
    EXPECT_CALL(inotify_instance_mock_, RemoveWatch(_)).Times(0);

    // Given a ServiceDiscoveryClient with two active find service calls for the same instance
    FindServiceHandle handle_1{make_FindServiceHandle(1U)};
    FindServiceHandle handle_2{make_FindServiceHandle(2U)};
    WhichContainsAServiceDiscoveryClient()
        .WithAnActiveStartFindService(kConfigStoreQm1.GetInstanceIdentifier(), handle_1)
        .WithAnActiveStartFindService(kConfigStoreQm1.GetInstanceIdentifier(), handle_2);

    // When calling StopFindService with a handle corresponding to only one of the active start find service calls
    score::cpp::ignore = service_discovery_client_->StopFindService(handle_1);
}

}  // namespace
}  // namespace score::mw::com::impl::lola::test
