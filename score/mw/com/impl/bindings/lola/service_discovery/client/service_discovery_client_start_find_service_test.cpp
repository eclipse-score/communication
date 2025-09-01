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
#include "score/os/errno.h"
#include "score/result/result.h"
#include "score/mw/com/impl/bindings/lola/service_discovery/client/service_discovery_client.h"

#include "score/mw/com/impl/bindings/lola/service_discovery/test/service_discovery_client_test_fixtures.h"
#include "score/mw/com/impl/bindings/lola/service_discovery/test/service_discovery_client_test_resources.h"
#include "score/mw/com/impl/configuration/lola_service_id.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"
#include "score/mw/com/impl/enriched_instance_identifier.h"
#include "score/mw/com/impl/find_service_handle.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/i_service_discovery.h"

#include <score/expected.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <unistd.h>
#include <atomic>
#include <cstdint>
#include <string>

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
ConfigurationStore kConfigStoreAsilB{
    kInstanceSpecifierString,
    make_ServiceIdentifierType("foo"),
    QualityType::kASIL_B,
    kServiceId,
    LolaServiceInstanceId{3U},
};
ConfigurationStore kConfigStoreFindAny{kInstanceSpecifierString,
                                       make_ServiceIdentifierType("foo"),
                                       QualityType::kASIL_QM,
                                       kServiceId,
                                       score::cpp::optional<LolaServiceInstanceId>{}};

HandleType kHandleFindAnyQm1{kConfigStoreFindAny.GetHandle(kConfigStoreQm1.lola_instance_id_.value())};
HandleType kHandleFindAnyQm2{kConfigStoreFindAny.GetHandle(kConfigStoreQm2.lola_instance_id_.value())};
HandleType kHandleFindAnyAsilB{kConfigStoreFindAny.GetHandle(kConfigStoreAsilB.lola_instance_id_.value())};

using ServiceDiscoveryClientStartFindServiceFixture = ServiceDiscoveryClientFixture;
TEST_F(ServiceDiscoveryClientStartFindServiceFixture, CallingStartFindServiceReturnsValidResult)
{
    // Given a ServiceDiscoveryClient
    WhichContainsAServiceDiscoveryClient();

    // When calling StartFindService with an InstanceIdentifier with a specified instance ID
    FindServiceHandle handle{make_FindServiceHandle(1U)};
    const auto start_find_service_result = service_discovery_client_->StartFindService(
        handle, [](auto, auto) noexcept {}, EnrichedInstanceIdentifier{kConfigStoreQm1.GetInstanceIdentifier()});

    // Then the result is valid
    EXPECT_TRUE(start_find_service_result.has_value());
}

TEST_F(ServiceDiscoveryClientStartFindServiceFixture, CallingStartFindServiceForAnyInstanceIdsReturnsValidResult)
{
    // Given a ServiceDiscoveryClient
    WhichContainsAServiceDiscoveryClient();

    // When calling StartFindService with an InstanceIdentifier without a specified instance ID
    FindServiceHandle handle{make_FindServiceHandle(1U)};
    const auto start_find_service_result = service_discovery_client_->StartFindService(
        handle, [](auto, auto) noexcept {}, EnrichedInstanceIdentifier{kConfigStoreFindAny.GetInstanceIdentifier()});

    // Then the result is valid
    EXPECT_TRUE(start_find_service_result.has_value());
}

using ServiceDiscoveryClientStartFindServiceDeathTest = ServiceDiscoveryClientStartFindServiceFixture;
TEST_F(ServiceDiscoveryClientStartFindServiceDeathTest, CallingStartFindServiceWithInvalidQualityTypeTerminates)
{
    const ConfigurationStore config_store_invalid_quality_type{
        kInstanceSpecifierString,
        make_ServiceIdentifierType("foo"),
        QualityType::kInvalid,
        kServiceId,
        LolaServiceInstanceId{1U},
    };

    // Since CreateAServiceDiscoveryClient() spawns a thread, it should be called within the EXPECT_DEATH context.
    auto test_function = [this, &config_store_invalid_quality_type] {
        // Given a ServiceDiscoveryClient
        WhichContainsAServiceDiscoveryClient();

        // When calling StartFindService with an InstanceIdentifier with an invalid quality type
        // Then the program terminates
        const FindServiceHandle handle{make_FindServiceHandle(1U)};
        score::cpp::ignore = service_discovery_client_->StartFindService(
            handle,
            [](auto, auto) noexcept {},
            EnrichedInstanceIdentifier{config_store_invalid_quality_type.GetInstanceIdentifier()});
    };
    EXPECT_DEATH(test_function(), ".*");
}

TEST_F(ServiceDiscoveryClientStartFindServiceDeathTest, CallingStartFindServiceWithUnknownQualityTypeTerminates)
{
    const auto unknown_quality_type = static_cast<QualityType>(100U);
    const ConfigurationStore config_store_unknown_quality_type{
        kInstanceSpecifierString,
        make_ServiceIdentifierType("foo"),
        unknown_quality_type,
        kServiceId,
        LolaServiceInstanceId{1U},
    };

    // Since CreateAServiceDiscoveryClient() spawns a thread, it should be called within the EXPECT_DEATH context.
    auto test_function = [this, &config_store_unknown_quality_type] {
        // Given a ServiceDiscoveryClient
        WhichContainsAServiceDiscoveryClient();

        // When calling StartFindService with an InstanceIdentifier with an unknown quality type
        // Then the program terminates
        const FindServiceHandle handle{make_FindServiceHandle(1U)};
        score::cpp::ignore = service_discovery_client_->StartFindService(
            handle,
            [](auto, auto) noexcept {},
            EnrichedInstanceIdentifier{config_store_unknown_quality_type.GetInstanceIdentifier()});
    };
    EXPECT_DEATH(test_function(), ".*");
}

TEST_F(ServiceDiscoveryClientStartFindServiceFixture, CallingStartFindServiceAddsWatchToInstancePath)
{
    RecordProperty("Verifies", "SCR-32386265");
    RecordProperty(
        "Description",
        "Checks that calling StartFindService with an InstanceIdentifier with a specified instance ID, then a watch "
        "will be added to the instance path (i.e. the search path that includes the service ID and instance ID).");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a ServiceDiscoveryClient which saves the generated flag file path
    WhichContainsAServiceDiscoveryClient();

    // Expecting that a watch is added to the instance path
    const auto expected_instance_directory_path =
        GenerateExpectedInstanceDirectoryPath(kServiceId, kConfigStoreQm1.lola_instance_id_->GetId()).Native();
    EXPECT_CALL(inotify_instance_mock_, AddWatch(expected_instance_directory_path, _));

    // When calling StartFindService with an InstanceIdentifier with a specified instance ID
    FindServiceHandle handle{make_FindServiceHandle(1U)};
    score::cpp::ignore = service_discovery_client_->StartFindService(
        handle, [](auto, auto) noexcept {}, EnrichedInstanceIdentifier{kConfigStoreQm1.GetInstanceIdentifier()});
}

TEST_F(ServiceDiscoveryClientStartFindServiceFixture,
       FailingToAddWatchToInstancePathWhileCallingStartFindServiceReturnsError)
{
    // Given a ServiceDiscoveryClient which saves the generated flag file path
    WhichContainsAServiceDiscoveryClient();

    // Expecting that a attempting to add a watch to the instance path returns an error
    const auto expected_instance_directory_path =
        GenerateExpectedInstanceDirectoryPath(kServiceId, kConfigStoreQm1.lola_instance_id_->GetId()).Native();
    ON_CALL(inotify_instance_mock_, AddWatch(expected_instance_directory_path, _))
        .WillByDefault(Return(score::cpp::make_unexpected(::score::os::Error::createFromErrno(EACCES))));

    // When calling StartFindService with an InstanceIdentifier with a specified instance ID
    const FindServiceHandle handle{make_FindServiceHandle(1U)};
    const auto start_find_service_result = service_discovery_client_->StartFindService(
        handle, [](auto, auto) noexcept {}, EnrichedInstanceIdentifier{kConfigStoreQm1.GetInstanceIdentifier()});

    // Then StartFindService returns an error
    ASSERT_FALSE(start_find_service_result.has_value());
}

TEST_F(ServiceDiscoveryClientStartFindServiceFixture, CallingStartFindServiceForAnyInstanceIdsAddsWatchToServicePath)
{
    RecordProperty("Verifies", "SCR-32386265");
    RecordProperty(
        "Description",
        "Checks that calling StartFindService with an InstanceIdentifier without a specified instance ID, then a watch "
        "will be added to the servce path (i.e. the search path that includes the service ID).");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a ServiceDiscoveryClient which saves the generated flag file path
    WhichContainsAServiceDiscoveryClient();

    // Expecting that a watch is added to the service path
    const auto expected_service_directory_path = GenerateExpectedServiceDirectoryPath(kServiceId).Native();
    EXPECT_CALL(inotify_instance_mock_, AddWatch(expected_service_directory_path, _));

    // When calling StartFindService with an InstanceIdentifier without a specified instance ID
    const FindServiceHandle handle{make_FindServiceHandle(1U)};
    score::cpp::ignore = service_discovery_client_->StartFindService(
        handle, [](auto, auto) noexcept {}, EnrichedInstanceIdentifier{kConfigStoreFindAny.GetInstanceIdentifier()});
}

TEST_F(ServiceDiscoveryClientStartFindServiceFixture,
       FailingToAddWatchToServicePathWhileCallingStartFindServiceReturnsError)
{
    // Given a ServiceDiscoveryClient which saves the generated flag file path
    WhichContainsAServiceDiscoveryClient();

    // Expecting that a attempting to add a watch to the service path returns an error
    const auto expected_service_directory_path = GenerateExpectedServiceDirectoryPath(kServiceId).Native();
    ON_CALL(inotify_instance_mock_, AddWatch(expected_service_directory_path, _))
        .WillByDefault(Return(score::cpp::make_unexpected(::score::os::Error::createFromErrno(EACCES))));

    // When calling StartFindService with an InstanceIdentifier with a specified instance ID
    const FindServiceHandle handle{make_FindServiceHandle(1U)};
    const auto start_find_service_result = service_discovery_client_->StartFindService(
        handle, [](auto, auto) noexcept {}, EnrichedInstanceIdentifier{kConfigStoreFindAny.GetInstanceIdentifier()});

    // Then StartFindService returns an error
    ASSERT_FALSE(start_find_service_result.has_value());
}

TEST_F(ServiceDiscoveryClientStartFindServiceFixture,
       CallingStartFindServiceTwiceWithTheSameIdentifierReturnsValidResult)
{
    // Given a ServiceDiscoveryClient with a currently active StartFindService
    const FindServiceHandle handle{make_FindServiceHandle(1U)};
    WhichContainsAServiceDiscoveryClient().WithAnActiveStartFindService(kConfigStoreQm1.GetInstanceIdentifier(),
                                                                        handle);

    // When calling StartFindService with the same InstanceIdentifier
    const FindServiceHandle handle2{make_FindServiceHandle(2U)};
    const auto second_start_find_service_result = service_discovery_client_->StartFindService(
        handle2, [](auto, auto) noexcept {}, EnrichedInstanceIdentifier{kConfigStoreQm1.GetInstanceIdentifier()});

    // Then the result is valid
    EXPECT_TRUE(second_start_find_service_result.has_value());
}

TEST_F(ServiceDiscoveryClientStartFindServiceFixture,
       CallingStartFindServiceTwiceWithTheSameIdentifierDoesNotAddAnotherWatch)
{
    // Expecting that only one watch will be added to the instance path when the first StartFindService is called
    EXPECT_CALL(inotify_instance_mock_, AddWatch(_, _)).Times(1);

    // Given a ServiceDiscoveryClient with a currently active StartFindService
    const FindServiceHandle handle{make_FindServiceHandle(1U)};
    WhichContainsAServiceDiscoveryClient().WithAnActiveStartFindService(kConfigStoreQm1.GetInstanceIdentifier(),
                                                                        handle);

    // When calling StartFindService with the same InstanceIdentifier
    const FindServiceHandle handle2{make_FindServiceHandle(2U)};
    score::cpp::ignore = service_discovery_client_->StartFindService(
        handle2, [](auto, auto) noexcept {}, EnrichedInstanceIdentifier{kConfigStoreQm1.GetInstanceIdentifier()});
}

TEST_F(ServiceDiscoveryClientStartFindServiceFixture,
       CallingStartFindServiceTwiceWithTheSameIdentifierCallsBothHandlersWhenServiceIsOffered)
{
    std::promise<void> service_found_barrier_1{};
    std::promise<void> service_found_barrier_2{};

    MockFunction<void(ServiceHandleContainer<HandleType>, FindServiceHandle)> find_service_handler_1{};
    MockFunction<void(ServiceHandleContainer<HandleType>, FindServiceHandle)> find_service_handler_2{};

    // Expecting that both find service handlers are called
    EXPECT_CALL(find_service_handler_1, Call(_, _)).WillOnce(InvokeWithoutArgs([&service_found_barrier_1] {
        service_found_barrier_1.set_value();
    }));
    EXPECT_CALL(find_service_handler_2, Call(_, _)).WillOnce(InvokeWithoutArgs([&service_found_barrier_2] {
        service_found_barrier_2.set_value();
    }));

    // Given a ServiceDiscoveryClient with a currently active StartFindService
    const FindServiceHandle handle{make_FindServiceHandle(1U)};
    WhichContainsAServiceDiscoveryClient().WithAnActiveStartFindService(
        kConfigStoreQm1.GetInstanceIdentifier(), handle, CreateWrappedMockFindServiceHandler(find_service_handler_1));

    // When calling StartFindService with the same InstanceIdentifier
    const FindServiceHandle handle2{make_FindServiceHandle(2U)};
    score::cpp::ignore = service_discovery_client_->StartFindService(
        handle2,
        CreateWrappedMockFindServiceHandler(find_service_handler_2),
        EnrichedInstanceIdentifier{kConfigStoreQm1.GetInstanceIdentifier()});

    // and when the service is offered
    EXPECT_TRUE(service_discovery_client_->OfferService(kConfigStoreQm1.GetInstanceIdentifier()).has_value());

    // Then both handlers are called
    service_found_barrier_1.get_future().wait();
    service_found_barrier_2.get_future().wait();
}

TEST_F(ServiceDiscoveryClientStartFindServiceFixture,
       CallingStartFindServiceOnOfferedServiceTwiceWithTheSameIdentifierCallsBothHandlers)
{
    std::promise<void> service_found_barrier_1{};
    std::promise<void> service_found_barrier_2{};

    MockFunction<void(ServiceHandleContainer<HandleType>, FindServiceHandle)> find_service_handler_1{};
    MockFunction<void(ServiceHandleContainer<HandleType>, FindServiceHandle)> find_service_handler_2{};

    // Expecting that both find service handlers are called
    EXPECT_CALL(find_service_handler_1, Call(_, _)).WillOnce(InvokeWithoutArgs([&service_found_barrier_1] {
        service_found_barrier_1.set_value();
    }));
    EXPECT_CALL(find_service_handler_2, Call(_, _)).WillOnce(InvokeWithoutArgs([&service_found_barrier_2] {
        service_found_barrier_2.set_value();
    }));

    // Given a ServiceDiscoveryClient with an offered service and a currently active StartFindService
    const auto& instance_identifier = kConfigStoreQm1.GetInstanceIdentifier();
    const FindServiceHandle handle{make_FindServiceHandle(1U)};
    WhichContainsAServiceDiscoveryClient()
        .WithAnOfferedService(instance_identifier)
        .WithAnActiveStartFindService(
            instance_identifier, handle, CreateWrappedMockFindServiceHandler(find_service_handler_1));

    // When calling StartFindService with the same InstanceIdentifier
    const FindServiceHandle handle2{make_FindServiceHandle(2U)};
    score::cpp::ignore =
        service_discovery_client_->StartFindService(handle2,
                                                    CreateWrappedMockFindServiceHandler(find_service_handler_2),
                                                    EnrichedInstanceIdentifier{instance_identifier});

    // Then both handlers are called
    service_found_barrier_1.get_future().wait();
    service_found_barrier_2.get_future().wait();
}

TEST_F(ServiceDiscoveryClientStartFindServiceFixture, CallsHandlerIfServiceInstanceAppearedBeforeSearchStarted)
{
    std::promise<void> barrier{};

    const auto& instance_identifier = kConfigStoreQm1.GetInstanceIdentifier();
    WhichContainsAServiceDiscoveryClient().WithAnOfferedService(instance_identifier);

    FindServiceHandle expected_handle{make_FindServiceHandle(1U)};
    const auto start_find_service_result = service_discovery_client_->StartFindService(
        expected_handle,
        [&barrier, &expected_handle](auto container, auto handle) noexcept {
            EXPECT_EQ(container.size(), 1);
            EXPECT_EQ(container.front(), kConfigStoreQm1.GetHandle());
            EXPECT_EQ(handle, expected_handle);
            barrier.set_value();
        },
        EnrichedInstanceIdentifier{instance_identifier});
    EXPECT_TRUE(start_find_service_result.has_value());
    barrier.get_future().wait();
}

TEST_F(ServiceDiscoveryClientStartFindServiceFixture, CallsHandlerIfServiceInstanceAppearsAfterSearchStarted)
{
    std::promise<void> service_found_barrier{};

    WhichContainsAServiceDiscoveryClient();

    FindServiceHandle expected_handle{make_FindServiceHandle(1U)};
    const auto start_find_service_result = service_discovery_client_->StartFindService(
        expected_handle,
        [&service_found_barrier, &expected_handle](auto container, auto handle) noexcept {
            EXPECT_EQ(container.size(), 1);
            EXPECT_EQ(container.front(), kConfigStoreQm1.GetHandle());
            EXPECT_EQ(handle, expected_handle);
            service_found_barrier.set_value();
        },
        EnrichedInstanceIdentifier{kConfigStoreQm1.GetInstanceIdentifier()});
    EXPECT_TRUE(start_find_service_result.has_value());

    EXPECT_TRUE(service_discovery_client_->OfferService(kConfigStoreQm1.GetInstanceIdentifier()).has_value());
    service_found_barrier.get_future().wait();
}

TEST_F(ServiceDiscoveryClientStartFindServiceFixture, CallsCorrectHandlerForDifferentInstanceIDs)
{
    RecordProperty("Verifies", "SCR-22128594");
    RecordProperty(
        "Description",
        "Checks that a service is only visible to consumers (i.e. to StartFindService) after OfferService is called.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    std::promise<void> service_found_barrier_1{};
    std::promise<void> service_found_barrier_2{};

    std::atomic<bool> handler_received_1{false};
    std::atomic<bool> handler_received_2{false};

    WhichContainsAServiceDiscoveryClient();

    FindServiceHandle expected_handle_1{make_FindServiceHandle(1U)};
    const auto start_find_service_result = service_discovery_client_->StartFindService(
        expected_handle_1,
        [&service_found_barrier_1, &expected_handle_1, &handler_received_1](auto container, auto handle) noexcept {
            EXPECT_EQ(container.size(), 1);
            EXPECT_EQ(container.front(), kConfigStoreQm1.GetHandle());
            EXPECT_EQ(handle, expected_handle_1);
            handler_received_1.store(true);
            service_found_barrier_1.set_value();
        },
        EnrichedInstanceIdentifier{kConfigStoreQm1.GetInstanceIdentifier()});
    EXPECT_TRUE(start_find_service_result.has_value());

    FindServiceHandle expected_handle_2{make_FindServiceHandle(2U)};
    const auto start_find_service_result_2 = service_discovery_client_->StartFindService(
        expected_handle_2,
        [&service_found_barrier_2, &expected_handle_2, &handler_received_2](auto container, auto handle) noexcept {
            EXPECT_EQ(container.size(), 1);
            EXPECT_EQ(container.front(), kConfigStoreQm2.GetHandle());
            EXPECT_EQ(handle, expected_handle_2);
            handler_received_2.store(true);
            service_found_barrier_2.set_value();
        },
        EnrichedInstanceIdentifier{kConfigStoreQm2.GetInstanceIdentifier()});
    EXPECT_TRUE(start_find_service_result_2.has_value());

    EXPECT_FALSE(handler_received_1.load());
    EXPECT_FALSE(handler_received_2.load());
    EXPECT_TRUE(service_discovery_client_->OfferService(kConfigStoreQm1.GetInstanceIdentifier()).has_value());
    EXPECT_TRUE(service_discovery_client_->OfferService(kConfigStoreQm2.GetInstanceIdentifier()).has_value());

    service_found_barrier_1.get_future().wait();
    service_found_barrier_2.get_future().wait();

    EXPECT_TRUE(handler_received_1.load());
    EXPECT_TRUE(handler_received_2.load());
}

TEST_F(ServiceDiscoveryClientStartFindServiceFixture, HandlersAreNotCalledWhenServiceIsNotOffered)
{
    RecordProperty("Verifies", "SCR-32385851");
    RecordProperty("Description", "Checks that FindServiceHandlers are not called when a service is not offered.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    MockFunction<void(ServiceHandleContainer<HandleType>, FindServiceHandle)> find_service_handler_1{};
    MockFunction<void(ServiceHandleContainer<HandleType>, FindServiceHandle)> find_service_handler_2{};

    EXPECT_CALL(find_service_handler_1, Call(_, _)).Times(0);
    EXPECT_CALL(find_service_handler_2, Call(_, _)).Times(0);

    WhichContainsAServiceDiscoveryClient();

    score::cpp::ignore = service_discovery_client_->StartFindService(
        make_FindServiceHandle(1U),
        CreateWrappedMockFindServiceHandler(find_service_handler_1),
        EnrichedInstanceIdentifier{kConfigStoreQm1.GetInstanceIdentifier()});

    score::cpp::ignore = service_discovery_client_->StartFindService(
        make_FindServiceHandle(2U),
        CreateWrappedMockFindServiceHandler(find_service_handler_2),
        EnrichedInstanceIdentifier{kConfigStoreQm2.GetInstanceIdentifier()});
}

TEST_F(ServiceDiscoveryClientStartFindServiceFixture, HandlersAreCalledOnceWhenServiceIsOffered)
{
    RecordProperty("Verifies", "SCR-32385851");
    RecordProperty("Description", "Checks that FindServiceHandlers are called once when a service is offered.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    std::promise<void> service_found_barrier_1{};
    std::promise<void> service_found_barrier_2{};

    MockFunction<void(ServiceHandleContainer<HandleType>, FindServiceHandle)> find_service_handler_1{};
    MockFunction<void(ServiceHandleContainer<HandleType>, FindServiceHandle)> find_service_handler_2{};

    EXPECT_CALL(find_service_handler_1, Call(_, _)).WillOnce(InvokeWithoutArgs([&service_found_barrier_1]() {
        service_found_barrier_1.set_value();
    }));
    EXPECT_CALL(find_service_handler_2, Call(_, _)).WillOnce(InvokeWithoutArgs([&service_found_barrier_2]() {
        service_found_barrier_2.set_value();
    }));

    WhichContainsAServiceDiscoveryClient();

    score::cpp::ignore = service_discovery_client_->StartFindService(
        make_FindServiceHandle(1U),
        CreateWrappedMockFindServiceHandler(find_service_handler_1),
        EnrichedInstanceIdentifier{kConfigStoreQm1.GetInstanceIdentifier()});

    score::cpp::ignore = service_discovery_client_->StartFindService(
        make_FindServiceHandle(2U),
        CreateWrappedMockFindServiceHandler(find_service_handler_2),
        EnrichedInstanceIdentifier{kConfigStoreQm2.GetInstanceIdentifier()});

    score::cpp::ignore = service_discovery_client_->OfferService(kConfigStoreQm1.GetInstanceIdentifier());
    score::cpp::ignore = service_discovery_client_->OfferService(kConfigStoreQm2.GetInstanceIdentifier());

    service_found_barrier_1.get_future().wait();
    service_found_barrier_2.get_future().wait();
}

TEST_F(ServiceDiscoveryClientStartFindServiceFixture, CallsCorrectHandlerForAnyInstanceIDs)
{
    InSequence in_sequence{};

    std::promise<void> service_found_barrier_1{};
    std::promise<void> service_found_barrier_2{};
    FindServiceHandle expected_handle{make_FindServiceHandle(1U)};

    StrictMock<MockFunction<void(ServiceHandleContainer<HandleType>, FindServiceHandle)>> find_service_handler{};

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
}

TEST_F(ServiceDiscoveryClientStartFindServiceFixture, CorrectlyAssociatesOffersBasedOnQuality)
{
    std::promise<void> service_found_barrier_1{};
    std::promise<void> service_found_barrier_2{};
    FindServiceHandle expected_handle_1{make_FindServiceHandle(1U)};
    FindServiceHandle expected_handle_2{make_FindServiceHandle(2U)};

    StrictMock<MockFunction<void(ServiceHandleContainer<HandleType>, FindServiceHandle)>> find_service_handler_1{};

    EXPECT_CALL(find_service_handler_1, Call(_, expected_handle_1))
        .Times(::testing::Between(1, 2))
        .WillRepeatedly(WithArg<0>(Invoke([&service_found_barrier_1](auto container) {
            const std::unordered_set<HandleType> handles{container.begin(), container.end()};
            if (handles.find(kHandleFindAnyQm1) != handles.cend() &&
                handles.find(kHandleFindAnyAsilB) != handles.cend())
            {
                service_found_barrier_1.set_value();
            }
        })));

    StrictMock<MockFunction<void(ServiceHandleContainer<HandleType>, FindServiceHandle)>> find_service_handler_2{};
    EXPECT_CALL(find_service_handler_2, Call(_, expected_handle_2))
        .WillOnce(WithArg<0>(Invoke([&service_found_barrier_2](auto container) {
            EXPECT_EQ(container.size(), 1);
            EXPECT_EQ(container[0], kConfigStoreAsilB.GetHandle());
            service_found_barrier_2.set_value();
        })));

    WhichContainsAServiceDiscoveryClient();

    const auto start_find_service_result_1 = service_discovery_client_->StartFindService(
        expected_handle_1,
        CreateWrappedMockFindServiceHandler(find_service_handler_1),
        EnrichedInstanceIdentifier{kConfigStoreFindAny.GetInstanceIdentifier()});
    EXPECT_TRUE(start_find_service_result_1.has_value());

    const auto start_find_service_result_2 = service_discovery_client_->StartFindService(
        expected_handle_2,
        CreateWrappedMockFindServiceHandler(find_service_handler_2),
        EnrichedInstanceIdentifier{kConfigStoreAsilB.GetInstanceIdentifier()});
    EXPECT_TRUE(start_find_service_result_2.has_value());

    EXPECT_TRUE(service_discovery_client_->OfferService(kConfigStoreQm1.GetInstanceIdentifier()).has_value());
    EXPECT_TRUE(service_discovery_client_->OfferService(kConfigStoreAsilB.GetInstanceIdentifier()).has_value());
    service_found_barrier_1.get_future().wait();
    service_found_barrier_2.get_future().wait();
}

TEST_F(ServiceDiscoveryClientStartFindServiceFixture, DoesNotCallHandlerIfServiceOfferIsStoppedBeforeSearchStarts)
{
    RecordProperty("Verifies", "SCR-22121224");
    RecordProperty("Description",
                   "Checks that a service is not visible to consumers (i.e. to StartFindService) after "
                   "StopOfferService is called.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    bool handler_called{false};

    WhichContainsAServiceDiscoveryClient();

    // When calling OfferService and then immediately StopOfferService
    EXPECT_TRUE(service_discovery_client_->OfferService(kConfigStoreQm1.GetInstanceIdentifier()).has_value());
    ASSERT_TRUE(
        service_discovery_client_
            ->StopOfferService(kConfigStoreQm1.GetInstanceIdentifier(), IServiceDiscovery::QualityTypeSelector::kBoth)
            .has_value());

    // When calling StartFindService (which calls the handler synchronously if the offer is already present)
    FindServiceHandle expected_handle{make_FindServiceHandle(1U)};
    const auto start_find_service_result = service_discovery_client_->StartFindService(
        expected_handle,
        [&handler_called](auto, auto) noexcept {
            handler_called = true;
        },
        EnrichedInstanceIdentifier{kConfigStoreQm1.GetInstanceIdentifier()});
    EXPECT_TRUE(start_find_service_result.has_value());

    // Then the handler should not be called
    EXPECT_FALSE(handler_called);
}

TEST_F(ServiceDiscoveryClientStartFindServiceFixture, CanCallStartFindServiceInsideHandler)
{
    InSequence in_sequence{};

    std::promise<void> service_found_barrier{};
    FindServiceHandle expected_handle_first_search{make_FindServiceHandle(1U)};
    FindServiceHandle expected_handle_second_search{make_FindServiceHandle(2U)};

    StrictMock<MockFunction<void(ServiceHandleContainer<HandleType>, FindServiceHandle)>> find_service_handler{};

    WhichContainsAServiceDiscoveryClient();

    // Expecting that the find service handler is called when the first service is offered.
    // and that StartFindService is called within that handler
    EXPECT_CALL(find_service_handler, Call(_, expected_handle_first_search))
        .WillOnce(Invoke([this, &expected_handle_second_search, &find_service_handler](auto, auto) {
            const auto result = service_discovery_client_->StartFindService(
                expected_handle_second_search,
                CreateWrappedMockFindServiceHandler(find_service_handler),
                EnrichedInstanceIdentifier{kConfigStoreQm2.GetInstanceIdentifier()});
            ASSERT_TRUE(result.has_value());
        }));

    EXPECT_CALL(find_service_handler, Call(_, expected_handle_second_search))
        .WillOnce(Invoke([&service_found_barrier](auto, auto) {
            service_found_barrier.set_value();
        }));

    // When calling StartFindService with a search
    const auto start_find_service_result = service_discovery_client_->StartFindService(
        expected_handle_first_search,
        CreateWrappedMockFindServiceHandler(find_service_handler),
        EnrichedInstanceIdentifier{kConfigStoreQm1.GetInstanceIdentifier()});
    ASSERT_TRUE(start_find_service_result.has_value());

    // and OfferService is called offering the first instance
    const auto result_1 = service_discovery_client_->OfferService(kConfigStoreQm1.GetInstanceIdentifier());
    ASSERT_TRUE(result_1.has_value());

    // and OfferService is called offering the second instance
    const auto result_2 = service_discovery_client_->OfferService(kConfigStoreQm2.GetInstanceIdentifier());
    ASSERT_TRUE(result_2.has_value());

    // both handlers are invoked and do not block
    service_found_barrier.get_future().wait();
}

TEST_F(ServiceDiscoveryClientStartFindServiceFixture, AddsWatchOnStartFindServiceWhileWorkerThreadIsBlockedOnRead)
{
    std::promise<void> first_barrier{};
    std::promise<void> second_barrier{};
    std::promise<void> third_barrier{};
    EXPECT_CALL(inotify_instance_mock_, Read())
        .WillOnce([&first_barrier, &second_barrier]() {
            first_barrier.set_value();
            second_barrier.get_future().wait();
            return score::cpp::static_vector<os::InotifyEvent, os::InotifyInstance::max_events>{};
        })
        .WillOnce([&third_barrier] {
            third_barrier.set_value();
            return score::cpp::static_vector<os::InotifyEvent, os::InotifyInstance::max_events>{};
        })
        .WillRepeatedly([]() {
            return score::cpp::static_vector<os::InotifyEvent, os::InotifyInstance::max_events>{};
        });

    EXPECT_CALL(inotify_instance_mock_, AddWatch(_, _)).WillOnce([&second_barrier](auto, auto) {
        second_barrier.set_value();
        return os::InotifyWatchDescriptor{1};
    });

    WhichContainsAServiceDiscoveryClient();

    first_barrier.get_future().wait();

    FindServiceHandle handle{make_FindServiceHandle(1U)};
    EXPECT_TRUE(
        service_discovery_client_
            ->StartFindService(
                handle, [](auto, auto) noexcept {}, EnrichedInstanceIdentifier{kConfigStoreQm1.GetInstanceIdentifier()})
            .has_value());
    third_barrier.get_future().wait();
}

TEST_F(ServiceDiscoveryClientStartFindServiceFixture,
       WorkerThreadDoesNotBailWhenNewSearchIsAlreadyAwareOfEventThatWasNotYetHandled)
{
    std::promise<void> barrier_worker_thread_blocked_in_read{};
    std::promise<void> barrier_worker_thread_called_read_second{};
    std::promise<void> barrier_new_search_added{};
    std::promise<void> barrier_handler_called{};

    const std::int32_t wd{2};
    const std::uint32_t mask{IN_CREATE | IN_ISDIR};
    const std::uint32_t cookie{0};
    const auto name{"1"};
    const auto event = os::MakeFakeEvent(wd, mask, cookie, name);

    EXPECT_CALL(inotify_instance_mock_, Read())
        .WillOnce([&barrier_worker_thread_blocked_in_read, &barrier_new_search_added, &event]() {
            barrier_worker_thread_blocked_in_read.set_value();
            barrier_new_search_added.get_future().wait();
            return score::cpp::static_vector<os::InotifyEvent, os::InotifyInstance::max_events>{event};
        })
        .WillOnce([&barrier_worker_thread_called_read_second]() {
            barrier_worker_thread_called_read_second.set_value();
            return score::cpp::static_vector<os::InotifyEvent, os::InotifyInstance::max_events>{};
        })
        .WillRepeatedly([]() {
            return score::cpp::static_vector<os::InotifyEvent, os::InotifyInstance::max_events>{};
        });

    const auto service_path = GenerateExpectedServiceDirectoryPath(kServiceId).Native();
    EXPECT_CALL(inotify_instance_mock_, AddWatch(service_path, _)).WillOnce([](auto, auto) {
        return os::InotifyWatchDescriptor{2};
    });

    const auto instance_path =
        GenerateExpectedInstanceDirectoryPath(kServiceId, kConfigStoreQm1.lola_instance_id_->GetId()).Native();
    EXPECT_CALL(inotify_instance_mock_, AddWatch(instance_path, _)).Times(2).WillRepeatedly([](auto, auto) {
        return os::InotifyWatchDescriptor{3};
    });

    WhichContainsAServiceDiscoveryClient().WithAnOfferedService(kConfigStoreQm1.GetInstanceIdentifier());

    barrier_worker_thread_blocked_in_read.get_future().wait();

    FindServiceHandle handle{make_FindServiceHandle(1U)};
    EXPECT_TRUE(service_discovery_client_
                    ->StartFindService(
                        handle,
                        [&barrier_handler_called](auto, auto) noexcept {
                            barrier_handler_called.set_value();
                        },
                        EnrichedInstanceIdentifier{kConfigStoreFindAny.GetInstanceIdentifier()})
                    .has_value());

    barrier_new_search_added.set_value();
    barrier_handler_called.get_future().wait();
    barrier_worker_thread_called_read_second.get_future().wait();
}

}  // namespace
}  // namespace score::mw::com::impl::lola::test
