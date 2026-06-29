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
#include "score/mw/com/impl/plumbing/binding_factory_error.h"
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

#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace score::mw::com::impl
{

using ::testing::_;
using ::testing::ByMove;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::StrictMock;

const auto kInstanceSpecifier = InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"}).value();
const auto kServiceIdentifier = make_ServiceIdentifierType("foo", 13, 37);
const LolaServiceInstanceId kLolaInstanceId{23U};
constexpr std::uint16_t kServiceId{34U};

const ConfigurationStore kConfigStore{kInstanceSpecifier,
                                      kServiceIdentifier,
                                      QualityType::kASIL_QM,
                                      kServiceId,
                                      kLolaInstanceId};

static constexpr auto kEventName = "event_name";
static constexpr auto kFieldEventName = "field_event_name";
static constexpr auto kFieldSetterName = "field_setter_name";
static constexpr auto kFieldGetterName = "field_getter_name";
static constexpr auto kFieldName = "field_name";
static constexpr auto kMethodName = "method_name";
static constexpr auto kMethodName2 = "method_name_2";

class ProxyBaseFixture : public ::testing::Test
{
  public:
    ProxyBaseFixture()
    {
        ON_CALL(runtime_mock_guard_.runtime_mock_, GetServiceDiscovery())
            .WillByDefault(testing::ReturnRef(service_discovery_mock_));
        ON_CALL(service_discovery_mock_, StopFindService(_)).WillByDefault(Return(Result<void>{}));
    }

    ProxyBaseFixture& WithAProxyBaseWithMockBinding(const HandleType& handle_type)
    {
        base_proxy_ =
            std::make_unique<ProxyBase>(std::make_unique<mock_binding::ProxyFacade>(proxy_binding_mock_), handle_type);
        return *this;
    }

    ScopedEventReceiveHandler CreateMockScopedEventReceiveHandler(MockFunction<void()>& mock_function)
    {
        return ScopedEventReceiveHandler(event_receive_handler_scope_, mock_function.AsStdFunction());
    }

    InstanceIdentifier instance_identifier_{kConfigStore.GetInstanceIdentifier()};
    HandleType handle_{kConfigStore.GetHandle()};

    RuntimeMockGuard runtime_mock_guard_{};
    ServiceDiscoveryMock service_discovery_mock_{};

    mock_binding::Proxy proxy_binding_mock_{};
    std::unique_ptr<ProxyBase> base_proxy_{nullptr};

    safecpp::Scope<> event_receive_handler_scope_{};
};

TEST_F(ProxyBaseFixture, ConstructingProxyBaseWithABindingContainingNullptrTerminates)
{
    // Given a handle to a service with a lola binding

    // When constructing a Proxybase with a binding that is a nullptr
    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED((ProxyBase{nullptr, handle_}));
}

TEST_F(ProxyBaseFixture, GetImplReturnsProxyBindingPassedToConstructor)
{
    // Given a ProxyBase with a mock binding and valid handle
    WithAProxyBaseWithMockBinding(handle_);

    // Expecting that IsEventProvided is called on the Proxy binding that was passed to the constructor
    EXPECT_CALL(proxy_binding_mock_, IsEventProvided("test_event")).WillOnce(Return(true));

    // When getting the proxy binding
    auto& proxy_binding = ProxyBaseView{*base_proxy_}.GetBinding();

    // Then the returned binding will be the same one that was provided to the constructor (checked by calling a
    // function on the returned binding and checking that the mock was called)
    proxy_binding.IsEventProvided("test_event");
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
    auto expected_handle = kConfigStore.GetHandle();
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
    auto expected_handle = kConfigStore.GetHandle();
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
    EXPECT_CALL(service_discovery_mock_, StopFindService(handle)).WillOnce(Return(Result<void>{}));

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
        make_InstanceIdentifier(deployment_info_2, ServiceTypeDeployment{kConfigStore.lola_service_type_deployment_});

    const auto service_identifier_3 = make_ServiceIdentifierType("foo3", 15, 39);
    std::uint16_t instance_id_3{33U};
    const ServiceInstanceDeployment deployment_info_3{
        service_identifier_3,
        LolaServiceInstanceDeployment{LolaServiceInstanceId{instance_id_3}},
        QualityType::kASIL_QM,
        kInstanceSpecifier};
    const auto instance_with_fake_binding_3 =
        make_InstanceIdentifier(deployment_info_3, ServiceTypeDeployment{kConfigStore.lola_service_type_deployment_});

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
        make_InstanceIdentifier(deployment_info_2, ServiceTypeDeployment{kConfigStore.lola_service_type_deployment_});

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

// Since ProxyMethodBase is an abstract class, we create a dummy ProxyMethod which mocks the pure virtual method. We use
// this class instead of a real ProxyMethod since we want to explicitly call RegisterMethod in the tests, and
// ProxyMethod already calls this on construction.
class DummyProxyMethod : public ProxyMethodBase
{
  public:
    using ProxyMethodBase::ProxyMethodBase;

    MOCK_METHOD(Result<void>, InitializeInArgsAndReturnValues, (ProxyBinding&), (override));
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

    ProxyEventBase event_0_{event_name_0_, std::make_unique<mock_binding::ProxyEventBase>()};
    ProxyEventBase event_1_{event_name_1_, std::make_unique<mock_binding::ProxyEventBase>()};

    ProxyEventBase field_event_dispatch_0_{field_name_0_, std::make_unique<mock_binding::ProxyEventBase>()};
    ProxyEventBase field_event_dispatch_1_{field_name_1_, std::make_unique<mock_binding::ProxyEventBase>()};
    DummyProxyMethod field_setter_dispatch_0_{method_name_0_,
                                              std::make_unique<mock_binding::ProxyMethod>(),
                                              MethodType::kSet};
    DummyProxyMethod field_getter_dispatch_0_{method_name_0_,
                                              std::make_unique<mock_binding::ProxyMethod>(),
                                              MethodType::kGet};
    DummyProxyMethod field_setter_dispatch_1_{method_name_1_,
                                              std::make_unique<mock_binding::ProxyMethod>(),
                                              MethodType::kSet};
    DummyProxyMethod field_getter_dispatch_1_{method_name_1_,
                                              std::make_unique<mock_binding::ProxyMethod>(),
                                              MethodType::kGet};

    ProxyFieldBase field_0_{field_name_0_,
                            &field_event_dispatch_0_,
                            &field_setter_dispatch_0_,
                            &field_getter_dispatch_0_};
    ProxyFieldBase field_1_{field_name_1_,
                            &field_event_dispatch_1_,
                            &field_setter_dispatch_1_,
                            &field_getter_dispatch_1_};

    DummyProxyMethod method_0_{method_name_0_, std::make_unique<mock_binding::ProxyMethod>(), MethodType::kMethod};
    DummyProxyMethod method_1_{method_name_1_, std::make_unique<mock_binding::ProxyMethod>(), MethodType::kMethod};
};

TEST_F(ProxyBaseServiceElementReferencesFixture, RegisteringServiceElementStoresReferenceInMap)
{
    // Given a valid MyProxy object

    // When registering 2 Events, Fields and Methods
    ProxyBaseView{proxy_}.RegisterEvent(event_name_0_, event_0_.GetReferenceToMoveable());
    ProxyBaseView{proxy_}.RegisterEvent(event_name_1_, event_1_.GetReferenceToMoveable());
    ProxyBaseView{proxy_}.RegisterField(field_name_0_, field_0_.GetReferenceToMoveable());
    ProxyBaseView{proxy_}.RegisterField(field_name_1_, field_1_.GetReferenceToMoveable());
    ProxyBaseView{proxy_}.RegisterMethod(method_name_0_, method_0_.GetReferenceToMoveable());
    ProxyBaseView{proxy_}.RegisterMethod(method_name_1_, method_1_.GetReferenceToMoveable());

    // Then the proxy's reference maps should contain references to the registered elements
    const auto& events = proxy_.GetEvents();
    EXPECT_EQ(events.size(), 2U);
    EXPECT_EQ(&events.at(event_name_0_).get().Get(), &event_0_);
    EXPECT_EQ(&events.at(event_name_1_).get().Get(), &event_1_);

    const auto& fields = proxy_.GetFields();
    EXPECT_EQ(fields.size(), 2U);
    EXPECT_EQ(&fields.at(field_name_0_).get().Get(), &field_0_);
    EXPECT_EQ(&fields.at(field_name_1_).get().Get(), &field_1_);

    const auto& methods = proxy_.GetMethods();
    EXPECT_EQ(methods.size(), 2U);
    EXPECT_EQ(&methods.at(method_name_0_).get().Get(), &method_0_);
    EXPECT_EQ(&methods.at(method_name_1_).get().Get(), &method_1_);
}

TEST_F(ProxyBaseServiceElementReferencesFixture, MoveConstructingUpdatesReferencesToServiceElements)
{
    RecordProperty("Verifies", "SCR-17432438");
    RecordProperty("Description", "skeleton is move constructible");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a valid MyProxy object on which 2 Events, Fields and Methods were registered
    ProxyBaseView{proxy_}.RegisterEvent(event_name_0_, event_0_.GetReferenceToMoveable());
    ProxyBaseView{proxy_}.RegisterEvent(event_name_1_, event_1_.GetReferenceToMoveable());
    ProxyBaseView{proxy_}.RegisterField(field_name_0_, field_0_.GetReferenceToMoveable());
    ProxyBaseView{proxy_}.RegisterField(field_name_1_, field_1_.GetReferenceToMoveable());
    ProxyBaseView{proxy_}.RegisterMethod(method_name_0_, method_0_.GetReferenceToMoveable());
    ProxyBaseView{proxy_}.RegisterMethod(method_name_1_, method_1_.GetReferenceToMoveable());

    // When move constructing a new MyProxy object
    MyProxy moved_to_proxy{std::move(proxy_)};

    // Then the moved-to proxy's reference maps should still contain references to the registered elements
    const auto& events = moved_to_proxy.GetEvents();
    ASSERT_EQ(events.size(), 2U);
    EXPECT_EQ(&events.at(event_name_0_).get().Get(), &event_0_);
    EXPECT_EQ(&events.at(event_name_1_).get().Get(), &event_1_);

    const auto& fields = moved_to_proxy.GetFields();
    ASSERT_EQ(fields.size(), 2U);
    EXPECT_EQ(&fields.at(field_name_0_).get().Get(), &field_0_);
    EXPECT_EQ(&fields.at(field_name_1_).get().Get(), &field_1_);

    const auto& methods = moved_to_proxy.GetMethods();
    EXPECT_EQ(methods.size(), 2U);
    EXPECT_EQ(&methods.at(method_name_0_).get().Get(), &method_0_);
    EXPECT_EQ(&methods.at(method_name_1_).get().Get(), &method_1_);
}

TEST_F(ProxyBaseServiceElementReferencesFixture, MoveAssigningUpdatesReferencesToServiceElements)
{
    RecordProperty("Verifies", "SCR-21290799");
    RecordProperty("Description", "Proxy is move assignable");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    constexpr auto other_event_name{"other_event"};
    constexpr auto other_field_name{"other_field"};
    constexpr auto other_method_name{"other_method"};
    mock_binding::Proxy proxy_binding_mock{};

    // Given a valid MyProxy object on which 2 Events, Fields and Methods were registered
    ProxyBaseView{proxy_}.RegisterEvent(event_name_0_, event_0_.GetReferenceToMoveable());
    ProxyBaseView{proxy_}.RegisterField(field_name_0_, field_0_.GetReferenceToMoveable());
    ProxyBaseView{proxy_}.RegisterMethod(method_name_0_, method_0_.GetReferenceToMoveable());
    ProxyBaseView{proxy_}.RegisterEvent(event_name_1_, event_1_.GetReferenceToMoveable());
    ProxyBaseView{proxy_}.RegisterField(field_name_1_, field_1_.GetReferenceToMoveable());
    ProxyBaseView{proxy_}.RegisterMethod(method_name_1_, method_1_.GetReferenceToMoveable());

    // and given a second valid MyProxy object
    MyProxy proxy_2{std::make_unique<mock_binding::ProxyFacade>(proxy_binding_mock), handle_};

    // and given that an Event, Field and Method were registered on the second proxy
    ProxyEventBase event{other_event_name, std::make_unique<mock_binding::ProxyEventBase>()};
    ProxyEventBase field_event_dispatch{other_field_name, std::make_unique<mock_binding::ProxyEventBase>()};
    DummyProxyMethod field_setter_dispatch{
        other_field_name, std::make_unique<mock_binding::ProxyMethod>(), MethodType::kSet};
    DummyProxyMethod field_getter_dispatch{
        other_field_name, std::make_unique<mock_binding::ProxyMethod>(), MethodType::kGet};
    ProxyFieldBase field{other_field_name, &field_event_dispatch, &field_setter_dispatch, &field_getter_dispatch};
    DummyProxyMethod method{other_method_name, std::make_unique<mock_binding::ProxyMethod>(), MethodType::kMethod};
    ProxyBaseView{proxy_2}.RegisterEvent(other_event_name, event.GetReferenceToMoveable());
    ProxyBaseView{proxy_2}.RegisterField(other_field_name, field.GetReferenceToMoveable());
    ProxyBaseView{proxy_2}.RegisterMethod(other_method_name, method.GetReferenceToMoveable());

    // When move assigning the first MyProxy object to the second
    proxy_2 = std::move(proxy_);

    // Then the second proxy's reference maps should contain references to the first proxy's registered elements
    const auto& events = proxy_2.GetEvents();
    ASSERT_EQ(events.size(), 2U);
    EXPECT_EQ(&events.at(event_name_0_).get().Get(), &event_0_);
    EXPECT_EQ(&events.at(event_name_1_).get().Get(), &event_1_);

    const auto& fields = proxy_2.GetFields();
    ASSERT_EQ(fields.size(), 2U);
    EXPECT_EQ(&fields.at(field_name_0_).get().Get(), &field_0_);
    EXPECT_EQ(&fields.at(field_name_1_).get().Get(), &field_1_);

    const auto& methods = proxy_2.GetMethods();
    EXPECT_EQ(methods.size(), 2U);
    EXPECT_EQ(&methods.at(method_name_0_).get().Get(), &method_0_);
    EXPECT_EQ(&methods.at(method_name_1_).get().Get(), &method_1_);
}

TEST_F(ProxyBaseServiceElementReferencesFixture, MoveAssigningToItselfDoesNotDoAnything)
{
    mock_binding::Proxy proxy_binding_mock{};
    // Given a valid MyProxy object
    MyProxy proxy_2{std::make_unique<mock_binding::ProxyFacade>(proxy_binding_mock), handle_};
    // When move assigning the MyProxy object to itself

    auto other_name_same_proxy_p = &proxy_2;
    proxy_2 = std::move(*other_name_same_proxy_p);
    // Then nothing happens.
    // In case of self assignement we would want to know that actually nothing happens and no sideffects occur.
    // Absence of side effects is not possible to test for. This test only validates that the self assignment
    // branch can be taken without crash.
}

// Attorney class which allows us to call the protected SetupMethods method of ProxyBase in the tests.
class ProxyBaseSetupMethodsAttorney : public ProxyBase
{
  public:
    using ProxyBase::ProxyBase;

    Result<void> SetupMethods(const std::size_t additional_shm_size_bytes = 0)
    {
        return ProxyBase::SetupMethods(additional_shm_size_bytes);
    }
};

class ProxyBaseAreBindingsValidFixture : public ::testing::Test
{
  public:
    ProxyBaseAreBindingsValidFixture& GivenAProxyBaseWithValidBinding()
    {
        proxy_base_ = std::make_unique<ProxyBaseSetupMethodsAttorney>(
            std::make_unique<mock_binding::ProxyFacade>(mock_proxy_binding_), kConfigStore.GetHandle());
        return *this;
    }

    ProxyBaseAreBindingsValidFixture& WithAValidEventRegistered()
    {
        event_ = std::make_unique<ProxyEventBase>(kEventName, std::make_unique<mock_binding::ProxyEventBase>());
        ProxyBaseView{*proxy_base_}.RegisterEvent(kEventName, event_->GetReferenceToMoveable());
        return *this;
    }

    ProxyBaseAreBindingsValidFixture& WithAValidFieldRegistered()
    {
        field_event_dispatch_ =
            std::make_unique<ProxyEventBase>(kFieldEventName, std::make_unique<mock_binding::ProxyEventBase>());
        field_setter_dispatch_ = std::make_unique<DummyProxyMethod>(
            kFieldSetterName, std::make_unique<mock_binding::ProxyMethod>(), MethodType::kSet);
        field_getter_dispatch_ = std::make_unique<DummyProxyMethod>(
            kFieldGetterName, std::make_unique<mock_binding::ProxyMethod>(), MethodType::kGet);
        field_ = std::make_unique<ProxyFieldBase>(
            kFieldName, field_event_dispatch_.get(), field_setter_dispatch_.get(), field_getter_dispatch_.get());
        ProxyBaseView{*proxy_base_}.RegisterField(kFieldName, field_->GetReferenceToMoveable());
        return *this;
    }

    ProxyBaseAreBindingsValidFixture& WithAValidMethodRegistered()
    {
        method_ = std::make_unique<DummyProxyMethod>(
            kMethodName, std::make_unique<mock_binding::ProxyMethod>(), MethodType::kMethod);
        ProxyBaseView{*proxy_base_}.RegisterMethod(kMethodName, method_->GetReferenceToMoveable());
        return *this;
    }

    ProxyBaseAreBindingsValidFixture& WithASecondValidMethodRegistered()
    {
        method_2_ = std::make_unique<DummyProxyMethod>(
            kMethodName2, std::make_unique<mock_binding::ProxyMethod>(), MethodType::kMethod);
        ProxyBaseView{*proxy_base_}.RegisterMethod(kMethodName2, method_2_->GetReferenceToMoveable());
        return *this;
    }

    mock_binding::Proxy mock_proxy_binding_;
    std::unique_ptr<ProxyBaseSetupMethodsAttorney> proxy_base_;

    std::unique_ptr<ProxyEventBase> event_{nullptr};

    std::unique_ptr<DummyProxyMethod> method_{nullptr};
    std::unique_ptr<DummyProxyMethod> method_2_{nullptr};

    std::unique_ptr<ProxyEventBase> field_event_dispatch_{nullptr};
    std::unique_ptr<DummyProxyMethod> field_setter_dispatch_{nullptr};
    std::unique_ptr<DummyProxyMethod> field_getter_dispatch_{nullptr};
    std::unique_ptr<ProxyFieldBase> field_{nullptr};
};

TEST_F(ProxyBaseAreBindingsValidFixture, AreBindingsValidReturnsTrueIfAllBindingsAreValid)
{
    GivenAProxyBaseWithValidBinding()
        .WithAValidEventRegistered()
        .WithAValidFieldRegistered()
        .WithAValidMethodRegistered();

    // When calling AreBindingsValid
    const bool are_bindings_valid = ProxyBaseView{*proxy_base_}.AreBindingsValid();

    // Then the result is true
    EXPECT_TRUE(are_bindings_valid);
}

TEST_F(ProxyBaseAreBindingsValidFixture, AreBindingsValidReturnsFalseIfRegisteredEventBindingConstructedWithError)
{
    GivenAProxyBaseWithValidBinding().WithAValidFieldRegistered().WithAValidMethodRegistered();

    // and given that an Event with a null binding was registered
    ProxyEventBase event{kEventName, Unexpected{BindingFactoryErrorCode::kUnsupportedBindingType}};
    ProxyBaseView{*proxy_base_}.RegisterEvent(kEventName, event.GetReferenceToMoveable());

    // When calling AreBindingsValid
    const bool are_bindings_valid = ProxyBaseView{*proxy_base_}.AreBindingsValid();

    // Then the result is false
    EXPECT_FALSE(are_bindings_valid);
}

TEST_F(ProxyBaseAreBindingsValidFixture, AreBindingsValidReturnsFalseIfRegisteredMethodBindingConstructedWithError)
{
    GivenAProxyBaseWithValidBinding().WithAValidEventRegistered().WithAValidFieldRegistered();

    // and given that a Method with a null binding was registered
    DummyProxyMethod method{
        kMethodName, Unexpected{BindingFactoryErrorCode::kUnsupportedBindingType}, MethodType::kMethod};
    ProxyBaseView{*proxy_base_}.RegisterMethod(kMethodName, method.GetReferenceToMoveable());

    // When calling AreBindingsValid
    const bool are_bindings_valid = ProxyBaseView{*proxy_base_}.AreBindingsValid();

    // Then the result is false
    EXPECT_FALSE(are_bindings_valid);
}

TEST_F(ProxyBaseAreBindingsValidFixture, AreBindingsValidReturnsFalseIfRegisteredFieldEventBindingConstructedWithError)
{
    GivenAProxyBaseWithValidBinding().WithAValidEventRegistered().WithAValidMethodRegistered();

    // and given that a Field with a null event binding but valid get / set bindings was registered
    ProxyEventBase invalid_field_event_dispatch{kFieldEventName,
                                                Unexpected{BindingFactoryErrorCode::kUnsupportedBindingType}};
    DummyProxyMethod valid_field_setter_dispatch{
        kFieldSetterName, std::make_unique<mock_binding::ProxyMethod>(), MethodType::kSet};
    DummyProxyMethod valid_field_getter_dispatch{
        kFieldGetterName, std::make_unique<mock_binding::ProxyMethod>(), MethodType::kGet};
    ProxyFieldBase field{
        kFieldName, &invalid_field_event_dispatch, &valid_field_setter_dispatch, &valid_field_getter_dispatch};
    ProxyBaseView{*proxy_base_}.RegisterField(kFieldName, field.GetReferenceToMoveable());

    // When calling AreBindingsValid
    const bool are_bindings_valid = ProxyBaseView{*proxy_base_}.AreBindingsValid();

    // Then the result is false
    EXPECT_FALSE(are_bindings_valid);
}

TEST_F(ProxyBaseAreBindingsValidFixture, AreBindingsValidReturnsFalseIfRegisteredFieldGetBindingConstructedWithError)
{
    GivenAProxyBaseWithValidBinding().WithAValidEventRegistered().WithAValidMethodRegistered();

    // and given that a Field with a null get binding but valid event / set bindings was registered
    ProxyEventBase valid_field_event_dispatch{kFieldEventName, std::make_unique<mock_binding::ProxyEventBase>()};
    DummyProxyMethod invalid_field_getter_dispatch{
        kFieldGetterName, Unexpected{BindingFactoryErrorCode::kUnsupportedBindingType}, MethodType::kGet};
    DummyProxyMethod valid_field_setter_dispatch{
        kFieldSetterName, std::make_unique<mock_binding::ProxyMethod>(), MethodType::kSet};
    ProxyFieldBase field{
        kFieldName, &valid_field_event_dispatch, &valid_field_setter_dispatch, &invalid_field_getter_dispatch};
    ProxyBaseView{*proxy_base_}.RegisterField(kFieldName, field.GetReferenceToMoveable());

    // When calling AreBindingsValid
    const bool are_bindings_valid = ProxyBaseView{*proxy_base_}.AreBindingsValid();

    // Then the result is false
    EXPECT_FALSE(are_bindings_valid);
}

TEST_F(ProxyBaseAreBindingsValidFixture, AreBindingsValidReturnsFalseIfRegisteredFieldSetBindingConstructedWithError)
{
    GivenAProxyBaseWithValidBinding().WithAValidEventRegistered().WithAValidMethodRegistered();

    // and given that a Field with a null set binding but valid event / get bindings was registered
    ProxyEventBase valid_field_event_dispatch{kFieldEventName, std::make_unique<mock_binding::ProxyEventBase>()};
    DummyProxyMethod invalid_field_setter_dispatch{
        kFieldSetterName, Unexpected{BindingFactoryErrorCode::kUnsupportedBindingType}, MethodType::kSet};
    DummyProxyMethod valid_field_getter_dispatch{
        kFieldGetterName, std::make_unique<mock_binding::ProxyMethod>(), MethodType::kGet};
    ProxyFieldBase field{
        kFieldName, &valid_field_event_dispatch, &invalid_field_setter_dispatch, &valid_field_getter_dispatch};
    ProxyBaseView{*proxy_base_}.RegisterField(kFieldName, field.GetReferenceToMoveable());

    // When calling AreBindingsValid
    const bool are_bindings_valid = ProxyBaseView{*proxy_base_}.AreBindingsValid();

    // Then the result is false
    EXPECT_FALSE(are_bindings_valid);
}

using ProxyBaseSetupMethodsFixture = ProxyBaseAreBindingsValidFixture;
TEST_F(ProxyBaseSetupMethodsFixture, DispatchesToBindingSetupMethods)
{
    GivenAProxyBaseWithValidBinding();

    // Expecting that SetupMethods is called on the Proxy binding
    EXPECT_CALL(mock_proxy_binding_, SetupMethods(_)).WillOnce(Return(Result<void>{}));

    // When calling SetupMethods
    const auto result = proxy_base_->SetupMethods();

    // Then the result is valid
    EXPECT_TRUE(result.has_value());
}

TEST_F(ProxyBaseSetupMethodsFixture, PropagatesErrorFromBindingSetupMethods)
{
    GivenAProxyBaseWithValidBinding();

    // Expecting that SetupMethods is called on the Proxy binding and returns an error
    const auto binding_error_code = ComErrc::kCommunicationLinkError;
    EXPECT_CALL(mock_proxy_binding_, SetupMethods(_)).WillOnce(Return(MakeUnexpected(binding_error_code)));

    // When calling SetupMethods
    const auto result = proxy_base_->SetupMethods();

    // Then the result is an error
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), binding_error_code);
}

TEST_F(ProxyBaseSetupMethodsFixture, CallsInitializeOnAllMethodBindings)
{
    GivenAProxyBaseWithValidBinding().WithAValidMethodRegistered().WithASecondValidMethodRegistered();

    // Expecting that Initialize is called on both method bindings and returns a valid result
    EXPECT_CALL(*method_, InitializeInArgsAndReturnValues(_)).WillOnce(Return(Result<void>{}));
    EXPECT_CALL(*method_2_, InitializeInArgsAndReturnValues(_)).WillOnce(Return(Result<void>{}));

    // When calling SetupMethods
    const auto result = proxy_base_->SetupMethods();

    // Then the result is valid
    EXPECT_TRUE(result.has_value());
}

TEST_F(ProxyBaseSetupMethodsFixture, ReturnsValidWhenInitializeReturnsBindingDisabled)
{
    GivenAProxyBaseWithValidBinding().WithAValidMethodRegistered().WithASecondValidMethodRegistered();

    // Expecting that Initialize is called on both method bindings and returns a kMethodBindingDisabled error from the
    // first and a valid result from the second
    EXPECT_CALL(*method_, InitializeInArgsAndReturnValues(_))
        .WillOnce(Return(MakeUnexpected(ComErrc::kMethodBindingDisabled)));
    EXPECT_CALL(*method_2_, InitializeInArgsAndReturnValues(_)).WillOnce(Return(Result<void>{}));

    // When calling SetupMethods
    const auto result = proxy_base_->SetupMethods();

    // Then the result is valid
    EXPECT_TRUE(result.has_value());
}

TEST_F(ProxyBaseSetupMethodsFixture, ReturnsErrorWhenInitializeReturnsErrorOtherThanBindingDisabled)
{
    GivenAProxyBaseWithValidBinding().WithAValidMethodRegistered().WithASecondValidMethodRegistered();

    // Expecting that Initialize is called on the first binding and returns an error other than kBindingDisabled
    const auto binding_error_code = ComErrc::kCommunicationLinkError;
    EXPECT_CALL(*method_, InitializeInArgsAndReturnValues(_)).WillOnce(Return(MakeUnexpected(binding_error_code)));
    EXPECT_CALL(*method_2_, InitializeInArgsAndReturnValues(_)).Times(0);

    // When calling SetupMethods
    const auto result = proxy_base_->SetupMethods();

    // Then the result is an error
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), binding_error_code);
}

}  // namespace score::mw::com::impl
