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
#include "score/mw/com/impl/plumbing/lola_proxy_element_building_blocks.h"

#include "score/mw/com/impl/bindings/lola/test/proxy_event_test_resources.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/configuration/service_identifier_type.h"
#include "score/mw/com/impl/configuration/service_instance_id.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/proxy_base.h"
#include "score/mw/com/impl/service_element_type.h"
#include "score/mw/com/impl/test/dummy_instance_identifier_builder.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <utility>

namespace score::mw::com::impl
{
namespace
{

using namespace ::testing;

constexpr auto kDummyEventName{"Event1"};
constexpr auto kDummyFieldName{"Field1"};
constexpr auto kDummyMethodName{"Method1"};

constexpr std::uint16_t kDummyEventId{5U};
constexpr std::uint16_t kDummyFieldId{6U};
constexpr std::uint16_t kDummyMethodId{7U};

constexpr uint16_t kInstanceId = 0x31U;
const LolaServiceId kServiceId{1U};
const auto kInstanceSpecifier = InstanceSpecifier::Create(std::string{"/my_dummy_instance_specifier"}).value();

const LolaServiceInstanceDeployment kLolaServiceInstanceDeployment{
    LolaServiceInstanceId{kInstanceId},
    {{kDummyEventName, LolaEventInstanceDeployment{{1U}, {3U}, 1U, true, 0}}},
    {{kDummyFieldName, LolaFieldInstanceDeployment{{1U}, {3U}, 1U, true, 0}}},
    {{kDummyMethodName, LolaMethodInstanceDeployment{5U}}}};

const LolaServiceTypeDeployment kLolaServiceTypeDeployment{kServiceId,
                                                           {{kDummyEventName, kDummyEventId}},
                                                           {{kDummyFieldName, kDummyFieldId}},
                                                           {{kDummyMethodName, kDummyMethodId}}};

constexpr auto kQualityType = QualityType::kASIL_B;
ConfigurationStore kConfigStoreAsilB{kInstanceSpecifier,
                                     make_ServiceIdentifierType("/a/service/somewhere/out/there", 13U, 37U),
                                     kQualityType,
                                     kLolaServiceTypeDeployment,
                                     kLolaServiceInstanceDeployment};

class LookupLolaProxyElementFixture : public lola::ProxyMockedMemoryFixture
{
  public:
    LookupLolaProxyElementFixture& WithAProxyBaseWithValidBinding(const HandleType& handle)
    {
        proxy_base_ = std::make_unique<ProxyBase>(std::move(proxy_), handle);
        return *this;
    }

    LookupLolaProxyElementFixture& WithAProxyBaseWithNullBinding(const HandleType& handle)
    {
        proxy_base_ = std::make_unique<ProxyBase>(nullptr, handle);
        return *this;
    }

    std::unique_ptr<ProxyBase> proxy_base_{nullptr};
    DummyInstanceIdentifierBuilder dummy_instance_identifier_builder_{};
};

TEST_F(LookupLolaProxyElementFixture, ReturnsValidResultForEventLookup)
{
    // Given a valid proxy with a lola binding
    const auto handle = kConfigStoreAsilB.GetHandle();
    InitialiseProxyWithConstructor(kConfigStoreAsilB.GetInstanceIdentifier());
    WithAProxyBaseWithValidBinding(handle);

    // When looking up an event element
    auto result = LookupLolaProxyElement(
        handle, ProxyBaseView{*proxy_base_}.GetBinding(), kDummyEventName, ServiceElementType::EVENT);

    // Then the result is valid and contains the expected element FQ ID and instance id
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->element_fq_id.element_id_, kDummyEventId);
    EXPECT_EQ(result->element_fq_id.service_id_, kServiceId);
    EXPECT_EQ(result->element_fq_id.instance_id_, kInstanceId);
    EXPECT_EQ(result->element_fq_id.element_type_, ServiceElementType::EVENT);
    EXPECT_EQ(result->instance_id, kInstanceId);
}

TEST_F(LookupLolaProxyElementFixture, ReturnsValidResultForFieldLookup)
{
    // Given a valid proxy with a lola binding
    const auto handle = kConfigStoreAsilB.GetHandle();
    InitialiseProxyWithConstructor(kConfigStoreAsilB.GetInstanceIdentifier());
    WithAProxyBaseWithValidBinding(handle);

    // When looking up a field element
    auto result = LookupLolaProxyElement(
        handle, ProxyBaseView{*proxy_base_}.GetBinding(), kDummyFieldName, ServiceElementType::FIELD);

    // Then the result is valid with FIELD element type and the instance id flows through
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->element_fq_id.element_id_, kDummyFieldId);
    EXPECT_EQ(result->element_fq_id.element_type_, ServiceElementType::FIELD);
    EXPECT_EQ(result->instance_id, kInstanceId);
}

TEST_F(LookupLolaProxyElementFixture, ReturnsValidResultForMethodLookup)
{
    // Given a valid proxy with a lola binding
    const auto handle = kConfigStoreAsilB.GetHandle();
    InitialiseProxyWithConstructor(kConfigStoreAsilB.GetInstanceIdentifier());
    WithAProxyBaseWithValidBinding(handle);

    // When looking up a method element
    auto result = LookupLolaProxyElement(
        handle, ProxyBaseView{*proxy_base_}.GetBinding(), kDummyMethodName, ServiceElementType::METHOD);

    // Then the result is valid with METHOD element type and the instance id flows through
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->element_fq_id.element_id_, kDummyMethodId);
    EXPECT_EQ(result->element_fq_id.element_type_, ServiceElementType::METHOD);
    EXPECT_EQ(result->instance_id, kInstanceId);
}

TEST_F(LookupLolaProxyElementFixture, ReturnsNulloptWhenParentBindingIsNull)
{
    // Given a proxy base with a null binding
    const auto handle = kConfigStoreAsilB.GetHandle();
    WithAProxyBaseWithNullBinding(handle);

    // When looking up an element
    auto result = LookupLolaProxyElement(handle, nullptr, kDummyEventName, ServiceElementType::EVENT);

    // Then the result is nullopt
    EXPECT_FALSE(result.has_value());
}

TEST_F(LookupLolaProxyElementFixture, ReturnsNulloptForSomeIpBinding)
{
    // Given a handle with a non-lola SomeIP deployment
    const auto instance_identifier = dummy_instance_identifier_builder_.CreateSomeIpBindingInstanceIdentifier();
    const auto handle = make_HandleType(instance_identifier, ServiceInstanceId{LolaServiceInstanceId{kInstanceId}});

    // When looking up an element
    auto result = LookupLolaProxyElement(handle, nullptr, kDummyEventName, ServiceElementType::EVENT);

    // Then the result is nullopt
    EXPECT_FALSE(result.has_value());
}

TEST_F(LookupLolaProxyElementFixture, ReturnsNulloptForBlankBinding)
{
    // Given a handle with a non-lola blank deployment
    const auto instance_identifier = dummy_instance_identifier_builder_.CreateBlankBindingInstanceIdentifier();
    const auto handle = make_HandleType(instance_identifier, ServiceInstanceId{LolaServiceInstanceId{kInstanceId}});

    // When looking up an element
    auto result = LookupLolaProxyElement(handle, nullptr, kDummyEventName, ServiceElementType::EVENT);

    // Then the result is nullopt
    EXPECT_FALSE(result.has_value());
}

}  // namespace
}  // namespace score::mw::com::impl
