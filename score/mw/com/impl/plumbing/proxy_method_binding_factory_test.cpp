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
    using MethodType = TypeParam;
    auto proxy_method = ProxyMethodBindingFactory<MethodType>::Create(handle, proxy_binding, kDummyMethodName);

    // Then a valid binding can be created
    ASSERT_NE(proxy_method, nullptr);
}

TYPED_TEST(ProxyMethodFactoryTypedFixture, CannotCreateProxyServiceWhenProxyBindingIsNullptr)
{
    const auto handle = this->GetValidLoLaHandle();

    // Given a null proxy binding
    auto proxy_binding{nullptr};

    // When creating a ProxyMethod using MethodBindingFactory
    using MethodType = TypeParam;
    auto proxy_method = ProxyMethodBindingFactory<MethodType>::Create(handle, proxy_binding, kDummyMethodName);

    // Then a nullptr is returned
    ASSERT_EQ(proxy_method, nullptr);
}

TYPED_TEST(ProxyMethodFactoryTypedFixture, CannotConstructEventFromSomeIpBinding)
{

    const auto handle = this->GetValidSomIpHandle();

    // Given a valid someip binding
    auto proxy_binding = this->CreateBindingFromHandle(handle);

    // When creating a ProxyMethod using MethodBindingFactory
    using MethodType = TypeParam;
    auto proxy_method = ProxyMethodBindingFactory<MethodType>::Create(handle, proxy_binding, kDummyMethodName);

    // Then a nullptr is returned
    EXPECT_EQ(proxy_method, nullptr);
}

TYPED_TEST(ProxyMethodFactoryTypedFixture, CannotConstructEventFromBlankBinding)
{
    const auto handle = this->GetBlankBindingHandle();

    // Given a blank binding
    auto proxy_binding = this->CreateBindingFromHandle(handle);

    // When creating a ProxyMethod using MethodBindingFactory
    using MethodType = TypeParam;
    auto proxy_method = ProxyMethodBindingFactory<MethodType>::Create(handle, proxy_binding, kDummyMethodName);

    // Then a nullptr is returned
    EXPECT_EQ(proxy_method, nullptr);
}

TYPED_TEST(ProxyMethodFactoryTypedFixture, GetQueueSizeReturnsValueForMethodInLolaDeployment)
{
    // Given a handle to a valid lola deployment which contains a method
    const auto handle = this->GetValidLoLaHandle();

    // when GetQueueSize is called with a method name that exists in the lola deployment
    auto queue_size = GetQueueSize(handle, kDummyMethodName);

    // Then the correct que_size is returned and no crush occures
    ASSERT_EQ(queue_size, kQueueSize);
}

TYPED_TEST(ProxyMethodFactoryTypedFixture, GetQueueSizeTerminatesForMethodNotInLolaDeployment)
{
    // Given a handle to a valid lola deployment which contains a method
    const auto handle = this->GetValidLoLaHandle();

    // when GetQueueSize is called with a method name which does not exist in the lola deployment
    auto wrong_name = "ThisMethodDoesNotExist";

    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_ASSERT_CONTRACT_VIOLATED(score::cpp::ignore = GetQueueSize(handle, wrong_name));
}

TYPED_TEST(ProxyMethodFactoryTypedFixture, GetQueueSizeTerminatesForMethodInLolaDeploymentWithoutQueueSize)
{

    // Given a handle to a valid lola deployment which contains a method with empty QueueSize
    const auto handle = kConfigStoreWithEmptyQueueSizeAsilB.GetHandle();

    // when GetQueueSize is called with the method name with empty QueueSize
    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_ASSERT_CONTRACT_VIOLATED(score::cpp::ignore = GetQueueSize(handle, kDummyMethodName));
}

}  // namespace score::mw::com::impl
