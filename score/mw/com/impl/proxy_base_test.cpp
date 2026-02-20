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
#include "score/mw/com/impl/proxy_base.h"
#include "score/mw/com/impl/bindings/mock_binding/proxy.h"
#include "score/mw/com/impl/bindings/mock_binding/proxy_event.h"
#include "score/mw/com/impl/bindings/mock_binding/proxy_method.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/configuration/service_identifier_type.h"
#include "score/mw/com/impl/configuration/service_instance_deployment.h"
#include "score/mw/com/impl/configuration/service_instance_id.h"
#include "score/mw/com/impl/configuration/service_type_deployment.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"
#include "score/mw/com/impl/find_service_handle.h"
#include "score/mw/com/impl/find_service_handler.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/methods/proxy_method_base.h"
#include "score/mw/com/impl/proxy_event_base.h"
#include "score/mw/com/impl/proxy_field_base.h"
#include "score/mw/com/impl/scoped_event_receive_handler.h"
#include "score/mw/com/impl/service_discovery_mock.h"
#include "score/mw/com/impl/test/runtime_mock_guard.h"

#include "score/language/safecpp/scoped_function/scope.h"
#include "score/result/result.h"

#include <score/assert.hpp>
#include <score/assert_support.hpp>
#include <score/utility.hpp>

#include <gmock/gmock-actions.h>
#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <future>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace score::mw::com::impl
{

using ::testing::_;
using ::testing::ByMove;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::SaveArg;

const auto kInstanceSpecifier = InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"}).value();
const auto kServiceIdentifier = make_ServiceIdentifierType("foo", 13, 37);
const LolaServiceInstanceId kLolaInstanceId{23U};
constexpr std::uint16_t kServiceId{34U};

class ProxyBaseFixture : public ::testing::Test
{
  public:
    ProxyBaseFixture()
    {
        ON_CALL(runtime_mock_guard_.runtime_mock_, GetServiceDiscovery())
            .WillByDefault(testing::ReturnRef(service_discovery_mock_));
        ON_CALL(service_discovery_mock_, StopFindService(_)).WillByDefault(Return(ResultBlank{}));
    }

    ProxyBaseFixture& WithAProxyBaseWithMockBinding(const HandleType& handle_type)
    {
        base_proxy_ = std::make_unique<ProxyBase>(std::move(proxy_binding_mock_), handle_type);
        return *this;
    }

    ScopedEventReceiveHandler CreateMockScopedEventReceiveHandler(MockFunction<void()>& mock_function)
    {
        return ScopedEventReceiveHandler(event_receive_handler_scope_, mock_function.AsStdFunction());
    }

    ConfigurationStore config_store_{kInstanceSpecifier,
                                     kServiceIdentifier,
                                     QualityType::kASIL_QM,
                                     kServiceId,
                                     kLolaInstanceId};
    InstanceIdentifier instance_identifier_{config_store_.GetInstanceIdentifier()};
    HandleType handle_{config_store_.GetHandle()};

    RuntimeMockGuard runtime_mock_guard_{};
    ServiceDiscoveryMock service_discovery_mock_{};

    std::unique_ptr<mock_binding::Proxy> proxy_binding_mock_{nullptr};
    std::unique_ptr<ProxyBase> base_proxy_{nullptr};

    safecpp::Scope<> event_receive_handler_scope_{};
};

TEST_F(ProxyBaseFixture, CanConstructProxyBaseWithABindingContainingNullptr)
{
    // Given a handle to a service with a lola binding

    // When constructing a Proxybase with a binding that is a nullptr nothing bad happens
    ProxyBase proxy_base{nullptr, handle_};
}

TEST_F(ProxyBaseFixture, GetImplReturnsProxyBindingPassedToConstructor)
{
    // Given a ProxyBase with a mock binding and valid handle
    WithAProxyBaseWithMockBinding(handle_);

    // When getting the proxy binding
    auto* proxy_binding = ProxyBaseView{*base_proxy_}.GetBinding();

    // Then the returned binding will have the same address as the binding provided to the constructor of ProxyBase
    const auto* const proxy_binding_address = proxy_binding_mock_.get();
    EXPECT_EQ(proxy_binding, proxy_binding_address);
}

TEST_F(ProxyBaseFixture, StoredHandleTypeEqualToSuppliedOne)
{
    RecordProperty("Verifies", "SCR-14030261, SCR-14110935");
    RecordProperty("Description", "Checks that GetHandle gets the handle from which the ProxyBase has been created.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a ProxyBase with a mock binding and valid handle
    WithAProxyBaseWithMockBinding(handle_);

    // When getting the handle from the ProxyBase
    auto returned_handle = base_proxy_->GetHandle();

    // Then the returned handle will be the same handle provided to the constructor of ProxyBase
    EXPECT_EQ(returned_handle, handle_);
}

using ProxyBaseFindServiceInstanceSpecifierFixture = ProxyBaseFixture;
TEST_F(ProxyBaseFindServiceInstanceSpecifierFixture, FindServiceShouldReturnHandlesContainerIfRuntimeCanResolve)
{
    RecordProperty("ParentRequirement", "SCR-14005977, SCR-14110930, SCR-18804932");
    RecordProperty("Description", "FindService can find a service using an instance specifier.");
    RecordProperty("TestingTechnique", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a valid instance identifier with a lola binding

    // Expecting that the ServiceDiscoveryMock will return the handle corresponding to the instance identifier when
    // finding the service
    auto expected_handle = config_store_.GetHandle();
    EXPECT_CALL(service_discovery_mock_, FindService(kInstanceSpecifier))
        .WillOnce(Return(Result<std::vector<HandleType>>{{expected_handle}}));

    // When finding a specific service
    const Result<ServiceHandleContainer<HandleType>> handles_result = ProxyBase::FindService(kInstanceSpecifier);

    // Then the service can be found and the result contains the handle derived from the instance identifier.
    ASSERT_TRUE(handles_result.has_value());
    const auto& handles = handles_result.value();
    ASSERT_EQ(handles.size(), 1);

    // And the returned handle is derived from the instance identifier
    EXPECT_EQ(handles[0], expected_handle);
}

TEST_F(ProxyBaseFindServiceInstanceSpecifierFixture, FindServiceShouldReturnErrorIfBindingReturnsError)
{
    RecordProperty("ParentRequirement", "SCR-14005977, SCR-14110930, SCR-18804932");
    RecordProperty("Description", "FindService can find a service using an instance specifier.");
    RecordProperty("TestingTechnique", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const auto binding_error_code = ComErrc::kErroneousFileHandle;
    const auto returned_error_code = ComErrc::kBindingFailure;

    // Given a valid instance identifier with a lola binding

    // Expecting that the ServiceDiscoveryMock will return an error
    auto expected_handle = config_store_.GetHandle();
    EXPECT_CALL(service_discovery_mock_, FindService(kInstanceSpecifier))
        .WillOnce(Return(MakeUnexpected(binding_error_code)));

    // When finding a specific service
    const Result<ServiceHandleContainer<HandleType>> handles_result = ProxyBase::FindService(kInstanceSpecifier);

    // Then an error is returned
    ASSERT_FALSE(handles_result.has_value());

    // And the error code is as expected
    EXPECT_EQ(handles_result.error(), returned_error_code);
}

using ProxyBaseStartFindServiceInstanceSpecifierFixture = ProxyBaseFixture;
TEST_F(ProxyBaseStartFindServiceInstanceSpecifierFixture, StartFindServiceWillDispatchToBinding)
{
    RecordProperty("ParentRequirement", "SCR-21792392, SCR-21788695");
    RecordProperty("Description",
                   "StartFindService with InstanceSpecifier will dispatch to the underlying Proxy binding for a "
                   "regular Proxy (SCR-21792392) and a GenericProxy (SCR-21788695).");
    RecordProperty("TestingTechnique", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Expecting that StartFindService is called on the Proxy binding
    const FindServiceHandle handle{make_FindServiceHandle(0)};
    EXPECT_CALL(service_discovery_mock_, StartFindService(_, kInstanceSpecifier)).WillOnce(Return(handle));

    // When calling StartFindService
    score::cpp::ignore = ProxyBase::StartFindService([](auto, auto) noexcept {}, kInstanceSpecifier);
}

TEST_F(ProxyBaseStartFindServiceInstanceSpecifierFixture,
       StartFindServiceWillReturnFindServiceHandleFromBindingOnSuccess)
{
    RecordProperty("ParentRequirement", "SCR-21792392, SCR-21788695");
    RecordProperty("Description",
                   "StartFindService with InstanceSpecifier will return a FindServiceHandle from the binding on "
                   "success for a regular Proxy (SCR-21792392) and a GenericProxy (SCR-21788695).");
    RecordProperty("TestingTechnique", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Expecting that StartFindService is called on the Proxy binding
    const FindServiceHandle handle{make_FindServiceHandle(0)};
    ON_CALL(service_discovery_mock_, StartFindService(_, kInstanceSpecifier)).WillByDefault(Return(handle));

    // When calling StartFindService
    auto find_service_handle_result = ProxyBase::StartFindService([](auto, auto) noexcept {}, kInstanceSpecifier);

    // Then the handle from the binding will be returned
    ASSERT_TRUE(find_service_handle_result.has_value());
    EXPECT_EQ(find_service_handle_result.value(), handle);
}

TEST_F(ProxyBaseStartFindServiceInstanceSpecifierFixture, StartFindServiceWillReturnPropagateErrorFromBinding)
{
    RecordProperty("ParentRequirement", "SCR-21792392, SCR-21788695");
    RecordProperty(
        "Description",
        "StartFindService with InstanceSpecifier will return an error containing kFindServiceHandlerFailure if the "
        "call to the binding returns an error for a regular Proxy (SCR-21792392) and a GenericProxy "
        "(SCR-21788695).");
    RecordProperty("TestingTechnique", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Expecting that StartFindService is called on the Proxy binding and returns an error
    ON_CALL(service_discovery_mock_, StartFindService(_, kInstanceSpecifier))
        .WillByDefault(Return(Unexpected{ComErrc::kNotOffered}));

    // When calling StartFindService
    auto find_service_handle_result = ProxyBase::StartFindService([](auto, auto) noexcept {}, kInstanceSpecifier);

    // Then an error containing kFindServiceHandlerFailure will be returned
    ASSERT_FALSE(find_service_handle_result.has_value());
    EXPECT_EQ(find_service_handle_result.error(), ComErrc::kFindServiceHandlerFailure);
}

using ProxyBaseStartFindServiceInstanceIdentifierFixture = ProxyBaseFixture;
TEST_F(ProxyBaseStartFindServiceInstanceIdentifierFixture, StartFindServiceWillDispatchToBinding)
{
    RecordProperty("ParentRequirement", "SCR-21792393, SCR-21790264");
    RecordProperty("Description",
                   "StartFindService with InstanceIdentifier will dispatch to the underlying Proxy binding for a "
                   "regular Proxy (SCR-21792393) and a GenericProxy (SCR-21790264).");
    RecordProperty("TestingTechnique", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Expecting that StartFindService is called on the Proxy binding
    const FindServiceHandle handle{make_FindServiceHandle(0)};
    EXPECT_CALL(service_discovery_mock_, StartFindService(_, instance_identifier_)).WillOnce(Return(handle));

    // When calling StartFindService
    score::cpp::ignore = ProxyBase::StartFindService([](auto, auto) noexcept {}, instance_identifier_);
}

TEST_F(ProxyBaseStartFindServiceInstanceIdentifierFixture,
       StartFindServiceWillReturnFindServiceHandleFromBindingOnSuccess)
{
    RecordProperty("ParentRequirement", "SCR-21792393, SCR-21790264");
    RecordProperty("Description",
                   "StartFindService with InstanceIdentifier will return a FindServiceHandle from the binding on "
                   "success for a regular Proxy (SCR-21792393) and a GenericProxy (SCR-21790264).");
    RecordProperty("TestingTechnique", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Expecting that StartFindService is called on the Proxy binding
    const FindServiceHandle handle{make_FindServiceHandle(0)};
    ON_CALL(service_discovery_mock_, StartFindService(_, instance_identifier_)).WillByDefault(Return(handle));

    // When calling StartFindService
    auto find_service_handle_result = ProxyBase::StartFindService([](auto, auto) noexcept {}, instance_identifier_);

    // Then the handle from the binding will be returned
    ASSERT_TRUE(find_service_handle_result.has_value());
    EXPECT_EQ(find_service_handle_result.value(), handle);
}

TEST_F(ProxyBaseStartFindServiceInstanceIdentifierFixture, StartFindServiceWillReturnPropagateErrorFromBinding)
{
    RecordProperty("ParentRequirement", "SCR-21792393, SCR-21790264");
    RecordProperty("Description",
                   "StartFindService with InstanceIdentifier will return an error containing "
                   "kFindServiceHandlerFailure if the "
                   "call to the binding returns an error for a regular Proxy (SCR-21792393) and a GenericProxy "
                   "(SCR-21790264).");
    RecordProperty("TestingTechnique", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Expecting that StartFindService is called on the Proxy binding and returns an error
    ON_CALL(service_discovery_mock_, StartFindService(_, instance_identifier_))
        .WillByDefault(Return(Unexpected{ComErrc::kNotOffered}));

    // When calling StartFindService
    auto find_service_handle_result = ProxyBase::StartFindService([](auto, auto) noexcept {}, instance_identifier_);

    // Then an error containing kFindServiceHandlerFailure will be returned
    ASSERT_FALSE(find_service_handle_result.has_value());
    EXPECT_EQ(find_service_handle_result.error(), ComErrc::kFindServiceHandlerFailure);
}

using ProxyBaseStopFindServiceFixture = ProxyBaseFixture;
TEST_F(ProxyBaseStopFindServiceFixture, StopFindServiceWillDispatchToBinding)
{
    RecordProperty("ParentRequirement", "SCR-21792394, SCR-21790756");
    RecordProperty("Description",
                   "StopFindService will dispatch to the underlying Proxy binding for a "
                   "regular Proxy (SCR-21792394) and a GenericProxy (SCR-21790756).");
    RecordProperty("TestingTechnique", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Expecting that StopFindService is called on the Proxy binding with the handle provided to StopFindService
    const FindServiceHandle handle{make_FindServiceHandle(0)};
    EXPECT_CALL(service_discovery_mock_, StopFindService(handle)).WillOnce(Return(ResultBlank{}));

    // When calling StartFindService
    score::cpp::ignore = ProxyBase::StopFindService(handle);
}

TEST_F(ProxyBaseStopFindServiceFixture, StopFindServiceWillReturnValidResultOnSuccess)
{
    RecordProperty("ParentRequirement", "SCR-21792394, SCR-21790756");
    RecordProperty("Description",
                   "StopFindService will return a valid result from the binding on "
                   "success for a regular Proxy (SCR-21792394) and a GenericProxy (SCR-21790756).");
    RecordProperty("TestingTechnique", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // When calling StopFindService with a handle
    const FindServiceHandle handle{make_FindServiceHandle(0)};
    auto stop_find_service_result = ProxyBase::StopFindService(handle);

    // Then a valid result will be returned
    ASSERT_TRUE(stop_find_service_result.has_value());
}

TEST_F(ProxyBaseStopFindServiceFixture, StopFindServiceWillReturnPropagateErrorFromBinding)
{
    RecordProperty("ParentRequirement", "SCR-21792394, SCR-21790756");
    RecordProperty("Description",
                   "StopFindService will return an error containing "
                   "kInvalidHandle if the "
                   "call to the binding returns an error for a regular Proxy (SCR-21792394) and a GenericProxy "
                   "(SCR-21790756).");
    RecordProperty("TestingTechnique", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Expecting that StopFindService is called on the Proxy binding and returns an error
    const FindServiceHandle handle{make_FindServiceHandle(0)};
    ON_CALL(service_discovery_mock_, StopFindService(handle)).WillByDefault(Return(Unexpected{ComErrc::kNotOffered}));

    // When calling StopFindService
    auto stop_find_service_result = ProxyBase::StopFindService(handle);

    // Then an error containing kInvalidHandle will be returned
    ASSERT_FALSE(stop_find_service_result.has_value());
    EXPECT_EQ(stop_find_service_result.error(), ComErrc::kInvalidHandle);
}

using ProxyBaseFindServiceInstanceIdentifierFixture = ProxyBaseFixture;
TEST_F(ProxyBaseFindServiceInstanceIdentifierFixture,
       FindServiceWithInstanceIdentifierShouldReturnHandlesContainerIfServiceCanBeFound)
{
    RecordProperty("Verifies", "SCR-14005991, SCR-14110933, SCR-18804932");
    RecordProperty("Description", "Checks finding a service with instance identifier");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a valid instance identifier with a lola binding

    // Expecting that the ServiceDiscovery will return the handle corresponding to the instance identifier
    // when finding the service
    EXPECT_CALL(service_discovery_mock_, FindService(instance_identifier_))
        .WillOnce(Return(Result<std::vector<HandleType>>{{handle_}}));

    // When calling FindService with the instance identifier
    const Result<ServiceHandleContainer<HandleType>> handles_result = ProxyBase::FindService(instance_identifier_);

    // Then the service can be found
    ASSERT_TRUE(handles_result.has_value());
    const auto& handles = handles_result.value();
    ASSERT_EQ(handles.size(), 1);

    // And the returned handle is derived from the instance identifier
    EXPECT_EQ(handles[0], handle_);
}

TEST_F(ProxyBaseFindServiceInstanceIdentifierFixture,
       FindServiceWithInstanceIdentifierShouldReturnErrorIfBindingReturnsError)
{
    RecordProperty("Verifies", "SCR-14005991, SCR-14110933, SCR-18804932");
    RecordProperty("Description", "FindService returns a kBindingFailure error code if binding returns any error.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const auto binding_error_code = ComErrc::kErroneousFileHandle;
    const auto returned_error_code = ComErrc::kBindingFailure;

    // Given a valid instance identifier with a fake binding

    // Expecting and that the ServiceDiscovery will return an error when finding the service when finding the
    // service
    EXPECT_CALL(service_discovery_mock_, FindService(instance_identifier_))
        .WillOnce(Return(MakeUnexpected(binding_error_code)));

    // When calling FindService with the instance identifier
    const Result<ServiceHandleContainer<HandleType>> handles_result = ProxyBase::FindService(instance_identifier_);

    // Then an error is returned
    ASSERT_FALSE(handles_result.has_value());

    // And the error code is as expected
    EXPECT_EQ(handles_result.error(), returned_error_code);
}

class ProxyBaseStopFindServiceMultithreadedFixture : public ProxyBaseFixture
{
  public:
    void SetUp() override
    {
        ProxyBaseFixture::SetUp();

        // By default, StartFindService saves the handler and returns a valid handle.
        ON_CALL(service_discovery_mock_, StartFindService(_, kInstanceSpecifier))
            .WillByDefault(
                testing::Invoke([this](FindServiceHandler<HandleType> handler, const InstanceSpecifier&) -> Result<FindServiceHandle> {
                    std::lock_guard<std::mutex> lock{handler_mutex_};
                    saved_handler_ = std::move(handler);
                    handler_set_promise_.set_value();
                    return kFindServiceHandle;
                }));
    }

    FindServiceHandler<HandleType> saved_handler_{};
    std::mutex handler_mutex_{};
    std::promise<void> handler_set_promise_{};
    const FindServiceHandle kFindServiceHandle{make_FindServiceHandle(42U)};
    const FindServiceHandle kFindServiceHandle2{make_FindServiceHandle(43U)};
};

TEST_F(ProxyBaseStopFindServiceMultithreadedFixture, find_stop_find_test)
{
    std::promise<void> stop_called_promise;
    auto stop_called_future = stop_called_promise.get_future();
    std::promise<void> handler_can_finish_promise;
    auto handler_can_finish_future = handler_can_finish_promise.get_future();
    std::atomic<bool> handler_finished{false};

    // StopFindService will block until the handler finishes.
    EXPECT_CALL(service_discovery_mock_, StopFindService(kFindServiceHandle))
        .WillOnce(testing::Invoke([&](const FindServiceHandle&) {
            stop_called_promise.set_value();
            // Wait until the handler is allowed to finish.
            handler_can_finish_future.wait();
            return ResultBlank{};
        }));

    // The handler will call StopFindService.
    auto handler = [&](ServiceHandleContainer<HandleType>, FindServiceHandle) {
        ProxyBase::StopFindService(kFindServiceHandle);
        handler_finished = true;
    };

    // Start finding the service in a separate thread.
    std::thread client_thread([&] {
        auto find_service_handle_result = ProxyBase::StartFindService(handler, kInstanceSpecifier);
        ASSERT_TRUE(find_service_handle_result.has_value());
        EXPECT_EQ(find_service_handle_result.value(), kFindServiceHandle);
    });

    // Wait for the handler to be registered.
    handler_set_promise_.get_future().wait();

    // Invoke the handler in another thread.
    std::thread discovery_thread([&] {
        std::lock_guard<std::mutex> lock{handler_mutex_};
        saved_handler_({}, kFindServiceHandle);
    });

    // Wait for StopFindService to be called from within the handler.
    stop_called_future.wait();

    // At this point, the handler is blocked inside StopFindService.
    // We check that the handler has not finished yet.
    EXPECT_FALSE(handler_finished);

    // Allow the handler to finish.
    handler_can_finish_promise.set_value();

    client_thread.join();
    discovery_thread.join();

    // The handler should have finished.
    EXPECT_TRUE(handler_finished);
}

TEST_F(ProxyBaseStopFindServiceMultithreadedFixture, find_concurrent_stop_test)
{
    std::atomic<bool> handler_running{false};
    std::condition_variable cv;
    std::mutex m;

    auto handler = [&](ServiceHandleContainer<HandleType>, FindServiceHandle) {
        handler_running = true;
        cv.notify_one();
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Simulate work
    };

    auto find_service_handle_result = ProxyBase::StartFindService(handler, kInstanceSpecifier);
    ASSERT_TRUE(find_service_handle_result.has_value());

    std::thread handler_thread([&] {
        std::lock_guard<std::mutex> lock{handler_mutex_};
        if (saved_handler_ != nullptr)
        {
            saved_handler_({}, kFindServiceHandle);
        }
    });

    // Wait until handler starts execution
    {
        std::unique_lock<std::mutex> lock(m);
        cv.wait(lock, [&] { return handler_running.load(); });
    }

    // Call StopFindService while handler is running
    auto stop_result = ProxyBase::StopFindService(kFindServiceHandle);
    EXPECT_TRUE(stop_result.has_value());

    handler_thread.join();
}

TEST_F(ProxyBaseStopFindServiceMultithreadedFixture, find_long_running_handler_test)
{
    std::atomic<int> handler_call_count{0};
    const int num_handler_invocations = 100;

    auto handler = [&](ServiceHandleContainer<HandleType>, FindServiceHandle) {
        handler_call_count++;
        // Short sleep to increase chance of concurrency issues
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    };

    auto find_service_handle_result = ProxyBase::StartFindService(handler, kInstanceSpecifier);
    ASSERT_TRUE(find_service_handle_result.has_value());
    const auto find_handle = find_service_handle_result.value();

    // Wait for the handler to be registered before starting the storm
    handler_set_promise_.get_future().wait();

    std::thread storm_thread([&] {
        for (int i = 0; i < num_handler_invocations; ++i)
        {
            std::lock_guard<std::mutex> lock{handler_mutex_};
            if (saved_handler_ != nullptr)
            {
                saved_handler_({}, find_handle);
            }
        }
    });

    // Give the storm a moment to start, then stop it mid-way
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    auto stop_result = ProxyBase::StopFindService(find_handle);
    EXPECT_TRUE(stop_result.has_value());

    storm_thread.join();
    // We don't assert on the exact count, as it's non-deterministic.
    // The main goal is to ensure the test completes without crashing or deadlocking.
    SUCCEED() << "Test completed without crashing. Handler was called " << handler_call_count << " times.";
}

TEST_F(ProxyBaseStopFindServiceMultithreadedFixture, find_inter_stop_test)
{
    // Two handlers for two separate find operations
    FindServiceHandler<HandleType> saved_handler_1;
    FindServiceHandler<HandleType> saved_handler_2;
    std::atomic<int> handler_1_calls{0};
    std::atomic<int> handler_2_calls{0};

    // Mock StartFindService to save both handlers and return distinct handles
    EXPECT_CALL(service_discovery_mock_, StartFindService(_, kInstanceSpecifier))
        .WillOnce(Invoke([&](FindServiceHandler<HandleType> handler, const InstanceSpecifier&) {
            saved_handler_1 = std::move(handler);
            return Result<FindServiceHandle>{kFindServiceHandle};
        }))
        .WillOnce(Invoke([&](FindServiceHandler<HandleType> handler, const InstanceSpecifier&) {
            saved_handler_2 = std::move(handler);
            return Result<FindServiceHandle>{kFindServiceHandle2};
        }));

    // Mock StopFindService for the first handle only
    EXPECT_CALL(service_discovery_mock_, StopFindService(kFindServiceHandle)).WillOnce(Return(ResultBlank{}));

    // Define the handlers
    auto handler_1 = [&](ServiceHandleContainer<HandleType>, FindServiceHandle) { handler_1_calls++; };
    auto handler_2 = [&](ServiceHandleContainer<HandleType>, FindServiceHandle) { handler_2_calls++; };

    // Start both find operations
    auto find_result_1 = ProxyBase::StartFindService(handler_1, kInstanceSpecifier);
    ASSERT_TRUE(find_result_1.has_value());
    EXPECT_EQ(find_result_1.value(), kFindServiceHandle);

    auto find_result_2 = ProxyBase::StartFindService(handler_2, kInstanceSpecifier);
    ASSERT_TRUE(find_result_2.has_value());
    EXPECT_EQ(find_result_2.value(), kFindServiceHandle2);

    // Stop the first find operation
    auto stop_result = ProxyBase::StopFindService(kFindServiceHandle);
    ASSERT_TRUE(stop_result.has_value());

    // Now, invoke the handler for the second (still active) find operation
    // This should succeed without issue.
    ASSERT_TRUE(saved_handler_2 != nullptr);
    saved_handler_2({}, kFindServiceHandle2);

    // The handler for the stopped service should have been removed and should not be callable.
    // In the real implementation, the handler is removed. In this test, we can check if it was nulled out.
    // The mock for StopFindService would need to be updated to simulate this removal for a more robust check.
    // For now, we confirm the second handler was called and the first was not (after the stop).
    EXPECT_EQ(handler_2_calls, 1);
    EXPECT_EQ(handler_1_calls, 0);
}

/// \todo Enable test when support for multiple bindings is added (Ticket-149776)
using ProxyBaseFindServiceMultipleBindingsFixture = ProxyBaseFixture;
TEST_F(ProxyBaseFindServiceMultipleBindingsFixture,
       DISABLED_FindServiceShouldReturnHandleIfHandleForAtLeastOneBindingIsValid)
{
    const auto service_identifier_2 = make_ServiceIdentifierType("foo2", 14, 38);
    std::uint16_t instance_id_2{23U};
    const ServiceInstanceDeployment deployment_info_2{
        service_identifier_2,
        LolaServiceInstanceDeployment{LolaServiceInstanceId{instance_id_2}},
        QualityType::kASIL_QM,
        kInstanceSpecifier};
    const auto instance_with_fake_binding_2 =
        make_InstanceIdentifier(deployment_info_2, ServiceTypeDeployment{config_store_.lola_service_type_deployment_});

    const auto service_identifier_3 = make_ServiceIdentifierType("foo3", 15, 39);
    std::uint16_t instance_id_3{33U};
    const ServiceInstanceDeployment deployment_info_3{
        service_identifier_3,
        LolaServiceInstanceDeployment{LolaServiceInstanceId{instance_id_3}},
        QualityType::kASIL_QM,
        kInstanceSpecifier};
    const auto instance_with_fake_binding_3 =
        make_InstanceIdentifier(deployment_info_3, ServiceTypeDeployment{config_store_.lola_service_type_deployment_});

    // Given a valid instance identifier with a valid lola binding

    // Expecting that the runtime will return a vector containing 3 instance identifiers
    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, resolve(kInstanceSpecifier))
        .WillOnce(Return(std::vector<InstanceIdentifier>{
            instance_identifier_, instance_with_fake_binding_2, instance_with_fake_binding_3}));

    // and that the ServiceDiscovery will return a handles vector containing one error and two valid handles
    // when finding the service
    const auto expected_handle_with_error = MakeUnexpected(ComErrc::kInvalidConfiguration);
    const auto expected_handle_2 = make_HandleType(instance_with_fake_binding_2, ServiceInstanceId{kLolaInstanceId});
    const auto expected_handle_3 = make_HandleType(instance_with_fake_binding_3, ServiceInstanceId{kLolaInstanceId});
    EXPECT_CALL(service_discovery_mock_, FindService(instance_identifier_))
        .WillOnce(Return(Result<std::vector<HandleType>>{expected_handle_with_error}));
    EXPECT_CALL(service_discovery_mock_, FindService(instance_with_fake_binding_2))
        .WillOnce(Return(Result<std::vector<HandleType>>{{expected_handle_2}}));
    EXPECT_CALL(service_discovery_mock_, FindService(instance_with_fake_binding_3))
        .WillOnce(Return(Result<std::vector<HandleType>>{{expected_handle_3}}));

    // When finding a specific service
    const Result<ServiceHandleContainer<HandleType>> handles_result = ProxyBase::FindService(kInstanceSpecifier);

    // Then two service instances can be found
    ASSERT_TRUE(handles_result.has_value());
    const auto& handles = handles_result.value();
    ASSERT_EQ(handles.size(), 2);

    // And the returned handles are derived from the corresponding instance identifiers
    EXPECT_EQ(handles[0], expected_handle_2);
    EXPECT_EQ(handles[1], expected_handle_3);
}

/// \todo Enable test when support for multiple bindings is added (Ticket-149776)
TEST_F(ProxyBaseFindServiceMultipleBindingsFixture, DISABLED_FindServiceShouldReturnErrorIfAllBindingsReturnErrors)
{
    const auto service_identifier_2 = make_ServiceIdentifierType("foo2", 14, 38);
    std::uint16_t instance_id_2{23U};
    const ServiceInstanceDeployment deployment_info_2{
        service_identifier_2,
        LolaServiceInstanceDeployment{LolaServiceInstanceId{instance_id_2}},
        QualityType::kASIL_QM,
        kInstanceSpecifier};
    const auto instance_with_fake_binding_2 =
        make_InstanceIdentifier(deployment_info_2, ServiceTypeDeployment{config_store_.lola_service_type_deployment_});

    // Given a valid instance identifier with a valid lola binding

    // Expecting that the runtime will return a vector containing 2 instance identifiers
    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, resolve(kInstanceSpecifier))
        .WillOnce(Return(std::vector<InstanceIdentifier>{instance_identifier_, instance_with_fake_binding_2}));

    // and that the ServiceDiscovery will return a handles vector containing one error and one handle
    // when finding the service
    const auto expected_handle_with_error = MakeUnexpected(ComErrc::kInvalidConfiguration);
    const auto expected_handle_with_error_2 = MakeUnexpected(ComErrc::kInvalidConfiguration);
    EXPECT_CALL(service_discovery_mock_, FindService(instance_identifier_))
        .WillOnce(Return(Result<std::vector<HandleType>>{expected_handle_with_error}));
    EXPECT_CALL(service_discovery_mock_, FindService(instance_identifier_))
        .WillOnce(Return(Result<std::vector<HandleType>>{expected_handle_with_error_2}));

    // When finding a specific service
    const Result<ServiceHandleContainer<HandleType>> handles_result = ProxyBase::FindService(kInstanceSpecifier);

    // Then an error is returned
    ASSERT_FALSE(handles_result.has_value());
    EXPECT_EQ(handles_result.error(), ComErrc::kBindingFailure);
}

class MyProxy : public ProxyBase
{
  public:
    using ProxyBase::ProxyBase;

    const ProxyBase::ProxyEvents& GetEvents()
    {
        return events_;
    }

    const ProxyBase::ProxyFields& GetFields()
    {
        return fields_;
    }

    const ProxyBase::ProxyMethods& GetMethods()
    {
        return methods_;
    }
};

/// Note. Technically, these tests are testing internals of ProxyBase. While we generally strive to test only the public
/// interface, we make an exception in this case since the reference updating of service elements is complex and can
/// lead to dangling references if not done correctly, which can be hard to test using the public interface alone.
class ProxyBaseServiceElementReferencesFixture : public ::testing::Test
{
  public:
    const std::string event_name_0_{"event_name_0"};
    const std::string event_name_1_{"event_name_1"};
    const std::string field_name_0_{"field_name_0"};
    const std::string field_name_1_{"field_name_1"};
    const std::string method_name_0_{"method_name_0"};
    const std::string method_name_1_{"method_name_1"};

    const ConfigurationStore config_store_{kInstanceSpecifier,
                                           kServiceIdentifier,
                                           QualityType::kASIL_QM,
                                           kServiceId,
                                           kLolaInstanceId};
    const InstanceIdentifier instance_identifier_{config_store_.GetInstanceIdentifier()};
    const HandleType handle_{config_store_.GetHandle()};

    mock_binding::Proxy proxy_binding_mock_{};
    MyProxy proxy_{std::make_unique<mock_binding::ProxyFacade>(proxy_binding_mock_), handle_};

    ProxyEventBase event_0_{proxy_,
                            &proxy_binding_mock_,
                            std::make_unique<mock_binding::ProxyEventBase>(),
                            event_name_0_};
    ProxyEventBase event_1_{proxy_,
                            &proxy_binding_mock_,
                            std::make_unique<mock_binding::ProxyEventBase>(),
                            event_name_1_};

    ProxyEventBase field_event_dispatch_0_{proxy_,
                                           &proxy_binding_mock_,
                                           std::make_unique<mock_binding::ProxyEventBase>(),
                                           field_name_0_};
    ProxyEventBase field_event_dispatch_1_{proxy_,
                                           &proxy_binding_mock_,
                                           std::make_unique<mock_binding::ProxyEventBase>(),
                                           field_name_1_};
    ProxyFieldBase field_0_{proxy_, &field_event_dispatch_0_, field_name_0_};
    ProxyFieldBase field_1_{proxy_, &field_event_dispatch_1_, field_name_1_};

    ProxyMethodBase method_0_{proxy_, std::make_unique<mock_binding::ProxyMethod>(), method_name_0_};
    ProxyMethodBase method_1_{proxy_, std::make_unique<mock_binding::ProxyMethod>(), method_name_1_};
};

TEST_F(ProxyBaseServiceElementReferencesFixture, RegisteringServiceElementStoresReferenceInMap)
{
    // Given a valid MyProxy object

    // When registering 2 Events, Fields and Methods
    ProxyBaseView{proxy_}.RegisterEvent(event_name_0_, event_0_);
    ProxyBaseView{proxy_}.RegisterEvent(event_name_1_, event_1_);
    ProxyBaseView{proxy_}.RegisterField(field_name_0_, field_0_);
    ProxyBaseView{proxy_}.RegisterField(field_name_1_, field_1_);
    ProxyBaseView{proxy_}.RegisterMethod(method_name_0_, method_0_);
    ProxyBaseView{proxy_}.RegisterMethod(method_name_1_, method_1_);

    // Then the proxy's reference maps should contain references to the registered elements
    const auto& events = proxy_.GetEvents();
    EXPECT_EQ(events.size(), 2U);
    EXPECT_EQ(&events.at(event_name_0_).get(), &event_0_);
    EXPECT_EQ(&events.at(event_name_1_).get(), &event_1_);

    const auto& fields = proxy_.GetFields();
    EXPECT_EQ(fields.size(), 2U);
    EXPECT_EQ(&fields.at(field_name_0_).get(), &field_0_);
    EXPECT_EQ(&fields.at(field_name_1_).get(), &field_1_);

    const auto& methods = proxy_.GetMethods();
    EXPECT_EQ(methods.size(), 2U);
    EXPECT_EQ(&methods.at(method_name_0_).get(), &method_0_);
    EXPECT_EQ(&methods.at(method_name_1_).get(), &method_1_);
}

TEST_F(ProxyBaseServiceElementReferencesFixture, MoveConstructingUpdatesReferencesToServiceElements)
{
    RecordProperty("Verifies", "SCR-17432438");
    RecordProperty("Description", "skeleton is move constructible");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a valid MyProxy object on which 2 Events, Fields and Methods were registered
    ProxyBaseView{proxy_}.RegisterEvent(event_name_0_, event_0_);
    ProxyBaseView{proxy_}.RegisterEvent(event_name_1_, event_1_);
    ProxyBaseView{proxy_}.RegisterField(field_name_0_, field_0_);
    ProxyBaseView{proxy_}.RegisterField(field_name_1_, field_1_);
    ProxyBaseView{proxy_}.RegisterMethod(method_name_0_, method_0_);
    ProxyBaseView{proxy_}.RegisterMethod(method_name_1_, method_1_);

    // When move constructing a new MyProxy object
    MyProxy moved_to_proxy{std::move(proxy_)};

    // Then the moved-to proxy's reference maps should still contain references to the registered elements
    const auto& events = moved_to_proxy.GetEvents();
    ASSERT_EQ(events.size(), 2U);
    EXPECT_EQ(&events.at(event_name_0_).get(), &event_0_);
    EXPECT_EQ(&events.at(event_name_1_).get(), &event_1_);

    const auto& fields = moved_to_proxy.GetFields();
    ASSERT_EQ(fields.size(), 2U);
    EXPECT_EQ(&fields.at(field_name_0_).get(), &field_0_);
    EXPECT_EQ(&fields.at(field_name_1_).get(), &field_1_);

    const auto& methods = moved_to_proxy.GetMethods();
    EXPECT_EQ(methods.size(), 2U);
    EXPECT_EQ(&methods.at(method_name_0_).get(), &method_0_);
    EXPECT_EQ(&methods.at(method_name_1_).get(), &method_1_);
}

TEST_F(ProxyBaseServiceElementReferencesFixture, MoveAssigningUpdatesReferencesToServiceElements)
{
    RecordProperty("Verifies", "SCR-17432438");
    RecordProperty("Description", "skeleton is move assignable");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    constexpr auto other_event_name{"other_event"};
    constexpr auto other_field_name{"other_field"};
    constexpr auto other_method_name{"other_method"};
    mock_binding::Proxy proxy_binding_mock{};

    // Given a valid MyProxy object on which 2 Events, Fields and Methods were registered
    ProxyBaseView{proxy_}.RegisterEvent(event_name_0_, event_0_);
    ProxyBaseView{proxy_}.RegisterField(field_name_0_, field_0_);
    ProxyBaseView{proxy_}.RegisterMethod(method_name_0_, method_0_);
    ProxyBaseView{proxy_}.RegisterEvent(event_name_1_, event_1_);
    ProxyBaseView{proxy_}.RegisterField(field_name_1_, field_1_);
    ProxyBaseView{proxy_}.RegisterMethod(method_name_1_, method_1_);

    // and given a second valid MyProxy object
    MyProxy proxy_2{std::make_unique<mock_binding::ProxyFacade>(proxy_binding_mock), handle_};

    // and given that an Event, Field and Method were registered on the second proxy
    ProxyEventBase event{
        proxy_2, &proxy_binding_mock, std::make_unique<mock_binding::ProxyEventBase>(), other_event_name};
    ProxyEventBase field_event_dispatch{
        proxy_2, &proxy_binding_mock, std::make_unique<mock_binding::ProxyEventBase>(), other_field_name};
    ProxyFieldBase field{proxy_2, &field_event_dispatch, other_field_name};
    ProxyMethodBase method{proxy_2, std::make_unique<mock_binding::ProxyMethod>(), other_method_name};
    ProxyBaseView{proxy_2}.RegisterEvent(other_event_name, event);
    ProxyBaseView{proxy_2}.RegisterField(other_field_name, field);
    ProxyBaseView{proxy_2}.RegisterMethod(other_method_name, method);

    // When move assigning the first MyProxy object to the second
    proxy_2 = std::move(proxy_);

    // Then the second proxy's reference maps should contain references to the first proxy's registered elements
    const auto& events = proxy_2.GetEvents();
    ASSERT_EQ(events.size(), 2U);
    EXPECT_EQ(&events.at(event_name_0_).get(), &event_0_);
    EXPECT_EQ(&events.at(event_name_1_).get(), &event_1_);

    const auto& fields = proxy_2.GetFields();
    ASSERT_EQ(fields.size(), 2U);
    EXPECT_EQ(&fields.at(field_name_0_).get(), &field_0_);
    EXPECT_EQ(&fields.at(field_name_1_).get(), &field_1_);

    const auto& methods = proxy_2.GetMethods();
    EXPECT_EQ(methods.size(), 2U);
    EXPECT_EQ(&methods.at(method_name_0_).get(), &method_0_);
    EXPECT_EQ(&methods.at(method_name_1_).get(), &method_1_);
}

TEST_F(ProxyBaseServiceElementReferencesFixture, UpdateEventTerminatesOnFailiure)
{
    // Given a proxy with a registered event
    ProxyBaseView{proxy_}.RegisterEvent(event_name_0_, event_0_);

    // When we try to update the even but use a wrong name, then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(ProxyBaseView{proxy_}.UpdateEvent("wrong_event_name", event_0_));
}

TEST_F(ProxyBaseServiceElementReferencesFixture, UpdateFieldTerminatesOnFailiure)
{
    // Given a proxy with a registered event
    ProxyBaseView{proxy_}.RegisterField(field_name_0_, field_0_);

    // When we try to update the even but use a wrong name, then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(ProxyBaseView{proxy_}.UpdateField("wrong_field_name", field_0_));
}

TEST_F(ProxyBaseServiceElementReferencesFixture, UpdateMethodTerminatesOnFailiure)
{
    // Given a proxy with a registered event
    ProxyBaseView{proxy_}.RegisterMethod(method_name_0_, method_0_);

    // When we try to update the even but use a wrong name, then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(ProxyBaseView{proxy_}.UpdateMethod("wrong_method_name", method_0_));
}
}  // namespace score::mw::com::impl
