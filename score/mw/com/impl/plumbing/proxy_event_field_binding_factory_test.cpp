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
#include "score/mw/com/impl/bindings/lola/test/proxy_event_test_resources.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/configuration/service_identifier_type.h"
#include "score/mw/com/impl/configuration/service_instance_id.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/plumbing/proxy_binding_factory.h"
#include "score/mw/com/impl/plumbing/proxy_event_binding_factory.h"
#include "score/mw/com/impl/plumbing/proxy_field_binding_factory.h"
#include "score/mw/com/impl/proxy_event_binding_base.h"
#include "score/mw/com/impl/test/dummy_instance_identifier_builder.h"

#include <gtest/gtest.h>
#include <exception>
#include <memory>
#include <utility>

namespace score::mw::com::impl
{

using namespace ::testing;

using TestSampleType = std::uint8_t;

constexpr auto kDummyEventName{"Event1"};
constexpr auto kDummyFieldName{"Field1"};
constexpr auto kDummyGenericProxyEventName{"GenericProxyEvent1"};

constexpr std::uint16_t kDummyEventId{5U};
constexpr std::uint16_t kDummyFieldId{6U};
constexpr std::uint16_t kDummyGenericProxyId{7U};

constexpr uint16_t kInstanceId = 0x31U;
const LolaServiceId kServiceId{1U};
constexpr lola::SkeletonEventProperties kSkeletonEventProperties{5U, 3U, true};
const auto kInstanceSpecifier = InstanceSpecifier::Create(std::string{"/my_dummy_instance_specifier"}).value();

const LolaServiceInstanceDeployment kLolaServiceInstanceDeployment{
    LolaServiceInstanceId{kInstanceId},
    {{kDummyEventName, LolaEventInstanceDeployment{{1U}, {3U}, 1U, true, 0}},
     {kDummyGenericProxyEventName, LolaEventInstanceDeployment{{1U}, {3U}, 1U, true, 0}}},
    {{kDummyFieldName, LolaFieldInstanceDeployment{{1U}, {3U}, 1U, true, 0}}}};
const LolaServiceTypeDeployment kLolaServiceTypeDeployment{
    kServiceId,
    {{kDummyEventName, kDummyEventId}, {kDummyGenericProxyEventName, kDummyGenericProxyId}},
    {{kDummyFieldName, kDummyFieldId}}};

constexpr auto kQualityType = QualityType::kASIL_B;
ConfigurationStore kConfigStoreAsilB{kInstanceSpecifier,
                                     make_ServiceIdentifierType("/a/service/somewhere/out/there", 13U, 37U),
                                     kQualityType,
                                     kLolaServiceTypeDeployment,
                                     kLolaServiceInstanceDeployment};

enum class ServiceElementTypes
{
    PROXY_EVENT,
    PROXY_FIELD,
    GENERIC_PROXY_EVENT
};

class ProxyServiceElementBindingFactoryParamaterisedFixture : public lola::ProxyMockedMemoryFixture,
                                                              public ::testing::WithParamInterface<ServiceElementTypes>
{
  public:
    void SetUp() override
    {
        ASSERT_TRUE((service_element_type_ == ServiceElementTypes::PROXY_EVENT) ||
                    (service_element_type_ == ServiceElementTypes::PROXY_FIELD) ||
                    (service_element_type_ == ServiceElementTypes::GENERIC_PROXY_EVENT));
    }

    ProxyServiceElementBindingFactoryParamaterisedFixture& WithAProxyBaseWithValidBinding(const HandleType& handle)
    {
        proxy_base_ = std::make_unique<ProxyBase>(std::move(proxy_), handle);
        return *this;
    }

    ProxyServiceElementBindingFactoryParamaterisedFixture& WithAProxyBaseWithInvalidBinding(const HandleType& handle)
    {
        proxy_base_ = std::make_unique<ProxyBase>(nullptr, handle);
        return *this;
    }

    lola::ElementFqId GetElementFqId()
    {
        switch (service_element_type_)
        {
            case ServiceElementTypes::PROXY_EVENT:
                return lola::ElementFqId{kServiceId, kDummyEventId, kInstanceId, ServiceElementType::EVENT};
            case ServiceElementTypes::PROXY_FIELD:
                return lola::ElementFqId{kServiceId, kDummyFieldId, kInstanceId, ServiceElementType::FIELD};
            case ServiceElementTypes::GENERIC_PROXY_EVENT:
                return lola::ElementFqId{kServiceId, kDummyGenericProxyId, kInstanceId, ServiceElementType::EVENT};
            default:
                // This should never be reached since we assert the value of element_type_ in service_element_type_()
                std::terminate();
        }
    }

    std::unique_ptr<ProxyEventBindingBase> CreateServiceElementBinding()
    {
        EXPECT_NE(proxy_base_, nullptr);
        if (proxy_base_ == nullptr)
        {
            return nullptr;
        }

        switch (service_element_type_)
        {
            case ServiceElementTypes::PROXY_EVENT:
                return ProxyEventBindingFactory<TestSampleType>::Create(*proxy_base_, kDummyEventName);
            case ServiceElementTypes::PROXY_FIELD:
                return ProxyFieldBindingFactory<TestSampleType>::CreateEventBinding(*proxy_base_, kDummyFieldName);
            case ServiceElementTypes::GENERIC_PROXY_EVENT:
                return GenericProxyEventBindingFactory::Create(*proxy_base_, kDummyGenericProxyEventName);
            default:
                // This should never be reached since we assert the value of element_type_ in service_element_type_()
                std::terminate();
        }
    }

    ServiceElementTypes service_element_type_{GetParam()};
    std::unique_ptr<ProxyBase> proxy_base_{nullptr};
    DummyInstanceIdentifierBuilder dummy_instance_identifier_builder_{};
};

INSTANTIATE_TEST_CASE_P(ProxyServiceElementBindingFactoryParamaterisedFixture,
                        ProxyServiceElementBindingFactoryParamaterisedFixture,
                        ::testing::Values(ServiceElementTypes::PROXY_EVENT,
                                          ServiceElementTypes::PROXY_FIELD,
                                          ServiceElementTypes::GENERIC_PROXY_EVENT));

TEST_P(ProxyServiceElementBindingFactoryParamaterisedFixture, CanConstructFixture) {}

TEST_P(ProxyServiceElementBindingFactoryParamaterisedFixture, CanConstructProxyServiceElement)
{
    RecordProperty("Verifies", "SCR-21803701, SCR-21803702, SCR-5898925");
    RecordProperty("Description", "Checks whether a proxy event lola binding can be created and set at runtime");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a created proxy binding and dummy SkeletonEvent which a ProxyEvent can subscribe to
    const auto instance_identifier = kConfigStoreAsilB.GetInstanceIdentifier();
    InitialiseProxyWithConstructor(instance_identifier);
    InitialiseDummySkeletonEvent(GetElementFqId(), kSkeletonEventProperties);

    // and a Proxy that contains a lola binding
    const auto handle = kConfigStoreAsilB.GetHandle();
    WithAProxyBaseWithValidBinding(handle);

    // When creating a ProxyEvent binding
    const auto proxy_event = CreateServiceElementBinding();

    // Then a valid binding can be created
    ASSERT_NE(proxy_event, nullptr);
}

TEST_P(ProxyServiceElementBindingFactoryParamaterisedFixture, CannotConstructEventFromSomeIpBinding)
{
    // Given a ProxyBase that contains a SomeIp binding
    const auto instance_identifier = dummy_instance_identifier_builder_.CreateSomeIpBindingInstanceIdentifier();
    const auto handle = make_HandleType(instance_identifier, ServiceInstanceId{LolaServiceInstanceId{kInstanceId}});
    WithAProxyBaseWithValidBinding(handle);

    // When constructing a proxy service element
    const auto unit = CreateServiceElementBinding();

    // Then a nullptr is returned
    EXPECT_EQ(unit, nullptr);
}

TEST_P(ProxyServiceElementBindingFactoryParamaterisedFixture, CannotCreateProxyServiceWhenProxyBindingIsNullptr)
{
    // Given a ProxyBase that does not contain a valid binding i.e. the binding is a nullptr
    const auto handle = kConfigStoreAsilB.GetHandle();
    const auto instance_identifier = kConfigStoreAsilB.GetInstanceIdentifier();
    WithAProxyBaseWithInvalidBinding(handle);

    // When creating a Proxy without valid proxy-binding
    const auto proxy_event = CreateServiceElementBinding();

    // Then a nullptr is returned
    ASSERT_EQ(proxy_event, nullptr);
}

TEST_P(ProxyServiceElementBindingFactoryParamaterisedFixture, CannotConstructEventFromBlankBinding)
{
    // Given a ProxyBase that contains a blank binding
    const auto instance_identifier = dummy_instance_identifier_builder_.CreateBlankBindingInstanceIdentifier();
    const auto handle = make_HandleType(instance_identifier, ServiceInstanceId{LolaServiceInstanceId{kInstanceId}});
    WithAProxyBaseWithValidBinding(handle);

    // When constructing a proxy service element
    const auto unit = CreateServiceElementBinding();

    // Then a nullptr is returned
    EXPECT_EQ(unit, nullptr);
}

}  // namespace score::mw::com::impl
