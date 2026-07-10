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
#include "score/mw/com/impl/plumbing/proxy_method_binding_factory.h"
#include "score/mw/com/impl/bindings/lola/test/proxy_event_test_resources.h"
#include "score/mw/com/impl/bindings/mock_binding/proxy.h"
#include "score/mw/com/impl/configuration/lola_event_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_field_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/configuration/service_identifier_type.h"
#include "score/mw/com/impl/configuration/service_instance_id.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/plumbing/proxy_method_binding_factory_impl.h"
#include "score/mw/com/impl/test/dummy_instance_identifier_builder.h"

#include <score/assert_support.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <utility>

namespace score::mw::com::impl
{

using namespace ::testing;

using TestSampleType = std::uint8_t;

constexpr auto kDummyMethodName{"Method1"};
constexpr std::uint16_t kDummyMethodId{5U};

constexpr uint16_t kInstanceId = 0x31U;
const LolaServiceId kServiceId{1U};
const auto kInstanceSpecifier = InstanceSpecifier::Create(std::string{"/my_dummy_instance_specifier"}).value();

const auto kQueueSize{23U};

const LolaServiceInstanceDeployment kLolaServiceInstanceDeployment{
    LolaServiceInstanceId{kInstanceId},
    {},
    {},
    {{kDummyMethodName, LolaMethodInstanceDeployment{kQueueSize, true}}}};

const LolaServiceTypeDeployment kLolaServiceTypeDeployment{kServiceId, {}, {}, {{kDummyMethodName, kDummyMethodId}}};

constexpr auto kQualityType = QualityType::kASIL_B;
ConfigurationStore kConfigStoreAsilB{kInstanceSpecifier,
                                     make_ServiceIdentifierType("/a/service/somewhere/out/there", 13U, 37U),
                                     kQualityType,
                                     kLolaServiceTypeDeployment,
                                     kLolaServiceInstanceDeployment};

constexpr auto kDummyFieldName{"Field1"};
constexpr std::uint16_t kDummyFieldId{6U};
const auto kFieldInstanceSpecifier = InstanceSpecifier::Create(std::string{"/my_field_instance_specifier"}).value();

const LolaServiceInstanceDeployment kLolaServiceInstanceDeploymentWithField{
    LolaServiceInstanceId{kInstanceId},
    {},
    {{kDummyFieldName, LolaFieldInstanceDeployment{LolaEventInstanceDeployment{{1U}, {3U}, 1U, true, 0}, true, true}}},
    {{kDummyFieldName, LolaMethodInstanceDeployment{kQueueSize, true}}}};

const LolaServiceTypeDeployment kLolaServiceTypeDeploymentWithField{kServiceId,
                                                                    {},
                                                                    {{kDummyFieldName, kDummyFieldId}},
                                                                    {{kDummyMethodName, kDummyMethodId}}};

ConfigurationStore kConfigStoreWithFieldAsilB{kFieldInstanceSpecifier,
                                              make_ServiceIdentifierType("/a/service/somewhere/out/there", 13U, 37U),
                                              kQualityType,
                                              kLolaServiceTypeDeploymentWithField,
                                              kLolaServiceInstanceDeploymentWithField};

class ProxyMethodFactoryFixture : public lola::ProxyMockedMemoryFixture
{

  public:
    HandleType GetValidLoLaHandle()
    {
        return kConfigStoreAsilB.GetHandle();
    }

    HandleType GetValidLoLaHandleWithField()
    {
        return kConfigStoreWithFieldAsilB.GetHandle();
    }

    HandleType GetBlankBindingHandle()
    {
        const auto instance_identifier = dummy_instance_identifier_builder_.CreateBlankBindingInstanceIdentifier();
        return make_HandleType(instance_identifier, ServiceInstanceId{LolaServiceInstanceId{kInstanceId}});
    }

  private:
    DummyInstanceIdentifierBuilder dummy_instance_identifier_builder_{};
};

template <typename MethodType>
class ProxyMethodFactoryTypedFixture : public ProxyMethodFactoryFixture
{
};

using RegisteredFunctionTypes = ::testing::
    Types<void(int), void(const double, int), void(), int(), void(), std::uint8_t(std::uint64_t, int, float)>;

TYPED_TEST_SUITE(ProxyMethodFactoryTypedFixture, RegisteredFunctionTypes, );

TYPED_TEST(ProxyMethodFactoryTypedFixture, CanConstructLolaProxyMethod)
{
    // Given a valid lola binding
    const auto handle = this->GetValidLoLaHandle();
    this->InitialiseProxyWithConstructor(handle.GetInstanceIdentifier());

    // When creating a ProxyMethod using MethodBindingFactory
    using MethodSignature = TypeParam;
    auto proxy_method = ProxyMethodBindingFactory<MethodSignature>::Create(
        handle, *this->proxy_, kDummyMethodName, MethodType::kMethod);

    // Then a valid binding can be created
    ASSERT_TRUE(proxy_method.has_value());
    ASSERT_NE(proxy_method.value(), nullptr);
}

TYPED_TEST(ProxyMethodFactoryTypedFixture, CanConstructFieldGetAndSetMethods)
{
    // Given a valid lola binding whose deployment carries a field named kDummyFieldName
    const auto handle = this->GetValidLoLaHandleWithField();
    this->InitialiseProxyWithConstructor(handle.GetInstanceIdentifier());
    using MethodSignature = TypeParam;

    // When creating a binding for both the Get and the Set of that field
    for (const auto method_type : {MethodType::kGet, MethodType::kSet})
    {
        auto proxy_method =
            ProxyMethodBindingFactory<MethodSignature>::Create(handle, *this->proxy_, kDummyFieldName, method_type);

        // Then a valid binding is created
        ASSERT_TRUE(proxy_method.has_value());
        ASSERT_NE(proxy_method.value(), nullptr);
    }
}

TYPED_TEST(ProxyMethodFactoryTypedFixture, ConstructingLolaMethodBindingWhichIsDisabledInConfigurationReturnsNullptr)
{
    // Given a valid lola binding with a method which is disabled in the configuration
    const LolaServiceInstanceDeployment lola_service_instance_deployment_disabled_method{
        LolaServiceInstanceId{kInstanceId},
        {},
        {},
        {{kDummyMethodName, LolaMethodInstanceDeployment{kQueueSize, false}}}};
    ConfigurationStore config_store_asil_b_disabled_method{
        kInstanceSpecifier,
        make_ServiceIdentifierType("/a/service/somewhere/out/there", 13U, 37U),
        kQualityType,
        kLolaServiceTypeDeployment,
        lola_service_instance_deployment_disabled_method};

    const auto handle = config_store_asil_b_disabled_method.GetHandle();
    this->InitialiseProxyWithConstructor(handle.GetInstanceIdentifier());

    // When creating a ProxyMethod using MethodBindingFactory
    using MethodSignature = TypeParam;
    auto proxy_method = ProxyMethodBindingFactory<MethodSignature>::Create(
        handle, *this->proxy_, kDummyMethodName, MethodType::kMethod);

    // Then a null pointer is returned
    ASSERT_TRUE(proxy_method.has_value());
    EXPECT_EQ(proxy_method.value(), nullptr);
}

TYPED_TEST(ProxyMethodFactoryTypedFixture, ConstructingLolaMethodBindingWithInstanceDeploymentWithoutMethodTerminates)
{
    // Given a handle to a valid lola deployment which does not contain the method
    const LolaServiceInstanceDeployment lola_service_instance_deployment_without_method{
        LolaServiceInstanceId{kInstanceId}, {}, {}, {}};
    ConfigurationStore config_store_without_method{
        kInstanceSpecifier,
        make_ServiceIdentifierType("/a/service/somewhere/out/there", 13U, 37U),
        kQualityType,
        kLolaServiceTypeDeployment,
        lola_service_instance_deployment_without_method};
    const auto handle = config_store_without_method.GetHandle();
    this->InitialiseProxyWithConstructor(handle.GetInstanceIdentifier());

    // when creating a ProxyMethod using MethodBindingFactory
    // Then the program terminates
    using MethodSignature = TypeParam;
    EXPECT_DEATH(score::cpp::ignore = ProxyMethodBindingFactory<MethodSignature>::Create(
                     handle, *this->proxy_, kDummyMethodName, MethodType::kMethod),
                 ".*");
}

TYPED_TEST(ProxyMethodFactoryTypedFixture, CannotConstructEventFromBlankBinding)
{
    const auto handle = this->GetBlankBindingHandle();

    // Given a blank binding
    mock_binding::Proxy proxy_binding_mock{};

    // When creating a ProxyMethod using MethodBindingFactory
    using MethodSignature = TypeParam;
    auto proxy_method = ProxyMethodBindingFactory<MethodSignature>::Create(
        handle, proxy_binding_mock, kDummyMethodName, MethodType::kMethod);

    // Then an error is returned
    ASSERT_FALSE(proxy_method.has_value());
    EXPECT_EQ(proxy_method.error(), BindingFactoryErrorCode::kUnsupportedBindingType);
}

TYPED_TEST(ProxyMethodFactoryTypedFixture, GetQueueSizeReturnsValueForMethodInLolaDeployment)
{
    // Given a handle to a valid lola deployment which contains a method
    const auto handle = this->GetValidLoLaHandle();

    // when GetQueueSize is called with a method name that exists in the lola deployment
    auto queue_size = detail::GetQueueSize(handle, kDummyMethodName, MethodType::kMethod);

    // Then the correct que_size is returned and no crush occures
    ASSERT_EQ(queue_size, kQueueSize);
}

TYPED_TEST(ProxyMethodFactoryTypedFixture, GetQueueSizeReturnsOneForFieldGetMethod)
{
    // Given a handle to a valid lola deployment
    const auto handle = this->GetValidLoLaHandle();

    // When GetQueueSize is called with MethodType::kGet, the method_name argument is not consulted
    // because field Get/Set are synchronous and fixed at queue size 1.
    auto queue_size = detail::GetQueueSize(handle, "AnyFieldName", MethodType::kGet);

    ASSERT_EQ(queue_size, 1U);
}

TYPED_TEST(ProxyMethodFactoryTypedFixture, GetQueueSizeReturnsOneForFieldSetMethod)
{
    // Given a handle to a valid lola deployment
    const auto handle = this->GetValidLoLaHandle();

    // Same as above for MethodType::kSet.
    auto queue_size = detail::GetQueueSize(handle, "AnyFieldName", MethodType::kSet);

    ASSERT_EQ(queue_size, 1U);
}

TYPED_TEST(ProxyMethodFactoryTypedFixture, GetQueueSizeTerminatesForMethodNotInLolaDeployment)
{
    // Given a handle to a valid lola deployment which contains a method
    const auto handle = this->GetValidLoLaHandle();

    // when GetQueueSize is called with a method name which does not exist in the lola deployment
    auto wrong_name = "ThisMethodDoesNotExist";

    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_ASSERT_CONTRACT_VIOLATED(
        score::cpp::ignore = detail::GetQueueSize(handle, wrong_name, MethodType::kMethod));
}

TYPED_TEST(ProxyMethodFactoryTypedFixture, ConstructingLolaMethodBindingWithUnknownMethodTypeTerminates)
{
    // Given a valid lola binding
    const auto handle = this->GetValidLoLaHandle();
    this->InitialiseProxyWithConstructor(handle.GetInstanceIdentifier());

    // When creating a ProxyMethod using MethodBindingFactory with MethodType::kUnknown
    // Then the program terminates
    using MethodSignature = TypeParam;
    EXPECT_DEATH(score::cpp::ignore = ProxyMethodBindingFactory<MethodSignature>::Create(
                     handle, *this->proxy_, kDummyMethodName, MethodType::kUnknown),
                 ".*");
}

TYPED_TEST(ProxyMethodFactoryTypedFixture, GetQueueSizeTerminatesForMethodInLolaDeploymentWithoutQueueSize)
{
    // Given a handle to a valid lola deployment which contains a method with empty QueueSize
    const LolaServiceInstanceDeployment lola_service_instance_deployment_with_empty_queue_size{
        LolaServiceInstanceId{kInstanceId},
        {},
        {},
        {{kDummyMethodName, LolaMethodInstanceDeployment{std::nullopt, true}}}};
    ConfigurationStore config_store_with_empty_queue_size_asil_b{
        kInstanceSpecifier,
        make_ServiceIdentifierType("/a/service/somewhere/out/there", 13U, 37U),
        kQualityType,
        kLolaServiceTypeDeployment,
        lola_service_instance_deployment_with_empty_queue_size};
    const auto handle = config_store_with_empty_queue_size_asil_b.GetHandle();

    // when GetQueueSize is called with the method name with empty QueueSize
    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_ASSERT_CONTRACT_VIOLATED(
        score::cpp::ignore = detail::GetQueueSize(handle, kDummyMethodName, MethodType::kMethod));
}

}  // namespace score::mw::com::impl
