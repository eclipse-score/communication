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
// Unit tests for SkeletonEventBindingFactory which are identical to tests for SkeletonFieldBindingFactory are
// implemented in skeleton_service_element_binding_factory_test.cpp. We do this to minimise code duplication by creating
// templated tests in skeleton_service_element_binding_factory_test.cpp. The tests which are only relevant to
// SkeletonEventBindingFactory and NOT SkeletonFieldBindingFactory are added here.

#include "score/mw/com/impl/plumbing/skeleton_event_binding_factory.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event_common.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event_properties.h"
#include "score/mw/com/impl/bindings/lola/test/skeleton_test_resources.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl
{

namespace lola
{

template <typename SampleType>
class SkeletonEventAttorney
{
  public:
    static const SkeletonEventProperties& GetSkeletonEventProperties(const SkeletonEvent<SampleType>& skeleton_event)
    {
        return skeleton_event.skeleton_event_common_.GetEventProperties();
    }
};

}  // namespace lola

namespace
{

using TestSampleType = std::uint32_t;

constexpr auto kDummyEventName{"Event1"};
constexpr std::uint16_t kDummyEventId{5U};

constexpr std::size_t kNumberOfConfiguredSlots{5U};
constexpr std::size_t kNumberOfConfiguredTracingSlots{3U};

constexpr uint16_t kInstanceId = 0x31U;
const LolaServiceId kServiceId{1U};
const auto kInstanceSpecifier = InstanceSpecifier::Create(std::string{"/my_dummy_instance_specifier"}).value();

const LolaServiceInstanceDeployment kLolaServiceInstanceDeployment{
    LolaServiceInstanceId{kInstanceId},
    {{kDummyEventName,
      LolaEventInstanceDeployment{{kNumberOfConfiguredSlots}, {3U}, 1U, true, kNumberOfConfiguredTracingSlots}}},
    {}};
const LolaServiceTypeDeployment kLolaServiceTypeDeployment{kServiceId, {{kDummyEventName, kDummyEventId}}, {}};

ConfigurationStore kConfigStoreAsilQM{kInstanceSpecifier,
                                      make_ServiceIdentifierType("/a/service/somewhere/out/there", 13U, 37U),
                                      QualityType::kASIL_QM,
                                      kLolaServiceTypeDeployment,
                                      kLolaServiceInstanceDeployment};

class SkeletonEventBindingFactoryFixture : public lola::SkeletonMockedMemoryFixture
{
};

TEST_F(SkeletonEventBindingFactoryFixture, CreatingEventBindingCreatesBindingWithConfiguredNumberOfEvents)
{
    // Given a fake Skeleton that uses a lola binding
    const auto& instance_identifier = kConfigStoreAsilQM.GetInstanceIdentifier();
    InitialiseSkeleton(instance_identifier);

    // When creating a SkeletonEventBinding
    auto skeleton_event_binding = SkeletonEventBindingFactory<TestSampleType>::Create(
        kConfigStoreAsilQM.GetInstanceIdentifier(), *skeleton_, kDummyEventName);

    // Then the created lola binding has the expected number of events in SkeletonEventProperties
    auto skeleton_event_lola_binding = dynamic_cast<lola::SkeletonEvent<TestSampleType>*>(skeleton_event_binding.get());
    ASSERT_NE(skeleton_event_lola_binding, nullptr);
    const auto skeleton_event_properties =
        lola::SkeletonEventAttorney<TestSampleType>::GetSkeletonEventProperties(*skeleton_event_lola_binding);
    EXPECT_EQ(skeleton_event_properties.GetTotalNumberOfSlots(),
              kNumberOfConfiguredSlots + kNumberOfConfiguredTracingSlots);
    EXPECT_EQ(skeleton_event_properties.GetNumberOfFieldGetterSlots(), 0U);
}

}  // namespace
}  // namespace score::mw::com::impl
