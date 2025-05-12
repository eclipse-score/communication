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
#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/proxy.h"
#include "score/mw/com/impl/bindings/lola/rollback_synchronization.h"
#include "score/mw/com/impl/bindings/lola/runtime_mock.h"
#include "score/mw/com/impl/bindings/lola/service_discovery/client/service_discovery_client.h"
#include "score/mw/com/impl/bindings/lola/test/proxy_event_test_resources.h"
#include "score/mw/com/impl/bindings/lola/test/service_discovery_test_resources.h"
#include "score/mw/com/impl/bindings/lola/test_doubles/fake_service_data.h"
#include "score/mw/com/impl/configuration/lola_event_id.h"
#include "score/mw/com/impl/configuration/lola_event_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/configuration/service_identifier_type.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"
#include "score/mw/com/impl/find_service_handler.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/proxy_base.h"
#include "score/mw/com/impl/service_discovery.h"
#include "score/mw/com/impl/service_discovery_mock.h"

#include "score/concurrency/long_running_threads_container.h"
#include "score/concurrency/notification.h"
#include "score/filesystem/filesystem_struct.h"
#include "score/filesystem/i_standard_filesystem.h"
#include "score/filesystem/path.h"
#include "score/os/unistd.h"
#include "score/os/utils/inotify/inotify_instance_impl.h"

#include <score/optional.hpp>
#include <score/stop_token.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace score::mw::com::impl::lola
{
namespace
{

using namespace ::testing;

#ifdef __QNXNTO__
const filesystem::Path kTmpPath{"/tmp_discovery/mw_com_lola/service_discovery"};
constexpr auto kServiceInstanceUsageMarkerFile =
    "/tmp_discovery/mw_com_lola/partial_restart/usage-0000000000052719-00016";
#else
const filesystem::Path kTmpPath{"/tmp/mw_com_lola/service_discovery"};
constexpr auto kServiceInstanceUsageMarkerFile = "/tmp/mw_com_lola/partial_restart/usage-0000000000052719-00016";
#endif

const std::string kEventName{"DummyEvent1"};
const std::string kNonProvidedEventName{"DummyEvent2"};
const ElementFqId kElementFqId{0xcdef, 0x5, 0x10, ElementType::EVENT};
const ElementFqId kNonProvidedElementFqId{0xcdef, 0x6, 0x10, ElementType::EVENT};

class ProxyWithRealMemFixture : public ::testing::Test
{
  public:
    void RegisterShmFile(std::string shm_file)
    {
        shm_files_.push_back(std::move(shm_file));
    }

    void SetUp() override
    {
        EXPECT_CALL(runtime_mock_.mock_, GetBindingRuntime(BindingType::kLoLa))
            .WillRepeatedly(::testing::Return(&lola_runtime_mock_));
        ON_CALL(lola_runtime_mock_, GetRollbackSynchronization())
            .WillByDefault(::testing::ReturnRef(rollback_synchronization_));
    }

    void TearDown() override
    {
        for (const auto& file : shm_files_)
        {
            score::filesystem::IStandardFilesystem::instance().Remove(std::string{"/dev/shm"} + file);
        }
        shm_files_.clear();
    }

    ProxyWithRealMemFixture& WithAMockedServiceDiscovery()
    {
        ON_CALL(runtime_mock_.mock_, GetServiceDiscovery())
            .WillByDefault(::testing::ReturnRef(service_discovery_mock_));
        return *this;
    }

    ProxyWithRealMemFixture& WithAConfigurationContainingEvents()
    {
        LolaServiceInstanceDeployment lola_service_instance_deployment{
            LolaServiceInstanceId{kElementFqId.instance_id_},
            {{kEventName, LolaEventInstanceDeployment{10U, 10U, 2U, true, 0}},
             {kNonProvidedEventName, LolaFieldInstanceDeployment{10U, 10U, 2U, true, 0}}}};
        LolaServiceTypeDeployment lola_service_type_deployment{
            kElementFqId.service_id_,
            {{kEventName, LolaEventId{kElementFqId.element_id_}},
             {kNonProvidedEventName, LolaEventId{kNonProvidedElementFqId.element_id_}}}};

        config_store_ =
            std::make_unique<ConfigurationStore>(InstanceSpecifier::Create("/my_dummy_instance_specifier").value(),
                                                 make_ServiceIdentifierType("foo"),
                                                 QualityType::kASIL_QM,
                                                 lola_service_type_deployment,
                                                 lola_service_instance_deployment);
        return *this;
    }

    ProxyWithRealMemFixture& WithAConfigurationContainingNoEvents()
    {
        LolaServiceInstanceDeployment lola_service_instance_deployment{
            LolaServiceInstanceId{kElementFqId.instance_id_}};
        LolaServiceTypeDeployment lola_service_type_deployment{kElementFqId.service_id_};

        config_store_ =
            std::make_unique<ConfigurationStore>(InstanceSpecifier::Create("/my_dummy_instance_specifier").value(),
                                                 make_ServiceIdentifierType("foo"),
                                                 QualityType::kASIL_QM,
                                                 lola_service_type_deployment,
                                                 lola_service_instance_deployment);

        return *this;
    }

    std::unique_ptr<FakeServiceData> CreateFakeSkeletonData(std::string control_file_name,
                                                            std::string data_file_name,
                                                            std::string service_instance_usage_marker_file,
                                                            bool init = true)
    {
        auto fake_skeleton_data = FakeServiceData::Create(control_file_name,
                                                          data_file_name,
                                                          service_instance_usage_marker_file,
                                                          os::Unistd::instance().getpid(),
                                                          init);
        if (fake_skeleton_data == nullptr)
        {
            return {};
        }
        RegisterShmFile(control_file_name);
        RegisterShmFile(data_file_name);

        return fake_skeleton_data;
    }

    std::unique_ptr<ConfigurationStore> config_store_{nullptr};

    std::vector<std::string> shm_files_{};
    RuntimeMockGuard runtime_mock_{};
    lola::RuntimeMock lola_runtime_mock_{};
    RollbackSynchronization rollback_synchronization_{};
    ServiceDiscoveryMock service_discovery_mock_{};
};

TEST_F(ProxyWithRealMemFixture, IsEventProvidedOnlyReturnsTrueIfEventIsInSharedMemory)
{
    WithAMockedServiceDiscovery().WithAConfigurationContainingEvents();

    const bool initialise_skeleton_data{true};
    auto fake_data_result = CreateFakeSkeletonData("/lola-ctl-0000000000052719-00016",
                                                   "/lola-data-0000000000052719-00016",
                                                   kServiceInstanceUsageMarkerFile,
                                                   initialise_skeleton_data);
    ASSERT_NE(fake_data_result, nullptr);

    // Only provide the first event in shared memory
    fake_data_result->AddEvent<std::uint8_t>(kElementFqId, SkeletonEventProperties{10U, 3U, true});

    const auto handle = config_store_->GetHandle();

    ON_CALL(service_discovery_mock_, StartFindService(::testing::_, EnrichedInstanceIdentifier{handle}))
        .WillByDefault(::testing::Return(make_FindServiceHandle(10U)));

    auto proxy = Proxy::Create(handle);
    ASSERT_NE(proxy, nullptr);
    EXPECT_TRUE(proxy->IsEventProvided(kEventName));
    EXPECT_FALSE(proxy->IsEventProvided(kNonProvidedEventName));
}

class ProxyServiceDiscoveryFixture : public ProxyWithRealMemFixture
{
  public:
    FindServiceHandler<HandleType> CreateNotifyingHandlerWhichCallsStartFindService(
        const InstanceIdentifier& instance_identifier)
    {
        return [this, &instance_identifier](auto handle_container, auto) noexcept {
            if (handle_container.size() > 0U)
            {
                // Notify main thread that the handler has started
                handler_started_notifier_.notify();

                // Sleep to allow the main thread time to call StartFindService / StopFindService
                std::this_thread::sleep_for(std::chrono::milliseconds{200U});

                const auto find_service_handle_result =
                    ProxyBase::StartFindService([](auto, auto) noexcept {}, instance_identifier);
                ASSERT_TRUE(find_service_handle_result.has_value());

                // Notify the main thread that the handler is done and it's safe to finish
                handler_done_notifier_.notify();
            }
        };
    }

    FindServiceHandler<HandleType> CreateNotifyingHandlerWhichCallsStopFindService()
    {
        return [this](auto handle_container, auto find_service_handle) noexcept {
            if (handle_container.size() > 0U)
            {
                // Notify main thread that the handler has started
                handler_started_notifier_.notify();

                // Sleep to allow the main thread time to call StartFindService / StopFindService
                std::this_thread::sleep_for(std::chrono::milliseconds{200U});

                const auto stop_find_service_result = ProxyBase::StopFindService(find_service_handle);
                ASSERT_TRUE(stop_find_service_result.has_value());

                // Notify the main thread that the handler is done and it's safe to finish
                handler_done_notifier_.notify();
            }
        };
    }

    ProxyWithRealMemFixture& WithARealServiceDiscovery()
    {
        ON_CALL(runtime_mock_.mock_, GetServiceDiscovery()).WillByDefault(ReturnRef(service_discovery_));
        ON_CALL(lola_runtime_mock_, GetServiceDiscoveryClient()).WillByDefault(ReturnRef(service_discovery_client_));

        return *this;
    }

    score::cpp::stop_source source_{};
    score::cpp::stop_token stop_token_{source_.get_token()};

    concurrency::Notification handler_started_notifier_{};
    concurrency::Notification handler_done_notifier_{};

    concurrency::LongRunningThreadsContainer long_running_threads_container_{};
    filesystem::Filesystem filesystem_{filesystem::FilesystemFactory{}.CreateInstance()};
    FileSystemGuard filesystem_guard_{filesystem_};

    // Note: service_discovery_ calls functions on service_discovery_client_ during destruction. Therefore, the ordering
    // of these 2 variables must not change to ensure the correct destruction sequence.
    ServiceDiscoveryClient service_discovery_client_{long_running_threads_container_,
                                                     std::make_unique<os::InotifyInstanceImpl>(),
                                                     std::make_unique<os::internal::UnistdImpl>(),
                                                     filesystem_};
    ServiceDiscovery service_discovery_{runtime_mock_.mock_};
};

// Test to check that race condition in Ticket-169333 does not occur.
TEST_F(ProxyServiceDiscoveryFixture, CallingStartFindServiceInHandlerAndStartFindServiceInMainThreadDoesNotDeadLock)
{
    MockFunction<void(ServiceHandleContainer<HandleType>, FindServiceHandle)> find_service_handler_2{};

    // Given a real ServiceDiscovery and a configuration that contains no events
    WithARealServiceDiscovery().WithAConfigurationContainingNoEvents();
    const InstanceIdentifier instance_identifier{config_store_->GetInstanceIdentifier()};

    // Expecting that the second FindServiceHandler is called once (when StartFindService is first called)
    EXPECT_CALL(find_service_handler_2, Call(_, _)).Times(1);

    // When calling StartFindService with a handler which will itself call StartFindService
    const auto find_service_handle_result = ProxyBase::StartFindService(
        CreateNotifyingHandlerWhichCallsStartFindService(instance_identifier), instance_identifier);
    ASSERT_TRUE(find_service_handle_result.has_value());

    // and when the service is offered
    const auto offer_service_result = service_discovery_.OfferService(instance_identifier);
    ASSERT_TRUE(offer_service_result.has_value());

    // Wait for a notification that the handler has been called and is about to call StartFindService
    handler_started_notifier_.waitWithAbort(stop_token_);

    // and when we call StartFindService again with a new handler
    const auto find_service_handle_result_2 =
        ProxyBase::StartFindService(CreateWrappedMockFindServiceHandler(find_service_handler_2), instance_identifier);
    ASSERT_TRUE(find_service_handle_result_2.has_value());

    // Then we expect that both calls to StartFindService are called without a dead lock and both handlers are called
    handler_done_notifier_.waitWithAbort(stop_token_);
}

// Test to check that race condition in Ticket-169333 does not occur.
TEST_F(ProxyServiceDiscoveryFixture, CallingStartFindServiceInHandlerAndStopFindServiceInMainThreadDoesNotDeadLock)
{
    // Given a real ServiceDiscovery and a configuration that contains no events
    WithARealServiceDiscovery().WithAConfigurationContainingNoEvents();
    const InstanceIdentifier instance_identifier{config_store_->GetInstanceIdentifier()};

    // When calling StartFindService with a handler which will itself call StartFindService
    const auto find_service_handle_result = ProxyBase::StartFindService(
        CreateNotifyingHandlerWhichCallsStartFindService(instance_identifier), instance_identifier);
    ASSERT_TRUE(find_service_handle_result.has_value());

    // and when the service is offered
    const auto offer_service_result = service_discovery_.OfferService(instance_identifier);
    ASSERT_TRUE(offer_service_result.has_value());

    // Wait for a notification that the handler has been called and is about to call StartFindService
    handler_started_notifier_.waitWithAbort(stop_token_);

    // and when we call StopFindService
    const auto stop_find_service_result = ProxyBase::StopFindService(find_service_handle_result.value());
    ASSERT_TRUE(stop_find_service_result.has_value());

    // Then we expect that both the call to StartFindService in the handler and the call to StopFindService are called
    // without a dead lock and that the handler are finishes
    handler_done_notifier_.waitWithAbort(stop_token_);
}

// Test to check that race condition in Ticket-169333 does not occur.
TEST_F(ProxyServiceDiscoveryFixture, CallingStopFindServiceInHandlerAndStartFindServiceInMainThreadDoesNotDeadLock)
{
    MockFunction<void(ServiceHandleContainer<HandleType>, FindServiceHandle)> find_service_handler_2{};

    // Given a real ServiceDiscovery and a configuration that contains no events
    WithARealServiceDiscovery().WithAConfigurationContainingNoEvents();
    const InstanceIdentifier instance_identifier{config_store_->GetInstanceIdentifier()};

    // Expecting that the second FindServiceHandler is called once (when StartFindService is first called)
    EXPECT_CALL(find_service_handler_2, Call(_, _)).Times(1);

    // When calling StartFindService with a handler which will itself call StopFindService
    const auto find_service_handle_result =
        ProxyBase::StartFindService(CreateNotifyingHandlerWhichCallsStopFindService(), instance_identifier);
    ASSERT_TRUE(find_service_handle_result.has_value());

    // and when the service is offered
    const auto offer_service_result = service_discovery_.OfferService(instance_identifier);
    ASSERT_TRUE(offer_service_result.has_value());

    // Wait for a notification that the handler has been called and is about to call StartFindService
    handler_started_notifier_.waitWithAbort(stop_token_);

    // and when we call StartFindService again with a new handler
    const auto find_service_handle_result_2 =
        ProxyBase::StartFindService(CreateWrappedMockFindServiceHandler(find_service_handler_2), instance_identifier);
    ASSERT_TRUE(find_service_handle_result_2.has_value());

    // Then we expect that both calls to StartFindService are called without a dead lock and both handlers are called
    handler_done_notifier_.waitWithAbort(stop_token_);
}

// Test to check that race condition in Ticket-169333 does not occur.
TEST_F(ProxyServiceDiscoveryFixture, CallingStopFindServiceInHandlerAndStopFindServiceInMainThreadDoesNotDeadLock)
{
    // Given a real ServiceDiscovery and a configuration that contains no events
    WithARealServiceDiscovery().WithAConfigurationContainingNoEvents();
    const InstanceIdentifier instance_identifier{config_store_->GetInstanceIdentifier()};

    // When calling StartFindService with a handler which will itself call StopFindService
    const auto find_service_handle_result =
        ProxyBase::StartFindService(CreateNotifyingHandlerWhichCallsStopFindService(), instance_identifier);
    ASSERT_TRUE(find_service_handle_result.has_value());

    // and when the service is offered
    const auto offer_service_result = service_discovery_.OfferService(instance_identifier);
    ASSERT_TRUE(offer_service_result.has_value());

    // Wait for a notification that the handler has been called and is about to call StartFindService
    handler_started_notifier_.waitWithAbort(stop_token_);

    // and when we call StopFindService
    const auto stop_find_service_result = ProxyBase::StopFindService(find_service_handle_result.value());
    ASSERT_TRUE(stop_find_service_result.has_value());

    // Then we expect that both the call to StartFindService in the handler and the call to StopFindService are called
    // without a dead lock and that the handler are finishes
    handler_done_notifier_.waitWithAbort(stop_token_);
}

}  // namespace
}  // namespace score::mw::com::impl::lola
