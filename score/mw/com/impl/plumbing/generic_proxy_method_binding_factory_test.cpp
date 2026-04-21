/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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
#include "score/mw/com/impl/plumbing/generic_proxy_method_binding_factory.h"

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/method_meta_info.h"
#include "score/mw/com/impl/bindings/lola/test/proxy_event_test_resources.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/configuration/service_identifier_type.h"
#include "score/mw/com/impl/configuration/service_instance_id.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"
#include "score/mw/com/impl/data_type_meta_info.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/proxy_base.h"
#include "score/mw/com/impl/service_element_type.h"
#include "score/mw/com/impl/test/dummy_instance_identifier_builder.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

namespace score::mw::com::impl
{
namespace
{

using namespace ::testing;

constexpr auto kDummyMethodName{"Method1"};
constexpr std::uint16_t kDummyMethodId{5U};
constexpr std::uint16_t kInstanceId{0x31U};
const LolaServiceId kServiceId{1U};
const auto kInstanceSpecifier = InstanceSpecifier::Create(std::string{"/my_generic_proxy_method_factory"}).value();
const auto kQueueSize{5U};

const LolaServiceInstanceDeployment kLolaServiceInstanceDeployment{
    LolaServiceInstanceId{kInstanceId},
    {},
    {},
    {{kDummyMethodName, LolaMethodInstanceDeployment{kQueueSize}}}};

const LolaServiceTypeDeployment kLolaServiceTypeDeployment{kServiceId, {}, {}, {{kDummyMethodName, kDummyMethodId}}};

ConfigurationStore kConfigStore{kInstanceSpecifier,
                                make_ServiceIdentifierType("foo"),
                                QualityType::kASIL_QM,
                                kLolaServiceTypeDeployment,
                                kLolaServiceInstanceDeployment};

const LolaServiceInstanceDeployment kLolaServiceInstanceDeploymentWithoutQueueSize{
    LolaServiceInstanceId{kInstanceId},
    {},
    {},
    {{kDummyMethodName, LolaMethodInstanceDeployment{std::nullopt}}}};

ConfigurationStore kConfigStoreWithoutQueueSize{
    InstanceSpecifier::Create(std::string{"/my_generic_proxy_method_factory_no_qs"}).value(),
    make_ServiceIdentifierType("foo"),
    QualityType::kASIL_QM,
    kLolaServiceTypeDeployment,
    kLolaServiceInstanceDeploymentWithoutQueueSize};

const LolaServiceInstanceDeployment kLolaServiceInstanceDeploymentWithoutMethodEntry{
    LolaServiceInstanceId{kInstanceId}, {}, {}, {}};

ConfigurationStore kConfigStoreWithoutMethodEntry{
    InstanceSpecifier::Create(std::string{"/my_generic_proxy_method_factory_no_entry"}).value(),
    make_ServiceIdentifierType("foo"),
    QualityType::kASIL_QM,
    kLolaServiceTypeDeployment,
    kLolaServiceInstanceDeploymentWithoutMethodEntry};

class GenericProxyMethodBindingFactoryFixture : public lola::ProxyMockedMemoryFixture
{
  public:
    std::unique_ptr<ProxyBase> MakeProxyBase(const HandleType& handle)
    {
        return std::make_unique<ProxyBase>(std::move(proxy_), handle);
    }

    void PublishMethodMetaInfo(const lola::ElementFqId element_fq_id)
    {
        fake_data_->AddMethodMetaInfo(element_fq_id, lola::MethodMetaInfo{std::nullopt, std::nullopt});
    }

    DummyInstanceIdentifierBuilder dummy_instance_identifier_builder_{};
};

TEST_F(GenericProxyMethodBindingFactoryFixture, CreateReturnsValidBindingWhenMetaInfoIsPublished)
{
    const auto handle = kConfigStore.GetHandle();
    InitialiseProxyWithConstructor(kConfigStore.GetInstanceIdentifier());
    const lola::ElementFqId element_fq_id{kServiceId, kDummyMethodId, kInstanceId, ServiceElementType::METHOD};
    PublishMethodMetaInfo(element_fq_id);

    const auto proxy_base = MakeProxyBase(handle);
    const auto binding = GenericProxyMethodBindingFactory::Create(
        handle, ProxyBaseView{*proxy_base}.GetBinding(), kDummyMethodName);

    EXPECT_NE(binding, nullptr);
}

TEST_F(GenericProxyMethodBindingFactoryFixture, CreateReturnsNullptrWhenMetaInfoNotPublished)
{
    const auto handle = kConfigStore.GetHandle();
    InitialiseProxyWithConstructor(kConfigStore.GetInstanceIdentifier());

    const auto proxy_base = MakeProxyBase(handle);
    const auto binding = GenericProxyMethodBindingFactory::Create(
        handle, ProxyBaseView{*proxy_base}.GetBinding(), kDummyMethodName);

    EXPECT_EQ(binding, nullptr);
}

TEST_F(GenericProxyMethodBindingFactoryFixture, CreateReturnsNullptrWhenMethodNameNotInDeployment)
{
    const auto handle = kConfigStore.GetHandle();
    InitialiseProxyWithConstructor(kConfigStore.GetInstanceIdentifier());

    const auto proxy_base = MakeProxyBase(handle);
    const auto binding = GenericProxyMethodBindingFactory::Create(
        handle, ProxyBaseView{*proxy_base}.GetBinding(), "MethodThatDoesNotExist");

    EXPECT_EQ(binding, nullptr);
}

TEST_F(GenericProxyMethodBindingFactoryFixture, CreateReturnsNullptrWhenQueueSizeMissingInDeployment)
{
    const auto handle = kConfigStoreWithoutQueueSize.GetHandle();
    InitialiseProxyWithConstructor(kConfigStoreWithoutQueueSize.GetInstanceIdentifier());
    const lola::ElementFqId element_fq_id{kServiceId, kDummyMethodId, kInstanceId, ServiceElementType::METHOD};
    PublishMethodMetaInfo(element_fq_id);

    const auto proxy_base = MakeProxyBase(handle);
    const auto binding = GenericProxyMethodBindingFactory::Create(
        handle, ProxyBaseView{*proxy_base}.GetBinding(), kDummyMethodName);

    EXPECT_EQ(binding, nullptr);
}

TEST_F(GenericProxyMethodBindingFactoryFixture, CreateReturnsNullptrWhenMethodMissingFromInstanceDeployment)
{
    // The type deployment carries the method (via kLolaServiceTypeDeployment), but this instance deployment omits it,
    // so the factory has no queue size to read on the proxy side.
    const auto handle = kConfigStoreWithoutMethodEntry.GetHandle();
    InitialiseProxyWithConstructor(kConfigStoreWithoutMethodEntry.GetInstanceIdentifier());
    const lola::ElementFqId element_fq_id{kServiceId, kDummyMethodId, kInstanceId, ServiceElementType::METHOD};
    PublishMethodMetaInfo(element_fq_id);

    const auto proxy_base = MakeProxyBase(handle);
    const auto binding = GenericProxyMethodBindingFactory::Create(
        handle, ProxyBaseView{*proxy_base}.GetBinding(), kDummyMethodName);

    EXPECT_EQ(binding, nullptr);
}

TEST_F(GenericProxyMethodBindingFactoryFixture, CreateReturnsNullptrWhenParentBindingIsNotLola)
{
    const auto handle = kConfigStore.GetHandle();

    // nullptr stands in for any non-lola binding - both fail the dynamic_cast in the factory.
    const auto binding = GenericProxyMethodBindingFactory::Create(handle, nullptr, kDummyMethodName);

    EXPECT_EQ(binding, nullptr);
}

TEST_F(GenericProxyMethodBindingFactoryFixture, CreateReturnsNullptrForSomeIpDeployment)
{
    const auto instance_identifier = dummy_instance_identifier_builder_.CreateSomeIpBindingInstanceIdentifier();
    const auto handle = make_HandleType(instance_identifier, ServiceInstanceId{LolaServiceInstanceId{kInstanceId}});

    const auto binding = GenericProxyMethodBindingFactory::Create(handle, nullptr, kDummyMethodName);

    EXPECT_EQ(binding, nullptr);
}

TEST_F(GenericProxyMethodBindingFactoryFixture, CreateReturnsNullptrForBlankDeployment)
{
    const auto instance_identifier = dummy_instance_identifier_builder_.CreateBlankBindingInstanceIdentifier();
    const auto handle = make_HandleType(instance_identifier, ServiceInstanceId{LolaServiceInstanceId{kInstanceId}});

    const auto binding = GenericProxyMethodBindingFactory::Create(handle, nullptr, kDummyMethodName);

    EXPECT_EQ(binding, nullptr);
}

}  // namespace
}  // namespace score::mw::com::impl
