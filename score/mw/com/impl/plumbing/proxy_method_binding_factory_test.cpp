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
#include "score/mw/com/impl/bindings/lola/methods/proxy_method_instance_identifier.h"
#include "score/mw/com/impl/bindings/lola/proxy_method.h"
#include "score/mw/com/impl/bindings/lola/test/proxy_event_test_resources.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/configuration/service_identifier_type.h"
#include "score/mw/com/impl/configuration/service_instance_id.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/plumbing/proxy_method_binding_factory_impl.h"
#include "score/mw/com/impl/proxy_base.h"
#include "score/mw/com/impl/proxy_binding.h"
#include "score/mw/com/impl/test/dummy_instance_identifier_builder.h"

#include <score/assert_support.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

namespace score::mw::com::impl::lola
{
class ProxyMethodAttorney
{
  public:
    explicit ProxyMethodAttorney(ProxyMethod& method) noexcept : method_{method} {}

    const ProxyMethodInstanceIdentifier& GetProxyMethodInstanceIdentifier() const noexcept
    {
        return method_.proxy_method_instance_identifier_;
    }

  private:
    ProxyMethod& method_;
};
}  // namespace score::mw::com::impl::lola

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
    {{kDummyMethodName, LolaMethodInstanceDeployment{kQueueSize}}}};

const LolaServiceTypeDeployment kLolaServiceTypeDeployment{kServiceId, {}, {}, {{kDummyMethodName, kDummyMethodId}}};

constexpr auto kQualityType = QualityType::kASIL_B;
ConfigurationStore kConfigStoreAsilB{kInstanceSpecifier,
                                     make_ServiceIdentifierType("/a/service/somewhere/out/there", 13U, 37U),
                                     kQualityType,
                                     kLolaServiceTypeDeployment,
                                     kLolaServiceInstanceDeployment};

// Deployment with a field alongside the method, for the kGet / kSet tests.
constexpr auto kDummyFieldName{"Field1"};
constexpr std::uint16_t kDummyFieldId{6U};
const auto kFieldInstanceSpecifier = InstanceSpecifier::Create(std::string{"/my_field_instance_specifier"}).value();

const LolaServiceInstanceDeployment kLolaServiceInstanceDeploymentWithField{
    LolaServiceInstanceId{kInstanceId},
    {},
    {{kDummyFieldName, LolaFieldInstanceDeployment{{1U}, {3U}, 1U, true, 0}}},
    {{kDummyMethodName, LolaMethodInstanceDeployment{kQueueSize}}}};

const LolaServiceTypeDeployment kLolaServiceTypeDeploymentWithField{kServiceId,
                                                                    {},
                                                                    {{kDummyFieldName, kDummyFieldId}},
                                                                    {{kDummyMethodName, kDummyMethodId}}};

ConfigurationStore kConfigStoreWithFieldAsilB{kFieldInstanceSpecifier,
                                              make_ServiceIdentifierType("/a/service/somewhere/out/there", 13U, 37U),
                                              kQualityType,
                                              kLolaServiceTypeDeploymentWithField,
                                              kLolaServiceInstanceDeploymentWithField};

const LolaServiceInstanceDeployment kLolaServiceInstanceDeploymentWithEmptyQueueSize{
    LolaServiceInstanceId{kInstanceId},
    {},
    {},
    {{kDummyMethodName, LolaMethodInstanceDeployment{std::nullopt}}}};

ConfigurationStore kConfigStoreWithEmptyQueueSizeAsilB{
    kInstanceSpecifier,
    make_ServiceIdentifierType("/a/service/somewhere/out/there", 13U, 37U),
    kQualityType,
    kLolaServiceTypeDeployment,
    kLolaServiceInstanceDeploymentWithEmptyQueueSize};

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

    HandleType GetValidSomIpHandle()
    {
        const auto instance_identifier = dummy_instance_identifier_builder_.CreateSomeIpBindingInstanceIdentifier();
        return make_HandleType(instance_identifier, ServiceInstanceId{LolaServiceInstanceId{kInstanceId}});
    }
    HandleType GetBlankBindingHandle()
    {
        const auto instance_identifier = dummy_instance_identifier_builder_.CreateBlankBindingInstanceIdentifier();
        return make_HandleType(instance_identifier, ServiceInstanceId{LolaServiceInstanceId{kInstanceId}});
    }

    ProxyBinding* CreateBindingFromHandle(HandleType handle)

    {
        proxy_base_ = std::make_unique<ProxyBase>(std::move(this->proxy_), handle);
        return ProxyBaseView{*proxy_base_}.GetBinding();
    }

  private:
    std::unique_ptr<ProxyBase> proxy_base_{nullptr};
    DummyInstanceIdentifierBuilder dummy_instance_identifier_builder_{};
};

template <typename MethodType>
class ProxyMethodFactoryTypedFixture : public ProxyMethodFactoryFixture
{
};

using RegisteredFunctionTypes = ::testing::
    Types<void(int), void(const double, int), void(), int(), void(), std::uint8_t(std::uint64_t, int, float)>;

TYPED_TEST_SUITE(ProxyMethodFactoryTypedFixture, RegisteredFunctionTypes, );

TYPED_TEST(ProxyMethodFactoryTypedFixture, CanConstructProxyMethod)
{
    // Given a valid lola binding

    const auto handle = this->GetValidLoLaHandle();
    this->InitialiseProxyWithConstructor(handle.GetInstanceIdentifier());

    auto proxy_binding = this->CreateBindingFromHandle(handle);

    // When creating a ProxyMethod using MethodBindingFactory
    using MethodSignature = TypeParam;
    auto proxy_method = ProxyMethodBindingFactory<MethodSignature>::Create(
        handle, proxy_binding, kDummyMethodName, MethodType::kMethod);

    // Then a valid binding can be created
    ASSERT_NE(proxy_method, nullptr);
}

TYPED_TEST(ProxyMethodFactoryTypedFixture, CannotCreateProxyServiceWhenProxyBindingIsNullptr)
{
    const auto handle = this->GetValidLoLaHandle();

    // Given a null proxy binding
    auto proxy_binding{nullptr};

    // When creating a ProxyMethod using MethodBindingFactory
    using MethodSignature = TypeParam;
    auto proxy_method = ProxyMethodBindingFactory<MethodSignature>::Create(
        handle, proxy_binding, kDummyMethodName, MethodType::kMethod);

    // Then a nullptr is returned
    ASSERT_EQ(proxy_method, nullptr);
}

TYPED_TEST(ProxyMethodFactoryTypedFixture, CannotConstructEventFromSomeIpBinding)
{
    // Given a handle carrying a SomeIp deployment
    const auto handle = this->GetValidSomIpHandle();

    // Given a valid someip binding
    auto proxy_binding = this->CreateBindingFromHandle(handle);

    // When creating a ProxyMethod using MethodBindingFactory
    using MethodSignature = TypeParam;
    auto proxy_method = ProxyMethodBindingFactory<MethodSignature>::Create(
        handle, proxy_binding, kDummyMethodName, MethodType::kMethod);

    // Then a nullptr is returned
    EXPECT_EQ(proxy_method, nullptr);
}

TYPED_TEST(ProxyMethodFactoryTypedFixture, CannotConstructEventFromBlankBinding)
{
    // Given a handle carrying a blank deployment
    const auto handle = this->GetBlankBindingHandle();

    // When Create is called
    auto proxy_binding = this->CreateBindingFromHandle(handle);
    using MethodSignature = TypeParam;
    auto proxy_method = ProxyMethodBindingFactory<MethodSignature>::Create(
        handle, proxy_binding, kDummyMethodName, MethodType::kMethod);

    // Then a nullptr is returned
    EXPECT_EQ(proxy_method, nullptr);
}

TYPED_TEST(ProxyMethodFactoryTypedFixture, CanConstructFieldGetMethod)
{
    // Given a valid lola binding with a field in the deployment
    const auto handle = this->GetValidLoLaHandleWithField();
    this->InitialiseProxyWithConstructor(handle.GetInstanceIdentifier());

    // When Create is called with MethodType::kGet
    auto proxy_binding = this->CreateBindingFromHandle(handle);
    using MethodSignature = TypeParam;
    auto proxy_method =
        ProxyMethodBindingFactory<MethodSignature>::Create(handle, proxy_binding, kDummyFieldName, MethodType::kGet);

    // Then the returned method's id is (field, kGet)
    ASSERT_NE(proxy_method, nullptr);
    auto* const lola_method = dynamic_cast<lola::ProxyMethod*>(proxy_method.get());
    ASSERT_NE(lola_method, nullptr);
    const auto& unique_id =
        lola::ProxyMethodAttorney{*lola_method}.GetProxyMethodInstanceIdentifier().unique_method_identifier;
    EXPECT_EQ(unique_id.method_or_field_id, kDummyFieldId);
    EXPECT_EQ(unique_id.method_type, MethodType::kGet);
}

TYPED_TEST(ProxyMethodFactoryTypedFixture, CanConstructFieldSetMethod)
{
    // Given a valid lola binding with a field in the deployment
    const auto handle = this->GetValidLoLaHandleWithField();
    this->InitialiseProxyWithConstructor(handle.GetInstanceIdentifier());

    // When Create is called with MethodType::kSet
    auto proxy_binding = this->CreateBindingFromHandle(handle);
    using MethodSignature = TypeParam;
    auto proxy_method =
        ProxyMethodBindingFactory<MethodSignature>::Create(handle, proxy_binding, kDummyFieldName, MethodType::kSet);

    // Then the returned method's id is (field, kSet)
    ASSERT_NE(proxy_method, nullptr);
    auto* const lola_method = dynamic_cast<lola::ProxyMethod*>(proxy_method.get());
    ASSERT_NE(lola_method, nullptr);
    const auto& unique_id =
        lola::ProxyMethodAttorney{*lola_method}.GetProxyMethodInstanceIdentifier().unique_method_identifier;
    EXPECT_EQ(unique_id.method_or_field_id, kDummyFieldId);
    EXPECT_EQ(unique_id.method_type, MethodType::kSet);
}

TYPED_TEST(ProxyMethodFactoryTypedFixture, GetAndSetForSameFieldProduceDistinctUniqueIdentifiers)
{
    // Given a valid lola binding with a field in the deployment
    const auto handle = this->GetValidLoLaHandleWithField();
    this->InitialiseProxyWithConstructor(handle.GetInstanceIdentifier());
    auto proxy_binding = this->CreateBindingFromHandle(handle);
    using MethodSignature = TypeParam;

    // When Create is called for the same field name with kGet and then kSet
    auto get_method =
        ProxyMethodBindingFactory<MethodSignature>::Create(handle, proxy_binding, kDummyFieldName, MethodType::kGet);
    auto set_method =
        ProxyMethodBindingFactory<MethodSignature>::Create(handle, proxy_binding, kDummyFieldName, MethodType::kSet);

    // Then both succeed, share the field id, and differ only on MethodType
    ASSERT_NE(get_method, nullptr);
    ASSERT_NE(set_method, nullptr);
    auto* const lola_get = dynamic_cast<lola::ProxyMethod*>(get_method.get());
    auto* const lola_set = dynamic_cast<lola::ProxyMethod*>(set_method.get());
    ASSERT_NE(lola_get, nullptr);
    ASSERT_NE(lola_set, nullptr);

    const auto& get_id =
        lola::ProxyMethodAttorney{*lola_get}.GetProxyMethodInstanceIdentifier().unique_method_identifier;
    const auto& set_id =
        lola::ProxyMethodAttorney{*lola_set}.GetProxyMethodInstanceIdentifier().unique_method_identifier;
    EXPECT_EQ(get_id.method_or_field_id, set_id.method_or_field_id);
    EXPECT_NE(get_id.method_type, set_id.method_type);
    EXPECT_EQ(get_id.method_type, MethodType::kGet);
    EXPECT_EQ(set_id.method_type, MethodType::kSet);
}

TYPED_TEST(ProxyMethodFactoryTypedFixture, CannotConstructFieldGetMethodWhenProxyBindingIsNullptr)
{
    // Given a valid lola handle with a field in the deployment
    const auto handle = this->GetValidLoLaHandleWithField();

    // When Create is called with MethodType::kGet and a null parent binding
    ProxyBinding* proxy_binding = nullptr;
    using MethodSignature = TypeParam;
    auto proxy_method =
        ProxyMethodBindingFactory<MethodSignature>::Create(handle, proxy_binding, kDummyFieldName, MethodType::kGet);

    // Then nullptr is returned
    ASSERT_EQ(proxy_method, nullptr);
}

TYPED_TEST(ProxyMethodFactoryTypedFixture, CannotConstructFieldSetMethodWhenProxyBindingIsNullptr)
{
    // Given a valid lola handle with a field in the deployment
    const auto handle = this->GetValidLoLaHandleWithField();

    // When Create is called with MethodType::kSet and a null parent binding
    ProxyBinding* proxy_binding = nullptr;
    using MethodSignature = TypeParam;
    auto proxy_method =
        ProxyMethodBindingFactory<MethodSignature>::Create(handle, proxy_binding, kDummyFieldName, MethodType::kSet);

    // Then nullptr is returned
    ASSERT_EQ(proxy_method, nullptr);
}

TYPED_TEST(ProxyMethodFactoryTypedFixture, GetQueueSizeReturnsValueForMethodInLolaDeployment)
{
    // Given a handle to a valid lola deployment which contains a method
    const auto handle = this->GetValidLoLaHandle();

    // when GetQueueSize is called with a method name that exists in the lola deployment
    auto queue_size = GetQueueSize(handle, kDummyMethodName, MethodType::kMethod);

    // Then the correct que_size is returned and no crush occures
    ASSERT_EQ(queue_size, kQueueSize);
}

TYPED_TEST(ProxyMethodFactoryTypedFixture, GetQueueSizeReturnsOneForFieldGetMethod)
{
    // Given a handle to a valid lola deployment
    const auto handle = this->GetValidLoLaHandle();

    // When GetQueueSize is called with MethodType::kGet, the method_name argument is not consulted
    // because field Get/Set are synchronous and fixed at queue size 1.
    auto queue_size = GetQueueSize(handle, "AnyFieldName", MethodType::kGet);

    ASSERT_EQ(queue_size, 1U);
}

TYPED_TEST(ProxyMethodFactoryTypedFixture, GetQueueSizeReturnsOneForFieldSetMethod)
{
    // Given a handle to a valid lola deployment
    const auto handle = this->GetValidLoLaHandle();

    // Same as above for MethodType::kSet.
    auto queue_size = GetQueueSize(handle, "AnyFieldName", MethodType::kSet);

    ASSERT_EQ(queue_size, 1U);
}

TYPED_TEST(ProxyMethodFactoryTypedFixture, GetQueueSizeTerminatesForMethodNotInLolaDeployment)
{
    // Given a handle to a valid lola deployment which contains a method
    const auto handle = this->GetValidLoLaHandle();

    // when GetQueueSize is called with a method name which does not exist in the lola deployment
    auto wrong_name = "ThisMethodDoesNotExist";

    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_ASSERT_CONTRACT_VIOLATED(score::cpp::ignore =
                                                          GetQueueSize(handle, wrong_name, MethodType::kMethod));
}

TYPED_TEST(ProxyMethodFactoryTypedFixture, GetQueueSizeTerminatesForMethodInLolaDeploymentWithoutQueueSize)
{

    // Given a handle to a valid lola deployment which contains a method with empty QueueSize
    const auto handle = kConfigStoreWithEmptyQueueSizeAsilB.GetHandle();

    // when GetQueueSize is called with the method name with empty QueueSize
    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_ASSERT_CONTRACT_VIOLATED(score::cpp::ignore =
                                                          GetQueueSize(handle, kDummyMethodName, MethodType::kMethod));
}

}  // namespace score::mw::com::impl
