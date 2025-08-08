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

#include "score/mw/com/impl/bindings/lola/service_discovery/test/service_discovery_client_test_fixtures.h"
#include "score/mw/com/impl/bindings/lola/service_discovery/test/service_discovery_client_test_resources.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"
#include "score/mw/com/impl/instance_specifier.h"

#include "score/concurrency/long_running_threads_container.h"
#include "score/os/utils/inotify/inotify_instance_mock.h"
#include "score/os/utils/inotify/inotify_watch_descriptor.h"

#include <score/assert.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

namespace score::mw::com::impl::lola::test
{
namespace
{

using namespace ::testing;

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

score::cpp::static_vector<os::InotifyEvent, os::InotifyInstance::max_events> CreateEventVectorWithEventMasks(
    const std::vector<uint32_t>& event_masks,
    const std::vector<int>& watch_descriptors = {},
    const std::vector<std::string_view>& event_names = {})
{
    score::cpp::static_vector<os::InotifyEvent, os::InotifyInstance::max_events> event_vector{};

    if (!watch_descriptors.empty())
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT(watch_descriptors.size() == event_masks.size());
    }

    if (!event_names.empty())
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT(event_names.size() == event_masks.size());
    }

    for (std::size_t i = 0U; i < event_masks.size(); ++i)
    {
        const std::string_view dummy_name = event_names.empty() ? "dummy_name-asil-qm" : event_names[i];
        const std::uint32_t dummy_cookie = 2U;
        const std::int32_t wd = watch_descriptors.empty() ? 1 : watch_descriptors[i];
        event_vector.emplace_back(os::MakeFakeEvent(wd, event_masks[i], dummy_cookie, dummy_name));
    }
    return event_vector;
}

using ServiceDiscoveryClientWorkerThreadFixture = ServiceDiscoveryClientFixture;
TEST_F(ServiceDiscoveryClientWorkerThreadFixture, CanConstructFixture) {}

TEST(ServiceDiscoveryClientConstructorTest, CanConstructWithBasicConstructor)
{
    concurrency::LongRunningThreadsContainer long_running_threads_container{};

    // When constructing a ServiceDiscoveryClient with the basic constructor which takes care of creating inotify,
    // unistd and filesystem resources
    const ServiceDiscoveryClient service_discovery_client{long_running_threads_container};

    // Then we don't crash
    // And the ServiceDiscoveryClient can be destroyed without hanging
}

TEST_F(ServiceDiscoveryClientWorkerThreadFixture, StartsReadingInotifyInstanceOnConstruction)
{
    std::promise<void> barrier{};
    EXPECT_CALL(inotify_instance_mock_, Read())
        .WillOnce([&barrier]() {
            barrier.set_value();
            return score::cpp::static_vector<os::InotifyEvent, os::InotifyInstance::max_events>{};
        })
        .WillRepeatedly([]() {
            return score::cpp::static_vector<os::InotifyEvent, os::InotifyInstance::max_events>{};
        });

    WhichContainsAServiceDiscoveryClient();
    barrier.get_future().wait();
}

TEST_F(ServiceDiscoveryClientWorkerThreadFixture, ClosesInotifyInstanceOnDestructionToUnblockWorker)
{
    std::promise<void> barrier{};
    EXPECT_CALL(inotify_instance_mock_, Close());

    EXPECT_CALL(inotify_instance_mock_, Read())
        .WillOnce([&barrier, this]() {
            barrier.set_value();
            return inotify_instance_->Read();
        })
        .WillRepeatedly(DoDefault());

    WhichContainsAServiceDiscoveryClient();
    barrier.get_future().wait();
}

using ServiceDiscoveryClientWorkerThreadDeathTest = ServiceDiscoveryClientWorkerThreadFixture;
TEST_F(ServiceDiscoveryClientWorkerThreadDeathTest, BailsOutOnInotifyQueueOverflow)
{
    const auto event_vector = CreateEventVectorWithEventMasks({IN_Q_OVERFLOW});

    ON_CALL(inotify_instance_mock_, Read()).WillByDefault(Return(event_vector));

    // Since creating a ServiceDiscoveryClient spawns a thread, it should be called within the EXPECT_DEATH context.
    const auto test_function = [this] {
        auto inotify_instance_facade = std::make_unique<os::InotifyInstanceFacade>(inotify_instance_mock_);
        auto unistd = std::make_unique<os::internal::UnistdImpl>();
        const ServiceDiscoveryClient service_discovery_client{
            long_running_threads_container_, std::move(inotify_instance_facade), std::move(unistd), filesystem_};
        // We expect to die in an async thread - so a timeout is fine to violate the test if we do not die.
        std::this_thread::sleep_for(std::chrono::hours{1});
    };

    EXPECT_DEATH(test_function(), ".*");
}

TEST_F(ServiceDiscoveryClientWorkerThreadFixture,
       OperationWasInterruptedBySignalErrorsReturnedByInotifyReadWillNotTriggerTermination)
{
    std::promise<void> barrier{};

    // Expecting that INotify::Read() will be called which returns an os::Error::Code::kOperationWasInterruptedBySignal
    // error
    EXPECT_CALL(inotify_instance_mock_, Read())
        .WillOnce([]() {
            return score::cpp::make_unexpected(os::Error::createFromErrno(EINTR));
        })
        .WillOnce([&barrier]() {
            // Set the barrier value on the second call to Read to ensure that the worker thread fully processes the
            // previous events.
            barrier.set_value();
            return score::cpp::make_unexpected(os::Error::createFromErrno(EINTR));
        })
        .WillRepeatedly([]() {
            return score::cpp::make_unexpected(os::Error::createFromErrno(EINTR));
        });

    // Given a ServiceDiscoveryClient which spawns a worker thread
    WhichContainsAServiceDiscoveryClient();

    // When INotify::Read() is called which returns an error
    // Then we don't terminate;
    barrier.get_future().wait();
}

TEST_F(ServiceDiscoveryClientWorkerThreadFixture, GenericErrorsReturnedByInotifyReadWillNotTriggerTermination)
{
    std::promise<void> barrier{};

    // Expecting that INotify::Read() will be called which returns a generic error
    EXPECT_CALL(inotify_instance_mock_, Read())
        .WillOnce([]() {
            return score::cpp::make_unexpected(os::Error::createFromErrno(EPERM));
        })
        .WillOnce([&barrier]() {
            // Set the barrier value on the second call to Read to ensure that the worker thread fully processes the
            // previous events.
            barrier.set_value();
            return score::cpp::make_unexpected(os::Error::createFromErrno(EPERM));
        })
        .WillRepeatedly([]() {
            return score::cpp::make_unexpected(os::Error::createFromErrno(EPERM));
        });

    // Given a ServiceDiscoveryClient which spawns a worker thread
    WhichContainsAServiceDiscoveryClient();

    // When INotify::Read() is called which returns an error
    // Then we don't terminate;
    barrier.get_future().wait();
}

TEST_F(ServiceDiscoveryClientWorkerThreadFixture,
       AllUnexpectedInotifyEventsForUnknownWatchesReturnedByInotifyReadWillNotTriggerTermination)
{
    const uint32_t unknown_inotify_event_mask{0U};
    const auto vector_with_unknown_events =
        CreateEventVectorWithEventMasks({IN_ACCESS, IN_MOVED_TO, IN_CREATE, IN_ISDIR, unknown_inotify_event_mask});
    std::promise<void> barrier{};

    // Expecting that INotify::Read() will be called which returns a vector containing unexpected events
    EXPECT_CALL(inotify_instance_mock_, Read())
        .WillOnce(Return(vector_with_unknown_events))
        .WillOnce([&barrier] {
            // Set the barrier value on the second call to Read to ensure that the worker thread fully processes the
            // previous events.
            barrier.set_value();
            return score::cpp::static_vector<os::InotifyEvent, os::InotifyInstance::max_events>{};
        })
        .WillRepeatedly(Return(score::cpp::static_vector<os::InotifyEvent, os::InotifyInstance::max_events>{}));

    // Given a ServiceDiscoveryClient which spawns a worker thread
    WhichContainsAServiceDiscoveryClient();

    // When INotify::Read() is called which returns a vector containing unexpected events
    // Then we don't terminate;
    barrier.get_future().wait();
}

TEST_F(ServiceDiscoveryClientWorkerThreadFixture,
       AllUnexpectedInotifyEventsForKnownWatchesReturnedByInotifyReadWillNotTriggerTermination)
{
    const int watch_descriptor{10U};
    const uint32_t unknown_inotify_event_mask{0U};
    const auto vector_with_unknown_events_for_watch = CreateEventVectorWithEventMasks(
        {IN_ACCESS, IN_MOVED_TO, IN_CREATE, IN_ISDIR, unknown_inotify_event_mask},
        {watch_descriptor, watch_descriptor, watch_descriptor, watch_descriptor, watch_descriptor});
    std::promise<void> event_read_with_deletion_event_barrier{};
    std::promise<void> start_find_service_called_barrier{};
    auto start_find_service_called_future = start_find_service_called_barrier.get_future();

    // Expecting that a watch is added by StartFindService which returns a watch descriptor
    EXPECT_CALL(inotify_instance_mock_, AddWatch(_, _)).WillOnce(Return(os::InotifyWatchDescriptor{watch_descriptor}));

    // and that INotify::Read() will be called which returns a vector containing unexpected events associated with the
    // added watch.
    EXPECT_CALL(inotify_instance_mock_, Read())
        .WillOnce([&start_find_service_called_future, &vector_with_unknown_events_for_watch] {
            // Wait until StartFindService has been called to ensure that a watch has been added for the
            // watch_descriptor returned by AddWatch before INotify::Read() returns
            start_find_service_called_future.wait();

            return vector_with_unknown_events_for_watch;
        })
        .WillOnce([&event_read_with_deletion_event_barrier] {
            // This is the second call to read. It indicates that Read was already called once with
            // vector_with_unknown_events_for_watch so the test is finished and the main thread can exit.
            event_read_with_deletion_event_barrier.set_value();
            return score::cpp::static_vector<os::InotifyEvent, os::InotifyInstance::max_events>{};
        })
        .WillRepeatedly(Return(score::cpp::static_vector<os::InotifyEvent, os::InotifyInstance::max_events>{}));

    // Given a ServiceDiscoveryClient which spawns a worker thread and has an active StartFindService call
    const FindServiceHandle handle{make_FindServiceHandle(1U)};
    WhichContainsAServiceDiscoveryClient().WithAnActiveStartFindService(kConfigStoreQm1.GetInstanceIdentifier(),
                                                                        handle);

    start_find_service_called_barrier.set_value();

    // When INotify::Read() is called which returns a vector containing unexpected events associated with an active
    // watch
    // Then we don't terminate;
    event_read_with_deletion_event_barrier.get_future().wait();
}

TEST_F(ServiceDiscoveryClientWorkerThreadDeathTest, DeletingServiceSearchDirectoryCausesWorkerThreadToTerminate)
{
    const int watch_descriptor{10U};
    const auto vector_with_delete_event = CreateEventVectorWithEventMasks({IN_DELETE}, {watch_descriptor});
    std::promise<void> event_read_with_deletion_event_barrier{};
    std::promise<void> start_find_service_called_barrier{};

    const auto test_function =
        [this, &vector_with_delete_event, &event_read_with_deletion_event_barrier, &start_find_service_called_barrier] {
            auto start_find_service_called_future = start_find_service_called_barrier.get_future();

            // Expecting that a watch is added by StartFindService on the service directory which returns a watch
            // descriptor
            const auto expected_service_directory_path = GenerateExpectedServiceDirectoryPath(kServiceId).Native();
            EXPECT_CALL(inotify_instance_mock_, AddWatch(expected_service_directory_path, _))
                .WillOnce(Return(os::InotifyWatchDescriptor{watch_descriptor}));

            // Expecting that INotify::Read() will be called which returns a vector containing a delete event
            EXPECT_CALL(inotify_instance_mock_, Read())
                .WillOnce([&start_find_service_called_future, &vector_with_delete_event] {
                    // Wait until StartFindService has been called to ensure that a watch has been added for the
                    // watch_descriptor returned by AddWatch before INotify::Read() returns
                    start_find_service_called_future.wait();

                    return vector_with_delete_event;
                })
                .WillOnce([&event_read_with_deletion_event_barrier] {
                    // This is the second call to read. It indicates that Read was already called once with
                    // vector_with_delete_event so the test is finished and the main thread can exit.
                    event_read_with_deletion_event_barrier.set_value();
                    return score::cpp::static_vector<os::InotifyEvent, os::InotifyInstance::max_events>{};
                })
                .WillRepeatedly(Return(score::cpp::static_vector<os::InotifyEvent, os::InotifyInstance::max_events>{}));

            // Given a ServiceDiscoveryClient which spawns a worker thread and has an active find any StartFindService
            // call
            const FindServiceHandle handle{make_FindServiceHandle(1U)};
            WhichContainsAServiceDiscoveryClient().WithAnActiveStartFindService(
                kConfigStoreFindAny.GetInstanceIdentifier(), handle);

            start_find_service_called_barrier.set_value();

            // When INotify::Read() is called which returns a vector containing a deletion event for the service search
            // directory
            event_read_with_deletion_event_barrier.get_future().wait();
        };
    // Then we terminate;
    EXPECT_DEATH(test_function(), ".*");
}

TEST_F(ServiceDiscoveryClientWorkerThreadFixture, WorkerThreadIgnoresDeletionEventOfInstanceSearchDirectory)
{
    const int watch_descriptor{10U};
    const auto vector_with_ignore_event = CreateEventVectorWithEventMasks({IN_IGNORED}, {watch_descriptor});
    std::promise<void> event_read_with_deletion_event_barrier{};
    std::promise<void> start_find_service_called_barrier{};

    auto start_find_service_called_future = start_find_service_called_barrier.get_future();

    // Expecting that a watch is added by StartFindService on the instance directory which returns a watch
    // descriptor
    const auto expected_instance_directory_path =
        GenerateExpectedInstanceDirectoryPath(kServiceId, kConfigStoreQm1.lola_instance_id_.value().GetId()).Native();
    EXPECT_CALL(inotify_instance_mock_, AddWatch(expected_instance_directory_path, _))
        .WillOnce(Return(os::InotifyWatchDescriptor{watch_descriptor}));

    // Expecting that INotify::Read() will be called which returns a vector containing a delete event
    EXPECT_CALL(inotify_instance_mock_, Read())
        .WillOnce([&start_find_service_called_future, &vector_with_ignore_event] {
            // Wait until StartFindService has been called to ensure that a watch has been added for the
            // watch_descriptor returned by AddWatch before INotify::Read() returns
            start_find_service_called_future.wait();

            return vector_with_ignore_event;
        })
        .WillOnce([&event_read_with_deletion_event_barrier] {
            // This is the second call to read. It indicates that Read was already called once with
            // vector_with_ignore_event so the test is finished and the main thread can exit.
            event_read_with_deletion_event_barrier.set_value();
            return score::cpp::static_vector<os::InotifyEvent, os::InotifyInstance::max_events>{};
        })
        .WillRepeatedly(Return(score::cpp::static_vector<os::InotifyEvent, os::InotifyInstance::max_events>{}));

    // Given a ServiceDiscoveryClient which spawns a worker thread and has an active find any StartFindService
    // call
    const FindServiceHandle handle{make_FindServiceHandle(1U)};
    WhichContainsAServiceDiscoveryClient().WithAnActiveStartFindService(kConfigStoreQm1.GetInstanceIdentifier(),
                                                                        handle);

    start_find_service_called_barrier.set_value();

    // When INotify::Read() is called which returns a vector containing a deletion event for the service search
    // directory
    event_read_with_deletion_event_barrier.get_future().wait();

    // Then we don't crash
}

TEST_F(ServiceDiscoveryClientWorkerThreadFixture,
       WorkerThreadAddsWatchOnInstanceDirectoryWhenSearchPathInstanceIdCanBeDerivedFromDirectoryName)
{
    const int watch_descriptor{10U};
    LolaServiceInstanceId::InstanceId instance_id{1U};
    const auto valid_instance_directory_name = std::to_string(static_cast<unsigned int>(instance_id));
    const auto vector_with_creation_event =
        CreateEventVectorWithEventMasks({IN_CREATE}, {watch_descriptor}, {valid_instance_directory_name});
    std::promise<void> event_read_with_creation_event_barrier{};
    std::promise<void> start_find_service_called_barrier{};

    auto start_find_service_called_future = start_find_service_called_barrier.get_future();

    // Expecting that a watch is added by StartFindService on the service directory which returns a watch
    // descriptor
    const auto expected_service_directory_path = GenerateExpectedServiceDirectoryPath(kServiceId).Native();
    EXPECT_CALL(inotify_instance_mock_, AddWatch(expected_service_directory_path, _))
        .WillOnce(Return(os::InotifyWatchDescriptor{watch_descriptor}));

    // and that a watch is added on the instance directory when the inotify creation event is received
    const auto expected_instance_directory_path =
        GenerateExpectedInstanceDirectoryPath(kServiceId, instance_id).Native();
    EXPECT_CALL(inotify_instance_mock_, AddWatch(expected_instance_directory_path, _))
        .WillOnce(Return(os::InotifyWatchDescriptor{watch_descriptor}));

    // and that INotify::Read() will be called which returns a vector containing a creation event with a name from which
    // an instance ID cannot be parsed
    EXPECT_CALL(inotify_instance_mock_, Read())
        .WillOnce([&start_find_service_called_future, &vector_with_creation_event] {
            // Wait until StartFindService has been called to ensure that a watch has been added for the
            // watch_descriptor returned by AddWatch before INotify::Read() returns
            start_find_service_called_future.wait();

            return vector_with_creation_event;
        })
        .WillOnce([&event_read_with_creation_event_barrier] {
            // This is the second call to read. It indicates that Read was already called once with
            // vector_with_creation_event so the test is finished and the main thread can exit.
            event_read_with_creation_event_barrier.set_value();
            return score::cpp::static_vector<os::InotifyEvent, os::InotifyInstance::max_events>{};
        })
        .WillRepeatedly(Return(score::cpp::static_vector<os::InotifyEvent, os::InotifyInstance::max_events>{}));

    // Given a ServiceDiscoveryClient which spawns a worker thread and has an active find any StartFindService
    // call
    const FindServiceHandle handle{make_FindServiceHandle(1U)};
    WhichContainsAServiceDiscoveryClient().WithAnActiveStartFindService(kConfigStoreFindAny.GetInstanceIdentifier(),
                                                                        handle);

    start_find_service_called_barrier.set_value();

    // When INotify::Read() is called which returns a vector containing a creation event with a name from which
    // an instance ID can be parsed
    event_read_with_creation_event_barrier.get_future().wait();

    // Then we don't terminate;
}

TEST_F(ServiceDiscoveryClientWorkerThreadFixture,
       WorkerThreadIgnoresDirectoryCreationEventsIfInstanceIdCannotBeDerivedFromDirectoryName)
{
    const int watch_descriptor{10U};
    const auto invalid_instance_directory_name = "invalid_instance_directory_name";
    const auto vector_with_delete_event =
        CreateEventVectorWithEventMasks({IN_CREATE}, {watch_descriptor}, {invalid_instance_directory_name});
    std::promise<void> event_read_with_deletion_event_barrier{};
    std::promise<void> start_find_service_called_barrier{};

    auto start_find_service_called_future = start_find_service_called_barrier.get_future();

    // Expecting that a watch is NOT added on the instance directory when the inotify creation event is received (since
    // it has an invalid name)
    EXPECT_CALL(inotify_instance_mock_, AddWatch(_, _)).Times(0);

    // but that a watch is added by StartFindService on the service directory which returns a watch
    // descriptor
    const auto expected_service_directory_path = GenerateExpectedServiceDirectoryPath(kServiceId).Native();
    EXPECT_CALL(inotify_instance_mock_, AddWatch(expected_service_directory_path, _))
        .WillOnce(Return(os::InotifyWatchDescriptor{watch_descriptor}))
        .RetiresOnSaturation();

    // and that INotify::Read() will be called which returns a vector containing a creation event with a name from which
    // an instance ID cannot be parsed
    EXPECT_CALL(inotify_instance_mock_, Read())
        .WillOnce([&start_find_service_called_future, &vector_with_delete_event] {
            // Wait until StartFindService has been called to ensure that a watch has been added for the
            // watch_descriptor returned by AddWatch before INotify::Read() returns
            start_find_service_called_future.wait();

            return vector_with_delete_event;
        })
        .WillOnce([&event_read_with_deletion_event_barrier] {
            // This is the second call to read. It indicates that Read was already called once with
            // vector_with_delete_event so the test is finished and the main thread can exit.
            event_read_with_deletion_event_barrier.set_value();
            return score::cpp::static_vector<os::InotifyEvent, os::InotifyInstance::max_events>{};
        })
        .WillRepeatedly(Return(score::cpp::static_vector<os::InotifyEvent, os::InotifyInstance::max_events>{}));

    // Given a ServiceDiscoveryClient which spawns a worker thread and has an active find any StartFindService
    // call
    const FindServiceHandle handle{make_FindServiceHandle(1U)};
    WhichContainsAServiceDiscoveryClient().WithAnActiveStartFindService(kConfigStoreFindAny.GetInstanceIdentifier(),
                                                                        handle);

    start_find_service_called_barrier.set_value();

    // When INotify::Read() is called which returns a vector containing a creation event with a name from which
    // an instance ID cannot be parsed
    event_read_with_deletion_event_barrier.get_future().wait();
}

TEST_F(ServiceDiscoveryClientWorkerThreadFixture,
       ReceivingACreationEventForAWatchedPathWillCauseTheWorkerThreadToCallRegisteredHandler)
{
    const int watch_descriptor_1{10U};
    const int watch_descriptor_2{20U};
    const auto vector_with_creation_event =
        CreateEventVectorWithEventMasks({IN_CREATE, IN_CREATE}, {watch_descriptor_1, watch_descriptor_2});
    std::promise<void> event_read_with_creation_event_barrier{};
    std::promise<void> start_find_service_called_barrier{};
    auto start_find_service_called_future = start_find_service_called_barrier.get_future();
    MockFunction<void(ServiceHandleContainer<HandleType>, FindServiceHandle)> find_service_handler_1{};
    MockFunction<void(ServiceHandleContainer<HandleType>, FindServiceHandle)> find_service_handler_2{};

    // Expecting that a watch is added for each StartFindService call which each return a watch descriptor
    EXPECT_CALL(inotify_instance_mock_, AddWatch(_, _))
        .WillOnce(Return(os::InotifyWatchDescriptor{watch_descriptor_1}));
    EXPECT_CALL(inotify_instance_mock_, AddWatch(_, _))
        .WillOnce(Return(os::InotifyWatchDescriptor{watch_descriptor_2}))
        .RetiresOnSaturation();

    // and that INotify::Read() will be called which returns a vector containing creation events associated with the
    // added watches
    EXPECT_CALL(inotify_instance_mock_, Read())
        .WillOnce([&start_find_service_called_future, &vector_with_creation_event] {
            // Wait until StartFindService has been called twice to ensure that watch have been added for the
            // watch_descriptors returned by the AddWatch calls before INotify::Read() returns
            start_find_service_called_future.wait();

            return vector_with_creation_event;
        })
        .WillOnce([&event_read_with_creation_event_barrier] {
            // This is the second call to read. It indicates that Read was already called once with
            // vector_with_creation_event so the test is finished and the main thread can exit.
            event_read_with_creation_event_barrier.set_value();
            return score::cpp::static_vector<os::InotifyEvent, os::InotifyInstance::max_events>{};
        })
        .WillRepeatedly(Return(score::cpp::static_vector<os::InotifyEvent, os::InotifyInstance::max_events>{}));

    // and expecting that both StartFindService handlers will be called
    EXPECT_CALL(find_service_handler_1, Call(_, _));
    EXPECT_CALL(find_service_handler_2, Call(_, _));

    // Given a ServiceDiscoveryClient which spawns a worker thread and has two active StartFindService calls.
    const FindServiceHandle handle_1{make_FindServiceHandle(1U)};
    const FindServiceHandle handle_2{make_FindServiceHandle(2U)};
    WhichContainsAServiceDiscoveryClient()
        .WithAnActiveStartFindService(kConfigStoreQm1.GetInstanceIdentifier(),
                                      handle_1,
                                      CreateWrappedMockFindServiceHandler(find_service_handler_1))
        .WithAnActiveStartFindService(kConfigStoreQm2.GetInstanceIdentifier(),
                                      handle_2,
                                      CreateWrappedMockFindServiceHandler(find_service_handler_2));

    start_find_service_called_barrier.set_value();

    // When INotify::Read() is called which returns a vector containing a creation event associated with an active
    // watch
    event_read_with_creation_event_barrier.get_future().wait();
}

TEST_F(ServiceDiscoveryClientWorkerThreadFixture,
       CallingStopFindServiceWhileWorkerThreadIsHandlingEventThatWouldTriggerHandlerDoesNotCallHandler)
{
    const int watch_descriptor_1{10U};
    const int watch_descriptor_2{20U};
    const auto vector_with_creation_event =
        CreateEventVectorWithEventMasks({IN_CREATE, IN_CREATE}, {watch_descriptor_1, watch_descriptor_2});
    std::promise<void> event_read_with_creation_event_barrier{};
    std::promise<void> start_find_service_called_barrier{};
    auto start_find_service_called_future = start_find_service_called_barrier.get_future();
    MockFunction<void(ServiceHandleContainer<HandleType>, FindServiceHandle)> find_service_handler_2{};

    // Expecting that a watch is added for each StartFindService call which each return a watch descriptor
    EXPECT_CALL(inotify_instance_mock_, AddWatch(_, _))
        .WillOnce(Return(os::InotifyWatchDescriptor{watch_descriptor_1}));
    EXPECT_CALL(inotify_instance_mock_, AddWatch(_, _))
        .WillOnce(Return(os::InotifyWatchDescriptor{watch_descriptor_2}))
        .RetiresOnSaturation();

    // and that INotify::Read() will be called which returns a vector containing creation events associated with the
    // added watches
    EXPECT_CALL(inotify_instance_mock_, Read())
        .WillOnce([&start_find_service_called_future, &vector_with_creation_event] {
            // Wait until StartFindService has been called twice to ensure that watch have been added for the
            // watch_descriptors returned by the AddWatch calls before INotify::Read() returns
            start_find_service_called_future.wait();

            return vector_with_creation_event;
        })
        .WillOnce([&event_read_with_creation_event_barrier] {
            // This is the second call to read. It indicates that Read was already called once with
            // vector_with_creation_event so the test is finished and the main thread can exit.
            event_read_with_creation_event_barrier.set_value();
            return score::cpp::static_vector<os::InotifyEvent, os::InotifyInstance::max_events>{};
        })
        .WillRepeatedly(Return(score::cpp::static_vector<os::InotifyEvent, os::InotifyInstance::max_events>{}));

    // and expecting that the handler of the StartFindServiceCall which was subsequently stopped will not be called
    EXPECT_CALL(find_service_handler_2, Call(_, _)).Times(0);

    // Given a ServiceDiscoveryClient which spawns a worker thread and has two active StartFindService calls. The
    // handler of the first StartFindService will stop the second StartFindService. Note. we can safely call stop find
    // service with handle_2 before we actually call StartFindService the second time which returns handle_2 because the
    // service is not offered, so the handler will not be called synchronously in the first StartFindService call.
    const FindServiceHandle handle_1{make_FindServiceHandle(1U)};
    const FindServiceHandle handle_2{make_FindServiceHandle(2U)};
    const auto find_service_handler_1 = [this, handle_2](auto, auto) noexcept {
        EXPECT_TRUE(service_discovery_client_->StopFindService(handle_2).has_value());
    };
    WhichContainsAServiceDiscoveryClient()
        .WithAnActiveStartFindService(kConfigStoreQm1.GetInstanceIdentifier(), handle_1, find_service_handler_1)
        .WithAnActiveStartFindService(kConfigStoreQm2.GetInstanceIdentifier(),
                                      handle_2,
                                      CreateWrappedMockFindServiceHandler(find_service_handler_2));

    start_find_service_called_barrier.set_value();

    // When INotify::Read() is called which returns a vector containing a creation event associated with an active
    // watch
    event_read_with_creation_event_barrier.get_future().wait();
}

TEST_F(ServiceDiscoveryClientWorkerThreadFixture, RemovingFlagFileCorrespondingToSearchedInstanceCallsHandler)
{
    const int watch_descriptor{10U};
    const auto vector_with_delete_event =
        CreateEventVectorWithEventMasks({IN_CREATE, IN_DELETE}, {watch_descriptor, watch_descriptor});
    std::promise<void> event_read_with_creation_and_deletion_event_barrier{};
    std::promise<void> start_find_service_called_barrier{};
    auto start_find_service_called_future = start_find_service_called_barrier.get_future();
    MockFunction<void(ServiceHandleContainer<HandleType>, FindServiceHandle)> find_service_handler{};

    // Expecting that a watch is added by StartFindService on the instance directory which returns a watch
    // descriptor
    const auto expected_instance_directory_path =
        GenerateExpectedInstanceDirectoryPath(kServiceId, kConfigStoreQm1.lola_instance_id_.value().GetId()).Native();
    EXPECT_CALL(inotify_instance_mock_, AddWatch(expected_instance_directory_path, _))
        .WillOnce(Return(os::InotifyWatchDescriptor{watch_descriptor}));

    // Expecting that INotify::Read() will be called which returns a vector containing a creation and delete event for
    // the instance flag file
    EXPECT_CALL(inotify_instance_mock_, Read())
        .WillOnce([&start_find_service_called_future, &vector_with_delete_event] {
            // Wait until StartFindService has been called to ensure that a watch has been added for the
            // watch_descriptor returned by AddWatch before INotify::Read() returns
            start_find_service_called_future.wait();

            return vector_with_delete_event;
        })
        .WillOnce([&event_read_with_creation_and_deletion_event_barrier] {
            // This is the second call to read. It indicates that Read was already called once with
            // vector_with_delete_event so the test is finished and the main thread can exit.
            event_read_with_creation_and_deletion_event_barrier.set_value();
            return score::cpp::static_vector<os::InotifyEvent, os::InotifyInstance::max_events>{};
        })
        .WillRepeatedly(Return(score::cpp::static_vector<os::InotifyEvent, os::InotifyInstance::max_events>{}));

    // and expecting that the StartFindService handler will be called
    EXPECT_CALL(find_service_handler, Call(_, _));

    // Given a ServiceDiscoveryClient which spawns a worker thread and has an active StartFindService call
    const FindServiceHandle handle{make_FindServiceHandle(1U)};
    WhichContainsAServiceDiscoveryClient().WithAnActiveStartFindService(
        kConfigStoreQm1.GetInstanceIdentifier(), handle, CreateWrappedMockFindServiceHandler(find_service_handler));

    start_find_service_called_barrier.set_value();

    // When INotify::Read() is called which returns a vector containing a creation and deletion event for the instance
    // flag file
    event_read_with_creation_and_deletion_event_barrier.get_future().wait();
}

TEST_F(ServiceDiscoveryClientWorkerThreadFixture, RemovingAsilBFlagFileCorrespondingToSearchedInstanceCallsHandler)
{
    const int watch_descriptor{10U};
    const auto instance_flag_file_name = "dummy_name-asil-b";
    const auto vector_with_delete_event =
        CreateEventVectorWithEventMasks({IN_CREATE, IN_DELETE},
                                        {watch_descriptor, watch_descriptor},
                                        {instance_flag_file_name, instance_flag_file_name});
    std::promise<void> event_read_with_creation_and_deletion_event_barrier{};
    std::promise<void> start_find_service_called_barrier{};
    auto start_find_service_called_future = start_find_service_called_barrier.get_future();
    MockFunction<void(ServiceHandleContainer<HandleType>, FindServiceHandle)> find_service_handler{};

    // Expecting that a watch is added by StartFindService on the instance directory which returns a watch
    // descriptor
    const auto expected_instance_directory_path =
        GenerateExpectedInstanceDirectoryPath(kServiceId, kConfigStoreAsilB.lola_instance_id_.value().GetId()).Native();
    EXPECT_CALL(inotify_instance_mock_, AddWatch(expected_instance_directory_path, _))
        .WillOnce(Return(os::InotifyWatchDescriptor{watch_descriptor}));

    // Expecting that INotify::Read() will be called which returns a vector containing a creation and delete event for
    // the instance flag file
    EXPECT_CALL(inotify_instance_mock_, Read())
        .WillOnce([&start_find_service_called_future, &vector_with_delete_event] {
            // Wait until StartFindService has been called to ensure that a watch has been added for the
            // watch_descriptor returned by AddWatch before INotify::Read() returns
            start_find_service_called_future.wait();

            return vector_with_delete_event;
        })
        .WillOnce([&event_read_with_creation_and_deletion_event_barrier] {
            // This is the second call to read. It indicates that Read was already called once with
            // vector_with_delete_event so the test is finished and the main thread can exit.
            event_read_with_creation_and_deletion_event_barrier.set_value();
            return score::cpp::static_vector<os::InotifyEvent, os::InotifyInstance::max_events>{};
        })
        .WillRepeatedly(Return(score::cpp::static_vector<os::InotifyEvent, os::InotifyInstance::max_events>{}));

    // and expecting that the StartFindService handler will be called
    EXPECT_CALL(find_service_handler, Call(_, _));

    // Given a ServiceDiscoveryClient which spawns a worker thread and has an active StartFindService call
    const FindServiceHandle handle{make_FindServiceHandle(1U)};
    WhichContainsAServiceDiscoveryClient().WithAnActiveStartFindService(
        kConfigStoreAsilB.GetInstanceIdentifier(), handle, CreateWrappedMockFindServiceHandler(find_service_handler));

    start_find_service_called_barrier.set_value();

    // When INotify::Read() is called which returns a vector containing a creation and deletion event for the instance
    // flag file
    event_read_with_creation_and_deletion_event_barrier.get_future().wait();
}

}  // namespace
}  // namespace score::mw::com::impl::lola::test
