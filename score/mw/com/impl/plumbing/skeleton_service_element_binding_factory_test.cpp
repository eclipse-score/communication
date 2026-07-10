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
#include "score/mw/com/impl/bindings/lola/test/skeleton_test_resources.h"
#include "score/mw/com/impl/bindings/mock_binding/skeleton.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/plumbing/skeleton_binding_factory.h"
#include "score/mw/com/impl/plumbing/skeleton_event_binding_factory.h"
#include "score/mw/com/impl/plumbing/skeleton_field_binding_factory.h"
#include "score/mw/com/impl/service_element_type.h"
#include "score/mw/com/impl/skeleton_binding.h"
#include "score/mw/com/impl/test/dummy_instance_identifier_builder.h"

#include "score/mw/log/logging.h"
#include "score/mw/log/recorder_mock.h"

#include <gtest/gtest.h>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace score::mw::com::impl
{
namespace
{

using TestSampleType = std::uint32_t;

constexpr auto kDummyEventName{"Event1"};
constexpr auto kDummyFieldName{"Field1"};

constexpr std::uint16_t kDummyEventId{5U};
constexpr std::uint16_t kDummyFieldId{6U};

constexpr uint16_t kInstanceId = 0x31U;
const LolaServiceId kServiceId{1U};
const auto kInstanceSpecifier = InstanceSpecifier::Create(std::string{"/my_dummy_instance_specifier"}).value();

const LolaServiceInstanceDeployment kLolaServiceInstanceDeployment{
    LolaServiceInstanceId{kInstanceId},
    {{kDummyEventName, LolaEventInstanceDeployment{{1U}, {3U}, 1U, true, 0U}}},
    {{kDummyFieldName,
      LolaFieldInstanceDeployment{LolaEventInstanceDeployment{{1U}, {3U}, 1U, true, 0U}, false, false}}}};
const LolaServiceTypeDeployment kLolaServiceTypeDeployment{kServiceId,
                                                           {{kDummyEventName, kDummyEventId}},
                                                           {{kDummyFieldName, kDummyFieldId}}};

ConfigurationStore kConfigStoreAsilQM{kInstanceSpecifier,
                                      make_ServiceIdentifierType("/a/service/somewhere/out/there", 13U, 37U),
                                      QualityType::kASIL_QM,
                                      kLolaServiceTypeDeployment,
                                      kLolaServiceInstanceDeployment};

class SkeletonServiceElementBindingFactoryParamaterisedFixture
    : public lola::SkeletonMockedMemoryFixture,
      public ::testing::WithParamInterface<ServiceElementType>
{
  protected:
    void SetUp() override
    {
        ASSERT_TRUE((service_element_type_ == ServiceElementType::EVENT) ||
                    (service_element_type_ == ServiceElementType::FIELD));
    }

    lola::ElementFqId GetElementFqId()
    {
        switch (service_element_type_)
        {
            case ServiceElementType::EVENT:
                return lola::ElementFqId{kServiceId, kDummyEventId, kInstanceId, service_element_type_};
            case ServiceElementType::FIELD:
                return lola::ElementFqId{kServiceId, kDummyFieldId, kInstanceId, service_element_type_};
            case ServiceElementType::METHOD:
            case ServiceElementType::INVALID:
            default:
                // This should never be reached since we assert the value of service_element_type_ in SetUp()
                std::terminate();
        }
    }

    std::unique_ptr<SkeletonEventBinding<TestSampleType>> CreateServiceElementBinding(
        const InstanceIdentifier instance_identifier,
        SkeletonBinding& skeleton_binding)
    {
        switch (service_element_type_)
        {
            case ServiceElementType::EVENT:
                return SkeletonEventBindingFactory<TestSampleType>::Create(
                    instance_identifier, skeleton_binding, kDummyEventName);
            case ServiceElementType::FIELD:
                return SkeletonFieldBindingFactory<TestSampleType>::CreateEventBinding(
                    instance_identifier, skeleton_binding, kDummyFieldName, FieldNotifier::kEnabled);
            case ServiceElementType::METHOD:
            case ServiceElementType::INVALID:
            default:
                // This should never be reached since we assert the value of service_element_type_ in SetUp()
                std::terminate();
        }
    }

    ServiceElementType service_element_type_{GetParam()};
    DummyInstanceIdentifierBuilder dummy_instance_identifier_builder{};
};

INSTANTIATE_TEST_CASE_P(SkeletonServiceElementBindingFactoryParamaterisedFixture,
                        SkeletonServiceElementBindingFactoryParamaterisedFixture,
                        ::testing::Values(ServiceElementType::EVENT, ServiceElementType::FIELD));

using SkeletonServiceElementBindingFactoryParamaterisedDeathTest =
    SkeletonServiceElementBindingFactoryParamaterisedFixture;
INSTANTIATE_TEST_CASE_P(SkeletonServiceElementBindingFactoryParamaterisedDeathTest,
                        SkeletonServiceElementBindingFactoryParamaterisedDeathTest,
                        ::testing::Values(ServiceElementType::EVENT, ServiceElementType::FIELD));

TEST_P(SkeletonServiceElementBindingFactoryParamaterisedFixture, CanConstructFixture) {}

TEST_P(SkeletonServiceElementBindingFactoryParamaterisedFixture, CanConstructServiceElement)
{
    RecordProperty("Verifies", "SCR-21803701, SCR-21803702, SCR-5898925");
    RecordProperty("Description", "Checks whether a skeleton field lola binding can be created and set at runtime");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a fake Skeleton that uses a lola binding
    const auto& instance_identifier = kConfigStoreAsilQM.GetInstanceIdentifier();
    InitialiseSkeleton(instance_identifier);

    // When constructing a Skeleton service element for
    const auto unit = CreateServiceElementBinding(instance_identifier, *skeleton_);

    // Then a valid binding can be created
    EXPECT_NE(unit, nullptr);
}

TEST_P(SkeletonServiceElementBindingFactoryParamaterisedFixture, CannotConstructServiceElementFromBlankBinding)
{
    // Given a SkeletonBase that uses a blank binding
    const auto instance_identifier = dummy_instance_identifier_builder.CreateBlankBindingInstanceIdentifier();

    // When constructing a service element
    mock_binding::Skeleton mock_skeleton_binding{};
    const auto unit = CreateServiceElementBinding(instance_identifier, mock_skeleton_binding);

    // Then a valid binding cannot be created
    EXPECT_EQ(unit, nullptr);
}

using SkeletonServiceElementBindingFactoryParamaterisedDeathTest =
    SkeletonServiceElementBindingFactoryParamaterisedFixture;
TEST_P(SkeletonServiceElementBindingFactoryParamaterisedDeathTest,
       ConstructingWithInvalidServiceElementNamesInServiceTypeDeploymentTerminates)
{
    // Given a SkeletonBase which contains a valid lola Skeleton binding
    const auto& instance_identifier = kConfigStoreAsilQM.GetInstanceIdentifier();
    InitialiseSkeleton(instance_identifier);

    // When constructing a Skeleton service element with an InstanceIdentifier containing service element names that
    // don't exist in the service type deployment
    const auto incorrect_event_name{"incorrect_event_name"};
    const auto incorrect_field_name{"incorrect_field_name"};
    const LolaServiceTypeDeployment lola_service_type_deployment_with_invalid_names{
        kServiceId, {{incorrect_event_name, kDummyEventId}}, {{incorrect_field_name, kDummyFieldId}}};

    ConfigurationStore config_store_with_invalid_service_element_names{
        kInstanceSpecifier,
        make_ServiceIdentifierType("/a/service/somewhere/out/there", 13U, 37U),
        QualityType::kASIL_QM,
        lola_service_type_deployment_with_invalid_names,
        kConfigStoreAsilQM.lola_service_instance_deployment_};
    const auto instance_identifier_invalid_type_deployment =
        config_store_with_invalid_service_element_names.GetInstanceIdentifier();

    // Then the program terminates
    EXPECT_DEATH(
        score::cpp::ignore = CreateServiceElementBinding(instance_identifier_invalid_type_deployment, *skeleton_),
        ".*");
}

TEST_P(SkeletonServiceElementBindingFactoryParamaterisedDeathTest,
       ConstructingWithInvalidServiceElementNamesInServiceInstanceDeploymentTerminates)
{
    // Given a SkeletonBase which contains a valid lola Skeleton binding
    const auto& instance_identifier = kConfigStoreAsilQM.GetInstanceIdentifier();
    InitialiseSkeleton(instance_identifier);

    // When constructing a Skeleton service element with an InstanceIdentifier containing service element names that
    // don't exist in the service instance deployment
    const auto incorrect_event_name{"incorrect_event_name"};
    const auto incorrect_field_name{"incorrect_field_name"};
    const LolaServiceInstanceDeployment lola_service_instance_deployment_with_invalid_names{
        LolaServiceInstanceId{kInstanceId},
        {{incorrect_event_name, LolaEventInstanceDeployment{{1U}, {3U}, 1U, true, 0U}}},
        {{incorrect_field_name,
          LolaFieldInstanceDeployment{LolaEventInstanceDeployment{{1U}, {3U}, 1U, true, 0U}, false, false}}}};

    ConfigurationStore config_store_with_invalid_service_element_names{
        kInstanceSpecifier,
        make_ServiceIdentifierType("/a/service/somewhere/out/there", 13U, 37U),
        QualityType::kASIL_QM,
        kConfigStoreAsilQM.lola_service_type_deployment_,
        lola_service_instance_deployment_with_invalid_names};
    const auto instance_identifier_invalid_instance_deployment =
        config_store_with_invalid_service_element_names.GetInstanceIdentifier();

    // Then the program terminates
    EXPECT_DEATH(
        score::cpp::ignore = CreateServiceElementBinding(instance_identifier_invalid_instance_deployment, *skeleton_),
        ".*");
}

TEST_P(SkeletonServiceElementBindingFactoryParamaterisedDeathTest,
       ConstructingWithoutNumberOfSamplesSlotsInServiceInstanceDeploymentTerminatestes)
{
    // Given a SkeletonBase which contains a valid lola Skeleton binding
    const auto& instance_identifier = kConfigStoreAsilQM.GetInstanceIdentifier();
    InitialiseSkeleton(instance_identifier);

    // and given an InstanceIdentifier containing event / field instance deployments which do not contain
    // number_of_sample_slots
    const LolaServiceInstanceDeployment lola_service_instance_deployment_without_event_sample_slots{
        LolaServiceInstanceId{kInstanceId},
        {{kDummyEventName, LolaEventInstanceDeployment{{}, {3U}, 1U, true, 0U}}},
        {{kDummyFieldName,
          LolaFieldInstanceDeployment{LolaEventInstanceDeployment{{}, {3U}, 1U, true, 0U}, false, false}}}};

    ConfigurationStore config_store_with_invalid_service_element_names{
        kInstanceSpecifier,
        make_ServiceIdentifierType("/a/service/somewhere/out/there", 13U, 37U),
        QualityType::kASIL_QM,
        kConfigStoreAsilQM.lola_service_type_deployment_,
        lola_service_instance_deployment_without_event_sample_slots};
    const auto instance_identifier_invalid_instance_deployment =
        config_store_with_invalid_service_element_names.GetInstanceIdentifier();

    // When creating the service element binding
    // Then the program terminates
    EXPECT_DEATH(
        score::cpp::ignore = CreateServiceElementBinding(instance_identifier_invalid_instance_deployment, *skeleton_),
        ".*");
}

TEST_P(SkeletonServiceElementBindingFactoryParamaterisedDeathTest,
       ConstructingWithoutMaxSubscribersInServiceInstanceDeploymentTerminatestes)
{
    // Given a SkeletonBase which contains a valid lola Skeleton binding
    const auto& instance_identifier = kConfigStoreAsilQM.GetInstanceIdentifier();
    InitialiseSkeleton(instance_identifier);

    // and given an InstanceIdentifier containing event / field instance deployments which do not contain
    // max_subscribers
    const LolaServiceInstanceDeployment lola_service_instance_deployment_without_max_subscribers{
        LolaServiceInstanceId{kInstanceId},
        {{kDummyEventName, LolaEventInstanceDeployment{{1U}, {}, 1U, true, 0U}}},
        {{kDummyFieldName,
          LolaFieldInstanceDeployment{LolaEventInstanceDeployment{{2U}, {}, 1U, true, 0U}, false, false}}}};

    ConfigurationStore config_store_with_invalid_service_element_names{
        kInstanceSpecifier,
        make_ServiceIdentifierType("/a/service/somewhere/out/there", 13U, 37U),
        QualityType::kASIL_QM,
        kConfigStoreAsilQM.lola_service_type_deployment_,
        lola_service_instance_deployment_without_max_subscribers};
    const auto instance_identifier_invalid_instance_deployment =
        config_store_with_invalid_service_element_names.GetInstanceIdentifier();

    // When creating the service element binding
    // Then the program terminates
    EXPECT_DEATH(
        score::cpp::ignore = CreateServiceElementBinding(instance_identifier_invalid_instance_deployment, *skeleton_),
        ".*");
}

// Slot count for a field depends only on whether it has a notifier (WithNotifier):
//   with notifier    + configured slots -> configured value is used, no warning
//   with notifier    + missing slots    -> terminates (see death test above)
//   without notifier + configured slots -> fixed count is used, configured value ignored with a warning
//   without notifier + missing slots    -> fixed count is used, no warning
// The observable behaviour (binding created, warning emitted) is checked below through the factory. The exact slot
// numbers, which the built binding does not expose, are pinned at the CreateSkeletonEventProperties helper further
// down.

class SkeletonFieldNotifierSlotCountFixture : public lola::SkeletonMockedMemoryFixture
{
  protected:
    void SetUp() override
    {
        lola::SkeletonMockedMemoryFixture::SetUp();
        score::mw::log::SetLogRecorder(&recorder_mock_);
    }

    void TearDown() override
    {
        score::mw::log::SetLogRecorder(nullptr);
        lola::SkeletonMockedMemoryFixture::TearDown();
    }

    InstanceIdentifier MakeFieldInstanceIdentifier(const std::optional<std::uint16_t> configured_field_slots)
    {
        config_store_.emplace(
            kInstanceSpecifier,
            make_ServiceIdentifierType("/a/service/somewhere/out/there", 13U, 37U),
            QualityType::kASIL_QM,
            kLolaServiceTypeDeployment,
            LolaServiceInstanceDeployment{
                LolaServiceInstanceId{kInstanceId},
                {{kDummyEventName, LolaEventInstanceDeployment{{1U}, {3U}, 1U, true, 0U}}},
                {{kDummyFieldName,
                  LolaFieldInstanceDeployment{
                      LolaEventInstanceDeployment{configured_field_slots, {3U}, 1U, true, 0U}, false, false}}}});
        return config_store_->GetInstanceIdentifier();
    }

    std::optional<ConfigurationStore> config_store_{};
    score::mw::log::RecorderMock recorder_mock_{};
};

TEST_F(SkeletonFieldNotifierSlotCountFixture, FieldWithoutNotifierIsCreatedWithoutAConfiguredSlotCount)
{
    // Given a lola skeleton whose field does not configure numberOfSampleSlots
    const auto instance_identifier = MakeFieldInstanceIdentifier(std::nullopt);
    InitialiseSkeleton(instance_identifier);

    // and no warning about an ignored slot count is expected
    EXPECT_CALL(recorder_mock_, StartRecord(std::string_view{"lola"}, score::mw::log::LogLevel::kWarn)).Times(0);

    // When creating the binding for a field whose notifier is disabled
    const auto unit = SkeletonFieldBindingFactory<TestSampleType>::CreateEventBinding(
        instance_identifier, *skeleton_, kDummyFieldName, FieldNotifier::kDisabled);

    // Then a valid binding is created (with a notifier a missing slot count would terminate, see
    // ConstructingWithoutNumberOfSamplesSlotsInServiceInstanceDeploymentTerminatestes test above)
    EXPECT_NE(unit, nullptr);
}

TEST_F(SkeletonFieldNotifierSlotCountFixture, FieldWithoutNotifierWarnsWhenASlotCountIsConfigured)
{
    // Given a lola skeleton whose field configures a numberOfSampleSlots that a notifier-less field cannot use
    const auto instance_identifier = MakeFieldInstanceIdentifier(std::uint16_t{5U});
    InitialiseSkeleton(instance_identifier);

    // Given a warning about the ignored slot count is expected
    EXPECT_CALL(recorder_mock_, StartRecord(std::string_view{"lola"}, score::mw::log::LogLevel::kWarn)).Times(1);

    // When creating the binding for a field whose notifier is disabled
    const auto unit = SkeletonFieldBindingFactory<TestSampleType>::CreateEventBinding(
        instance_identifier, *skeleton_, kDummyFieldName, FieldNotifier::kDisabled);

    // Then a valid binding is still created
    EXPECT_NE(unit, nullptr);
}

TEST_F(SkeletonFieldNotifierSlotCountFixture, FieldWithNotifierDoesNotWarnAboutTheConfiguredSlotCount)
{
    // Given a lola skeleton whose field configures a numberOfSampleSlots
    const auto instance_identifier = MakeFieldInstanceIdentifier(std::uint16_t{5U});
    InitialiseSkeleton(instance_identifier);

    // Given no warning is expected because an enabled notifier uses the configured count
    EXPECT_CALL(recorder_mock_, StartRecord(std::string_view{"lola"}, score::mw::log::LogLevel::kWarn)).Times(0);

    // When creating the binding for a field whose notifier is enabled
    const auto unit = SkeletonFieldBindingFactory<TestSampleType>::CreateEventBinding(
        instance_identifier, *skeleton_, kDummyFieldName, FieldNotifier::kEnabled);

    // Then a valid binding is created
    EXPECT_NE(unit, nullptr);
}

// There is no Public API to check the slot count of a binding, so the exact slot count is pinned at the
// CreateSkeletonEventProperties helper.

TEST(CreateSkeletonEventPropertiesTest, UsesConfiguredSlotCountWhenNoOverrideIsGiven)
{
    // Given a field deployment configuring 5 sample slots and no tracing slots
    const LolaFieldInstanceDeployment field_deployment{
        LolaEventInstanceDeployment{{5U}, {3U}, 1U, true, 0U}, false, false};

    // When creating the SkeletonEventProperties without a slot count override
    const auto properties = detail::CreateSkeletonEventProperties(field_deployment, std::nullopt);

    // Then the configured slot count is used
    EXPECT_EQ(properties.number_of_slots, 5U);
}

TEST(CreateSkeletonEventPropertiesTest, OverrideReplacesConfiguredCountAndTracingSlotsAreAddedOnTop)
{
    // Given a field deployment configuring 9 sample slots and 3 tracing slots
    const LolaFieldInstanceDeployment field_deployment{
        LolaEventInstanceDeployment{{9U}, {3U}, 1U, true, 3U}, false, false};

    // When creating the SkeletonEventProperties with a slot count override of 2
    const auto properties = detail::CreateSkeletonEventProperties(field_deployment, std::uint16_t{2U});

    // Then the override replaces the configured 9 and the tracing slots are added on top (2 + 3)
    EXPECT_EQ(properties.number_of_slots, 5U);
}

}  // namespace
}  // namespace score::mw::com::impl
