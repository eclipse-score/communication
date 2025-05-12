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
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"
#include "score/mw/com/impl/plumbing/skeleton_binding_factory.h"
#include "score/mw/com/impl/plumbing/skeleton_event_binding_factory.h"
#include "score/mw/com/impl/plumbing/skeleton_field_binding_factory.h"
#include "score/mw/com/impl/test/dummy_instance_identifier_builder.h"

#include <gtest/gtest.h>
#include <cstdint>
#include <memory>
#include <string>

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
const auto kInstanceSpecifier = InstanceSpecifier::Create("/my_dummy_instance_specifier").value();

const LolaServiceInstanceDeployment kLolaServiceInstanceDeployment{
    LolaServiceInstanceId{kInstanceId},
    {{kDummyEventName, LolaEventInstanceDeployment{{1U}, {3U}, 1U, true, 0U}}},
    {{kDummyFieldName, LolaFieldInstanceDeployment{{1U}, {3U}, 1U, true, 0U}}}};
const LolaServiceTypeDeployment kLolaServiceTypeDeployment{kServiceId,
                                                           {{kDummyEventName, kDummyEventId}},
                                                           {{kDummyFieldName, kDummyFieldId}}};

ConfigurationStore kConfigStoreAsilQM{kInstanceSpecifier,
                                      make_ServiceIdentifierType("/a/service/somewhere/out/there", 13U, 37U),
                                      QualityType::kASIL_QM,
                                      kLolaServiceTypeDeployment,
                                      kLolaServiceInstanceDeployment};

class SkeletonServiceElementBindingFactoryParamaterisedFixture : public ::testing::TestWithParam<lola::ElementType>
{
  protected:
    void SetUp() override
    {
        ASSERT_TRUE((service_element_type_ == lola::ElementType::EVENT) ||
                    (service_element_type_ == lola::ElementType::FIELD));
    }

    SkeletonServiceElementBindingFactoryParamaterisedFixture& WithASkeletonBaseWithValidBinding(
        InstanceIdentifier instance_identifier)
    {
        skeleton_base_ = std::make_unique<SkeletonBase>(SkeletonBindingFactory::Create(instance_identifier),
                                                        std::move(instance_identifier));
        return *this;
    }

    SkeletonServiceElementBindingFactoryParamaterisedFixture& WithASkeletonBaseWithInvalidBinding(
        InstanceIdentifier instance_identifier)
    {
        skeleton_base_ = std::make_unique<SkeletonBase>(nullptr, std::move(instance_identifier));
        return *this;
    }

    lola::ElementFqId GetElementFqId()
    {
        switch (service_element_type_)
        {
            case lola::ElementType::EVENT:
                return lola::ElementFqId{kServiceId, kDummyEventId, kInstanceId, service_element_type_};
            case lola::ElementType::FIELD:
                return lola::ElementFqId{kServiceId, kDummyFieldId, kInstanceId, service_element_type_};
            case lola::ElementType::INVALID:
            default:
                // This should never be reached since we assert the value of service_element_type_ in SetUp()
                std::terminate();
        }
    }

    std::unique_ptr<SkeletonEventBinding<TestSampleType>> CreateServiceElementBinding(
        const InstanceIdentifier instance_identifier)
    {
        EXPECT_NE(skeleton_base_, nullptr);
        if (skeleton_base_ == nullptr)
        {
            return nullptr;
        }

        switch (service_element_type_)
        {
            case lola::ElementType::EVENT:
                return SkeletonEventBindingFactory<TestSampleType>::Create(
                    instance_identifier, *skeleton_base_, kDummyEventName);
            case lola::ElementType::FIELD:
                return SkeletonFieldBindingFactory<TestSampleType>::CreateEventBinding(
                    instance_identifier, *skeleton_base_, kDummyFieldName);
            case lola::ElementType::INVALID:
            default:
                // This should never be reached since we assert the value of service_element_type_ in SetUp()
                std::terminate();
        }
    }

    lola::ElementType service_element_type_{GetParam()};
    std::unique_ptr<SkeletonBase> skeleton_base_{nullptr};
    DummyInstanceIdentifierBuilder dummy_instance_identifier_builder{};
};

INSTANTIATE_TEST_CASE_P(SkeletonServiceElementBindingFactoryParamaterisedFixture,
                        SkeletonServiceElementBindingFactoryParamaterisedFixture,
                        ::testing::Values(lola::ElementType::EVENT, lola::ElementType::FIELD));

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
    WithASkeletonBaseWithValidBinding(instance_identifier);

    // When constructing a Skeleton service element for
    const auto unit = CreateServiceElementBinding(instance_identifier);

    // Then a valid binding can be created
    EXPECT_NE(unit, nullptr);
}

TEST_P(SkeletonServiceElementBindingFactoryParamaterisedFixture, CannotConstructServiceElementFromSomeIpBinding)
{
    // Given a SkeletonBase that uses a someip binding
    const auto instance_identifier = dummy_instance_identifier_builder.CreateSomeIpBindingInstanceIdentifier();
    WithASkeletonBaseWithValidBinding(instance_identifier);

    // When constructing a service element
    const auto unit = CreateServiceElementBinding(instance_identifier);

    // Then a valid binding cannot be created
    EXPECT_EQ(unit, nullptr);
}

TEST_P(SkeletonServiceElementBindingFactoryParamaterisedFixture, CannotConstructServiceElementFromBlankBinding)
{
    // Given a SkeletonBase that uses a blank binding
    const auto instance_identifier = dummy_instance_identifier_builder.CreateBlankBindingInstanceIdentifier();
    WithASkeletonBaseWithValidBinding(instance_identifier);

    // When constructing a service element
    const auto unit = CreateServiceElementBinding(instance_identifier);

    // Then a valid binding cannot be created
    EXPECT_EQ(unit, nullptr);
}

TEST_P(SkeletonServiceElementBindingFactoryParamaterisedFixture,
       CannotConstructServiceElementWhenSkeletonBindingIsNullptr)
{
    // Given a SkeletonBase which does not contain a valid lola Skeleton binding
    const auto& instance_identifier = kConfigStoreAsilQM.GetInstanceIdentifier();
    WithASkeletonBaseWithInvalidBinding(instance_identifier);

    // When constructing a Skeleton service element
    const auto unit = CreateServiceElementBinding(instance_identifier);

    // Then a valid binding cannot be created
    EXPECT_EQ(unit, nullptr);
}

}  // namespace
}  // namespace score::mw::com::impl
