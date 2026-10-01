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
// Unit tests for ProxyEventBindingFactory are implemented in proxy_service_element_binding_factory_test.cpp. We
// do this since the tests for ProxyEventBindingFactory and ProxyFieldBindingFactory are almost identical, so we
// create templated tests in proxy_service_element_binding_factory_test.cpp. Any tests which are only relevant to
// ProxyEventBindingFactory and NOT ProxyFieldBindingFactory can be added here.

#include "score/mw/com/impl/plumbing/proxy_event_binding_factory.h"
#include "score/mw/com/impl/bindings/lola/proxy_event.h"
#include "score/mw/com/impl/bindings/lola/test/proxy_event_test_resources.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl
{
namespace
{

using TestSampleType = lola::ProxyMockedMemoryFixture::SampleType;

constexpr auto kDummyEventName{"Event1"};
constexpr std::uint16_t kDummyEventId{5U};
constexpr std::uint16_t kInstanceId{0x31U};
const LolaServiceId kServiceId{1U};
const lola::SkeletonEventProperties kSkeletonEventProperties{5U, 0U, 0U, false, 3U, true};
const auto kInstanceSpecifier = InstanceSpecifier::Create(std::string{"/my_dummy_instance_specifier"}).value();

const LolaServiceInstanceDeployment kLolaServiceInstanceDeployment{
    LolaServiceInstanceId{kInstanceId},
    {{kDummyEventName, LolaEventInstanceDeployment{{1U}, {3U}, 1U, true, 0}}},
    {}};
const LolaServiceTypeDeployment kLolaServiceTypeDeploymentWithE2E{
    kServiceId,
    {{kDummyEventName, kDummyEventId}},
    {},
    {},
    {{kDummyEventName, E2EEventTypeDeployment{E2EProfile::kP4, 0x3AU, 2U}}}};

ConfigurationStore kConfigStoreWithE2E{kInstanceSpecifier,
                                       make_ServiceIdentifierType("/a/service/somewhere/out/there", 13U, 37U),
                                       QualityType::kASIL_QM,
                                       kLolaServiceTypeDeploymentWithE2E,
                                       kLolaServiceInstanceDeployment};

class ProxyEventBindingFactoryE2EFixture : public lola::ProxyMockedMemoryFixture
{
};

TEST_F(ProxyEventBindingFactoryE2EFixture, CreatedProxyEventResolvesSameE2EConfigAsProvider)
{
    // Given a lola skeleton event and instance identifier configured with a POC E2E event deployment
    const lola::ElementFqId element_fq_id{kServiceId, kDummyEventId, kInstanceId, ServiceElementType::EVENT};
    InitialiseDummySkeletonEvent(element_fq_id, kSkeletonEventProperties);
    InitialiseProxyWithConstructor(kConfigStoreWithE2E.GetInstanceIdentifier());

    // When creating a ProxyEvent binding for that event
    auto proxy_event_binding = ProxyEventBindingFactory<TestSampleType>::Create(
        kConfigStoreWithE2E.GetHandle(), *proxy_, kDummyEventName, ServiceElementType::EVENT);
    ASSERT_TRUE(proxy_event_binding.has_value());

    // Then the created lola proxy event resolves the same E2E deployment the provider resolved into
    // SkeletonEventProperties, from the identical LolaServiceTypeDeployment
    auto* const proxy_event_lola_binding =
        dynamic_cast<lola::ProxyEvent*>(proxy_event_binding.value().get());
    ASSERT_NE(proxy_event_lola_binding, nullptr);
    ASSERT_TRUE(proxy_event_lola_binding->GetE2EEventDeployment().has_value());
    EXPECT_EQ(proxy_event_lola_binding->GetE2EEventDeployment()->profile_, E2EProfile::kP4);
    EXPECT_EQ(proxy_event_lola_binding->GetE2EEventDeployment()->data_id_, 0x3AU);
    EXPECT_EQ(proxy_event_lola_binding->GetE2EEventDeployment()->max_delta_counter_, 2U);
}

}  // namespace
}  // namespace score::mw::com::impl
