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
// Unit tests for SkeletonFieldBindingFactory which are identical to tests for SkeletonEventBindingFactory are
// implemented in skeleton_service_element_binding_factory_test.cpp. We do this to minimise code duplication by creating
// templated tests in skeleton_service_element_binding_factory_test.cpp. The tests which are only relevant to
// SkeletonFieldBindingFactory and NOT SkeletonEventBindingFactory are added here.

#include "score/mw/com/impl/plumbing/skeleton_field_binding_factory.h"
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

constexpr std::size_t kNumberOfConfiguredSlots{5U};
constexpr std::size_t kNumberOfConfiguredTracingSlots{3U};
constexpr std::size_t kExpectedAdditionalGetterSlots{1U};
constexpr std::size_t kExpectedAdditionalSetterSlots{1U};

constexpr auto kDummyFieldName{"Field1"};
constexpr std::uint16_t kDummyFieldId{6U};

constexpr uint16_t kInstanceId = 0x31U;
const LolaServiceId kServiceId{1U};
const auto kInstanceSpecifier = InstanceSpecifier::Create(std::string{"/my_dummy_instance_specifier"}).value();

const LolaServiceInstanceDeployment kLolaServiceInstanceDeployment{
    LolaServiceInstanceId{kInstanceId},
    {},
    {{kDummyFieldName,
      LolaFieldInstanceDeployment{
          LolaEventInstanceDeployment{{kNumberOfConfiguredSlots}, {3U}, 1U, true, kNumberOfConfiguredTracingSlots},
          false,
          false}}}};
const LolaServiceTypeDeployment kLolaServiceTypeDeployment{kServiceId, {}, {{kDummyFieldName, kDummyFieldId}}};

ConfigurationStore kConfigStoreAsilQM{kInstanceSpecifier,
                                      make_ServiceIdentifierType("/a/service/somewhere/out/there", 13U, 37U),
                                      QualityType::kASIL_QM,
                                      kLolaServiceTypeDeployment,
                                      kLolaServiceInstanceDeployment};

struct TestParams
{
    FieldTagsStore field_tags;
    std::size_t expected_total_number_of_slots;
    std::size_t expected_number_of_getter_slots;
};

class SkeletonFieldBindingFactoryParamaterisedFixture : public lola::SkeletonMockedMemoryFixture,
                                                        public ::testing::WithParamInterface<TestParams>
{
};

INSTANTIATE_TEST_CASE_P(
    SkeletonFieldBindingFactoryParamaterisedFixture,
    SkeletonFieldBindingFactoryParamaterisedFixture,
    ::testing::Values(
        TestParams{FieldTagsStore::Create<WithNotifier>(),
                   kNumberOfConfiguredSlots + kNumberOfConfiguredTracingSlots,
                   0U},
        TestParams{FieldTagsStore::Create<WithGetter>(),
                   kNumberOfConfiguredSlots + kNumberOfConfiguredTracingSlots + kExpectedAdditionalGetterSlots,
                   kExpectedAdditionalGetterSlots},
        TestParams{FieldTagsStore::Create<WithNotifier, WithGetter>(),
                   kNumberOfConfiguredSlots + kNumberOfConfiguredTracingSlots + kExpectedAdditionalGetterSlots,
                   kExpectedAdditionalGetterSlots},
        TestParams{FieldTagsStore::Create<WithNotifier, WithSetter>(),
                   kNumberOfConfiguredSlots + kNumberOfConfiguredTracingSlots + kExpectedAdditionalSetterSlots,
                   0U},
        TestParams{FieldTagsStore::Create<WithGetter, WithSetter>(),
                   kNumberOfConfiguredSlots + kNumberOfConfiguredTracingSlots + kExpectedAdditionalGetterSlots +
                       kExpectedAdditionalSetterSlots,
                   kExpectedAdditionalGetterSlots},
        TestParams{FieldTagsStore::Create<WithNotifier, WithGetter, WithSetter>(),
                   kNumberOfConfiguredSlots + kNumberOfConfiguredTracingSlots + kExpectedAdditionalGetterSlots +
                       kExpectedAdditionalSetterSlots,
                   kExpectedAdditionalGetterSlots}));

TEST_P(SkeletonFieldBindingFactoryParamaterisedFixture, CreatingWithNotifierCreatesBindingWithConfiguredNumberOfSlots)
{
    const auto field_tags = GetParam().field_tags;
    const auto expected_number_of_slots = GetParam().expected_total_number_of_slots;
    const auto expected_number_of_getter_slots = GetParam().expected_number_of_getter_slots;

    // Given a fake Skeleton that uses a lola binding
    const auto& instance_identifier = kConfigStoreAsilQM.GetInstanceIdentifier();
    InitialiseSkeleton(instance_identifier);

    // When creating a SkeletonFieldBinding with specific field tags
    auto skeleton_field_binding = SkeletonFieldBindingFactory<TestSampleType>::CreateEventBinding(
        kConfigStoreAsilQM.GetInstanceIdentifier(), *skeleton_, kDummyFieldName, field_tags);

    // Then the created lola binding has the expected number of slots
    auto skeleton_field_lola_binding = dynamic_cast<lola::SkeletonEvent<TestSampleType>*>(skeleton_field_binding.get());
    ASSERT_NE(skeleton_field_lola_binding, nullptr);
    const auto skeleton_event_properties =
        lola::SkeletonEventAttorney<TestSampleType>::GetSkeletonEventProperties(*skeleton_field_lola_binding);
    EXPECT_EQ(skeleton_event_properties.GetTotalNumberOfSlots(), expected_number_of_slots);
    EXPECT_EQ(skeleton_event_properties.GetNumberOfFieldGetterSlots(), expected_number_of_getter_slots);
}

class SkeletonFieldBindingFactoryFixture : public lola::SkeletonMockedMemoryFixture
{
};

TEST_F(SkeletonFieldBindingFactoryFixture, CreatingWithoutNotifierOrGetterTerminates)
{
    // Given a fake Skeleton that uses a lola binding
    const auto& instance_identifier = kConfigStoreAsilQM.GetInstanceIdentifier();
    InitialiseSkeleton(instance_identifier);

    // When creating a SkeletonFieldBinding without notifier or getter
    // Then the program terminates
    EXPECT_DEATH(score::cpp::ignore = SkeletonFieldBindingFactory<TestSampleType>::CreateEventBinding(
                     kConfigStoreAsilQM.GetInstanceIdentifier(),
                     *skeleton_,
                     kDummyFieldName,
                     FieldTagsStore::Create<WithSetter>()),
                 ".*");
}

}  // namespace
}  // namespace score::mw::com::impl
