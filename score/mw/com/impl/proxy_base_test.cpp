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
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/configuration/service_instance_deployment.h"
#include "score/mw/com/impl/configuration/service_type_deployment.h"
#include "score/mw/com/impl/find_service_handle.h"
#include "score/mw/com/impl/find_service_handler.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/impl/runtime_mock.h"
#include "score/mw/com/impl/scoped_event_receive_handler.h"
#include "score/mw/com/impl/service_discovery_mock.h"

#include "score/language/safecpp/scoped_function/scope.h"

#include <score/utility.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <utility>
#include <vector>

namespace score::mw::com::impl
{

using ::testing::_;
using ::testing::ByMove;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::StrictMock;

const auto kServiceSpecifierString = "abc/abc/TirePressurePort";
const auto kInstanceSpecifier = InstanceSpecifier::Create(kServiceSpecifierString).value();
const auto kServiceIdentifier = make_ServiceIdentifierType("foo", 13, 37);
LolaServiceInstanceId kLolaInstanceId{23U};
ServiceInstanceId kInstanceId{kLolaInstanceId};
const ServiceInstanceDeployment kDeploymentInfo{kServiceIdentifier,
                                                LolaServiceInstanceDeployment{kLolaInstanceId},
                                                QualityType::kASIL_QM,
                                                kInstanceSpecifier};
std::uint16_t kServiceId{34U};
const ServiceTypeDeployment kTypeDeployment{LolaServiceTypeDeployment{kServiceId}};
const auto kInstanceIdWithLolaBinding = make_InstanceIdentifier(kDeploymentInfo, kTypeDeployment);

class RuntimeMockGuard
{
  public:
    RuntimeMockGuard()
    {
        Runtime::InjectMock(&runtime_mock_);
    }
    ~RuntimeMockGuard()
    {
        Runtime::InjectMock(nullptr);
    }

    RuntimeMock runtime_mock_;
};

class ProxyBaseFixture : public ::testing::Test
{
  public:
    ProxyBaseFixture()
    {
        ON_CALL(runtime_mock_guard_.runtime_mock_, GetServiceDiscovery())
            .WillByDefault(testing::ReturnRef(service_discovery_mock_));
        ON_CALL(service_discovery_mock_, StopFindService(_)).WillByDefault(Return(ResultBlank{}));
    }

    ProxyBaseFixture& WithAProxyBaseWithMockBinding(
        const HandleType& handle_type = make_HandleType(kInstanceIdWithLolaBinding, kInstanceId))
    {
        base_proxy_ = std::make_unique<ProxyBase>(std::move(proxy_binding_mock_), handle_type);
        return *this;
    }

    ScopedEventReceiveHandler CreateMockScopedEventReceiveHandler(MockFunction<void()>& mock_function)
    {
        return ScopedEventReceiveHandler(event_receive_handler_scope_, mock_function.AsStdFunction());
    }

    RuntimeMockGuard runtime_mock_guard_{};
    ServiceDiscoveryMock service_discovery_mock_{};

    std::unique_ptr<mock_binding::Proxy> proxy_binding_mock_{nullptr};
    std::unique_ptr<ProxyBase> base_proxy_{nullptr};

    safecpp::Scope<> event_receive_handler_scope_{};
};

TEST_F(ProxyBaseFixture, CanConstructProxyBaseWithABindingContainingNullptr)
{
    // Given a handle to a service with a lola binding
    auto handle = make_HandleType(kInstanceIdWithLolaBinding, kInstanceId);

    // When constructing a Proxybase with a binding that is a nullptr nothing bad happens
    ProxyBase proxy_base{nullptr, handle};
}

TEST_F(ProxyBaseFixture, GetImplReturnsProxyBindingPassedToConstructor)
{
    // Given a ProxyBase with a mock binding and valid handle
    WithAProxyBaseWithMockBinding();

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
    auto handle = make_HandleType(kInstanceIdWithLolaBinding, kInstanceId);
    WithAProxyBaseWithMockBinding(handle);

    // When getting the handle from the ProxyBase
    auto returned_handle = base_proxy_->GetHandle();

    // Then the returned handle will be the same handle provided to the constructor of ProxyBase
    EXPECT_EQ(returned_handle, handle);
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
    auto expected_handle = make_HandleType(kInstanceIdWithLolaBinding, kInstanceId);
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
    auto expected_handle = make_HandleType(kInstanceIdWithLolaBinding, kInstanceId);
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
    EXPECT_CALL(service_discovery_mock_, StartFindService(_, kInstanceIdWithLolaBinding)).WillOnce(Return(handle));

    // When calling StartFindService
    score::cpp::ignore = ProxyBase::StartFindService([](auto, auto) noexcept {}, kInstanceIdWithLolaBinding);
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
    ON_CALL(service_discovery_mock_, StartFindService(_, kInstanceIdWithLolaBinding)).WillByDefault(Return(handle));

    // When calling StartFindService
    auto find_service_handle_result =
        ProxyBase::StartFindService([](auto, auto) noexcept {}, kInstanceIdWithLolaBinding);

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
    ON_CALL(service_discovery_mock_, StartFindService(_, kInstanceIdWithLolaBinding))
        .WillByDefault(Return(Unexpected{ComErrc::kNotOffered}));

    // When calling StartFindService
    auto find_service_handle_result =
        ProxyBase::StartFindService([](auto, auto) noexcept {}, kInstanceIdWithLolaBinding);

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
    auto expected_handle = make_HandleType(kInstanceIdWithLolaBinding, kInstanceId);
    EXPECT_CALL(service_discovery_mock_, FindService(kInstanceIdWithLolaBinding))
        .WillOnce(Return(Result<std::vector<HandleType>>{{expected_handle}}));

    // When calling FindService with the instance identifier
    const Result<ServiceHandleContainer<HandleType>> handles_result =
        ProxyBase::FindService(kInstanceIdWithLolaBinding);

    // Then the service can be found
    ASSERT_TRUE(handles_result.has_value());
    const auto& handles = handles_result.value();
    ASSERT_EQ(handles.size(), 1);

    // And the returned handle is derived from the instance identifier
    EXPECT_EQ(handles[0], expected_handle);
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
    auto expected_handle = make_HandleType(kInstanceIdWithLolaBinding, kInstanceId);
    EXPECT_CALL(service_discovery_mock_, FindService(kInstanceIdWithLolaBinding))
        .WillOnce(Return(MakeUnexpected(binding_error_code)));

    // When calling FindService with the instance identifier
    const Result<ServiceHandleContainer<HandleType>> handles_result =
        ProxyBase::FindService(kInstanceIdWithLolaBinding);

    // Then an error is returned
    ASSERT_FALSE(handles_result.has_value());

    // And the error code is as expected
    EXPECT_EQ(handles_result.error(), returned_error_code);
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
    const auto instance_with_fake_binding_2 = make_InstanceIdentifier(deployment_info_2, kTypeDeployment);

    const auto service_identifier_3 = make_ServiceIdentifierType("foo3", 15, 39);
    std::uint16_t instance_id_3{33U};
    const ServiceInstanceDeployment deployment_info_3{
        service_identifier_3,
        LolaServiceInstanceDeployment{LolaServiceInstanceId{instance_id_3}},
        QualityType::kASIL_QM,
        kInstanceSpecifier};
    const auto instance_with_fake_binding_3 = make_InstanceIdentifier(deployment_info_3, kTypeDeployment);

    // Given a valid instance identifier with a valid lola binding

    // Expecting that the runtime will return a vector containing 3 instance identifiers
    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, resolve(kInstanceSpecifier))
        .WillOnce(Return(std::vector<InstanceIdentifier>{
            kInstanceIdWithLolaBinding, instance_with_fake_binding_2, instance_with_fake_binding_3}));

    // and that the ServiceDiscovery will return a handles vector containing one error and two valid handles
    // when finding the service
    const auto expected_handle_with_error = MakeUnexpected(ComErrc::kInvalidConfiguration);
    const auto expected_handle_2 = make_HandleType(instance_with_fake_binding_2, kInstanceId);
    const auto expected_handle_3 = make_HandleType(instance_with_fake_binding_3, kInstanceId);
    EXPECT_CALL(service_discovery_mock_, FindService(kInstanceIdWithLolaBinding))
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
    const auto instance_with_fake_binding_2 = make_InstanceIdentifier(deployment_info_2, kTypeDeployment);

    // Given a valid instance identifier with a valid lola binding

    // Expecting that the runtime will return a vector containing 2 instance identifiers
    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, resolve(kInstanceSpecifier))
        .WillOnce(Return(std::vector<InstanceIdentifier>{kInstanceIdWithLolaBinding, instance_with_fake_binding_2}));

    // and that the ServiceDiscovery will return a handles vector containing one error and one handle
    // when finding the service
    const auto expected_handle_with_error = MakeUnexpected(ComErrc::kInvalidConfiguration);
    const auto expected_handle_with_error_2 = MakeUnexpected(ComErrc::kInvalidConfiguration);
    EXPECT_CALL(service_discovery_mock_, FindService(kInstanceIdWithLolaBinding))
        .WillOnce(Return(Result<std::vector<HandleType>>{expected_handle_with_error}));
    EXPECT_CALL(service_discovery_mock_, FindService(kInstanceIdWithLolaBinding))
        .WillOnce(Return(Result<std::vector<HandleType>>{expected_handle_with_error_2}));

    // When finding a specific service
    const Result<ServiceHandleContainer<HandleType>> handles_result = ProxyBase::FindService(kInstanceSpecifier);

    // Then an error is returned
    ASSERT_FALSE(handles_result.has_value());
    EXPECT_EQ(handles_result.error(), ComErrc::kBindingFailure);
}

}  // namespace score::mw::com::impl
