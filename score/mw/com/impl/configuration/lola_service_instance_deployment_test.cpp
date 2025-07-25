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
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"

#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/configuration/test/configuration_test_resources.h"

#include "score/mw/com/impl/service_element_type.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>

namespace score::mw::com::impl
{
namespace
{

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

LolaServiceInstanceId kDummyInstanceId{10U};
const std::string kDummyEventName{"my_dummy_event"};
const std::string kDummyFieldName{"my_dummy_field"};
const auto kDummyLolaEventInstanceDeployment{MakeLolaEventInstanceDeployment(12U, 13U)};
const auto kDummyLolaFieldInstanceDeployment{MakeLolaFieldInstanceDeployment(14U, 15U)};
const auto kDummyLolaEventInstanceDeployment2{MakeLolaEventInstanceDeployment(22U, 23U)};
const auto kDummyLolaFieldInstanceDeployment2{MakeLolaFieldInstanceDeployment(24U, 25U)};

const std::unordered_map<QualityType, std::vector<uid_t>> kAllowedConsumers{{QualityType::kASIL_QM, {1, 2}}};
const std::unordered_map<QualityType, std::vector<uid_t>> kAllowedConsumers2{{QualityType::kASIL_QM, {11, 12}}};
const std::unordered_map<QualityType, std::vector<uid_t>> kAllowedProviders{{QualityType::kASIL_B, {3, 4}}};
const std::unordered_map<QualityType, std::vector<uid_t>> kAllowedProviders2{{QualityType::kASIL_B, {13, 14}}};

TEST(LolaServiceInstanceDeployment, construction)
{
    LolaServiceInstanceDeployment unit{LolaServiceInstanceId{LolaServiceInstanceId{42U}}};

    ASSERT_EQ(unit.instance_id_.value(), LolaServiceInstanceId{42U});
}

TEST(LolaServiceInstanceDeployment, InstanceIsOptional)
{
    LolaServiceInstanceDeployment unit{};

    ASSERT_FALSE(unit.instance_id_.has_value());
}

TEST(LolaServiceInstanceDeployment, SameServiceIdBothInstancesAnyIsCompatible)
{
    EXPECT_TRUE(areCompatible(LolaServiceInstanceDeployment{LolaServiceInstanceId{43U}},
                              LolaServiceInstanceDeployment{LolaServiceInstanceId{43U}}));
}

TEST(LolaServiceInstanceDeployment, DifferentServiceIdBothInstancesAnyIsNotCompatible)
{
    EXPECT_FALSE(areCompatible(LolaServiceInstanceDeployment{LolaServiceInstanceId{43U}},
                               LolaServiceInstanceDeployment{LolaServiceInstanceId{44U}}));
}

TEST(LolaServiceInstanceDeployment, SameServiceIdOneInstancesAnyIsCompatible)
{
    EXPECT_TRUE(
        areCompatible(LolaServiceInstanceDeployment{LolaServiceInstanceId{43U}}, LolaServiceInstanceDeployment{}));
}

TEST(LolaServiceInstanceDeployment, Equality)
{
    EXPECT_EQ(LolaServiceInstanceDeployment{LolaServiceInstanceId{43U}},
              LolaServiceInstanceDeployment{LolaServiceInstanceId{43U}});
}

TEST(LolaServiceInstanceDeployment, ContainsEventReturnsTrueIfEventPresent)
{
    LolaServiceInstanceDeployment unit{LolaServiceInstanceId{2U}};
    auto temp = MakeDefaultLolaEventInstanceDeployment();
    unit.events_.emplace(std::make_pair("abc", temp));
    EXPECT_TRUE(unit.ContainsEvent("abc"));
}

TEST(LolaServiceInstanceDeployment, ContainsEventReturnsFalseIfEventMissing)
{
    LolaServiceInstanceDeployment unit{LolaServiceInstanceId{2U}};
    auto temp = MakeDefaultLolaEventInstanceDeployment();
    unit.events_.emplace(std::make_pair("abc", temp));
    EXPECT_FALSE(unit.ContainsEvent("def"));
}

TEST(LolaServiceInstanceDeployment, ContainsFieldReturnsTrueIfEventPresent)
{
    LolaServiceInstanceDeployment unit{LolaServiceInstanceId{LolaServiceInstanceId{2U}}};
    auto temp = MakeDefaultLolaEventInstanceDeployment();
    unit.fields_.emplace(std::make_pair("abc", temp));
    EXPECT_TRUE(unit.ContainsField("abc"));
}

TEST(LolaServiceInstanceDeployment, ContainsFieldReturnsFalseIfEventMissing)
{
    LolaServiceInstanceDeployment unit{LolaServiceInstanceId{2U}};
    auto temp = MakeDefaultLolaEventInstanceDeployment();
    unit.fields_.emplace(std::make_pair("abc", temp));
    EXPECT_FALSE(unit.ContainsField("def"));
}

using LolaServiceInstanceDeploymentFixture = ConfigurationStructsFixture;
TEST_F(LolaServiceInstanceDeploymentFixture, CanCreateFromSerializedObjectWithOptionals)
{
    const LolaServiceInstanceDeployment unit{MakeLolaServiceInstanceDeployment()};

    const auto serialized_unit{unit.Serialize()};

    LolaServiceInstanceDeployment reconstructed_unit{serialized_unit};

    ExpectLolaServiceInstanceDeploymentObjectsEqual(reconstructed_unit, unit);
}

TEST_F(LolaServiceInstanceDeploymentFixture, CanCreateFromSerializedObjectWithSetStrict)
{
    LolaServiceInstanceDeployment unit{MakeLolaServiceInstanceDeployment()};
    unit.strict_permissions_ = true;

    const auto serialized_unit{unit.Serialize()};

    LolaServiceInstanceDeployment reconstructed_unit{serialized_unit};

    ExpectLolaServiceInstanceDeploymentObjectsEqual(reconstructed_unit, unit);
}

TEST_F(LolaServiceInstanceDeploymentFixture, CanCreateFromSerializedObjectWithoutOptionals)
{
    const LolaServiceInstanceDeployment unit{MakeLolaServiceInstanceDeployment({}, {})};

    const auto serialized_unit{unit.Serialize()};

    LolaServiceInstanceDeployment reconstructed_unit{serialized_unit};

    ExpectLolaServiceInstanceDeploymentObjectsEqual(reconstructed_unit, unit);
}

using LolaServiceInstanceDeploymentGetServiceElementFixture = ConfigurationStructsFixture;
TEST_F(LolaServiceInstanceDeploymentGetServiceElementFixture, ReturnsEventThatExistsInDeployment)
{
    // Given a LolaServiceInstanceDeployment containing an event and field
    const LolaServiceInstanceDeployment unit{kDummyInstanceId,
                                             {{kDummyEventName, kDummyLolaEventInstanceDeployment}},
                                             {{kDummyFieldName, kDummyLolaFieldInstanceDeployment}}};

    // When getting an LolaEventInstanceDeployment using the correct event name
    const auto& event_instance_deployment =
        GetServiceElementInstanceDeployment<ServiceElementType::EVENT>(unit, kDummyEventName);

    // Then the returned object is the same as the one in the configuration
    EXPECT_EQ(event_instance_deployment, kDummyLolaEventInstanceDeployment);
}

TEST_F(LolaServiceInstanceDeploymentGetServiceElementFixture, ReturnsFieldThatExistsInDeployment)
{
    // Given a LolaServiceInstanceDeployment containing an event and field
    const LolaServiceInstanceDeployment unit{kDummyInstanceId,
                                             {{kDummyEventName, kDummyLolaEventInstanceDeployment}},
                                             {{kDummyFieldName, kDummyLolaFieldInstanceDeployment}}};

    // When getting an LolaFieldInstanceDeployment using the correct field name
    const auto& field_instance_deployment =
        GetServiceElementInstanceDeployment<ServiceElementType::FIELD>(unit, kDummyFieldName);

    // Then the returned object is the same as the one in the configuration
    EXPECT_EQ(field_instance_deployment, kDummyLolaFieldInstanceDeployment);
}

using LolaServiceInstanceDeploymentGetServiceElementDeathTest = LolaServiceInstanceDeploymentGetServiceElementFixture;
TEST_F(LolaServiceInstanceDeploymentGetServiceElementDeathTest, GettingEventThatDoesNotExistInDeploymentTerminates)
{
    // Given a LolaServiceInstanceDeployment containing an event and field
    const LolaServiceInstanceDeployment unit{kDummyInstanceId,
                                             {{kDummyEventName, kDummyLolaEventInstanceDeployment}},
                                             {{kDummyFieldName, kDummyLolaFieldInstanceDeployment}}};

    // When getting a LolaEventInstanceDeployment using an incorrect event name
    // Then the program termintaes
    EXPECT_DEATH(score::cpp::ignore = GetServiceElementInstanceDeployment<ServiceElementType::EVENT>(unit, kDummyFieldName),
                 ".*");
}

TEST_F(LolaServiceInstanceDeploymentGetServiceElementDeathTest, GettingFieldThatDoesNotExistInDeploymentTerminates)
{
    // Given a LolaServiceInstanceDeployment containing an event and field
    const LolaServiceInstanceDeployment unit{kDummyInstanceId,
                                             {{kDummyEventName, kDummyLolaEventInstanceDeployment}},
                                             {{kDummyFieldName, kDummyLolaFieldInstanceDeployment}}};

    // When getting a LolaFieldInstanceDeployment using an incorrect field name
    // Then the program termintaes
    EXPECT_DEATH(score::cpp::ignore = GetServiceElementInstanceDeployment<ServiceElementType::FIELD>(unit, kDummyEventName),
                 ".*");
}

TEST(LolaServiceInstanceDeploymentDeathTest, CreatingFromSerializedObjectWithMismatchedSerializationVersionTerminates)
{
    const LolaServiceInstanceDeployment unit{LolaServiceInstanceId{42U}};

    const auto serialization_version_key = "serializationVersion";
    const std::uint32_t invalid_serialization_version = LolaServiceInstanceDeployment::serializationVersion + 1;

    auto serialized_unit{unit.Serialize()};
    auto it = serialized_unit.find(serialization_version_key);
    ASSERT_NE(it, serialized_unit.end());
    it->second = json::Any{invalid_serialization_version};

    EXPECT_DEATH(LolaServiceInstanceDeployment reconstructed_unit{serialized_unit}, ".*");
}

using LolaServiceInstanceDeploymentEqualityFixture = ConfigurationStructsFixture;
TEST_F(LolaServiceInstanceDeploymentEqualityFixture, ComparingSameDeploymentsReturnsTrue)
{
    // Given two LolaServiceInstanceDeployments containing the same data
    const LolaServiceInstanceDeployment unit{1U,
                                             {{"same_event_name", kDummyLolaEventInstanceDeployment}},
                                             {{"same_field_name", kDummyLolaFieldInstanceDeployment}},
                                             true,
                                             kAllowedConsumers,
                                             kAllowedProviders};
    const LolaServiceInstanceDeployment unit2{1U,
                                              {{"same_event_name", kDummyLolaEventInstanceDeployment}},
                                              {{"same_field_name", kDummyLolaFieldInstanceDeployment}},
                                              true,
                                              kAllowedConsumers,
                                              kAllowedProviders};

    // When comparing the two
    const auto are_equal = unit == unit2;

    // Then the result is true
    EXPECT_TRUE(are_equal);
}

class LolaServiceInstanceDeploymentEqualityParamaterisedFixture
    : public ::testing::TestWithParam<std::pair<LolaServiceInstanceDeployment, LolaServiceInstanceDeployment>>
{
};

TEST_P(LolaServiceInstanceDeploymentEqualityParamaterisedFixture, DifferentDeploymentsAreNotEqual)
{
    // Given 2 TransactionLogIds containing different values
    const auto [lola_service_type_deployment_1, lola_service_type_deployment_2] = GetParam();

    // When comparing the two
    const auto comparison_result = lola_service_type_deployment_1 == lola_service_type_deployment_2;

    // Then the result is false
    EXPECT_FALSE(comparison_result);
}

INSTANTIATE_TEST_CASE_P(
    LolaServiceInstanceDeploymentEqualityParamaterisedFixture,
    LolaServiceInstanceDeploymentEqualityParamaterisedFixture,
    ::testing::Values(
        std::make_pair(LolaServiceInstanceDeployment{1U}, LolaServiceInstanceDeployment{2U}),

        std::make_pair(LolaServiceInstanceDeployment{1U,
                                                     {{"first_event_name", kDummyLolaEventInstanceDeployment}},
                                                     {{"same_field_name", kDummyLolaFieldInstanceDeployment}}},
                       LolaServiceInstanceDeployment{1U,
                                                     {{"second_event_name", kDummyLolaEventInstanceDeployment}},
                                                     {{"same_field_name", kDummyLolaFieldInstanceDeployment}}}),

        std::make_pair(LolaServiceInstanceDeployment{1U,
                                                     {{"same_event_name", kDummyLolaEventInstanceDeployment}},
                                                     {{"same_field_name", kDummyLolaFieldInstanceDeployment}}},
                       LolaServiceInstanceDeployment{1U,
                                                     {{"same_event_name", kDummyLolaEventInstanceDeployment2}},
                                                     {{"same_field_name", kDummyLolaFieldInstanceDeployment}}}),

        std::make_pair(LolaServiceInstanceDeployment{1U,
                                                     {{"same_event_name", kDummyLolaEventInstanceDeployment}},
                                                     {{"same_field_name", kDummyLolaFieldInstanceDeployment}}},
                       LolaServiceInstanceDeployment{1U,
                                                     {{"same_event_name", kDummyLolaEventInstanceDeployment}},
                                                     {{"same_field_name", kDummyLolaFieldInstanceDeployment2}}}),

        std::make_pair(LolaServiceInstanceDeployment{1U, {}, {}, true},
                       LolaServiceInstanceDeployment{1U, {}, {}, false}),

        std::make_pair(LolaServiceInstanceDeployment{1U, {}, {}, true, kAllowedConsumers2, kAllowedProviders},
                       LolaServiceInstanceDeployment{1U, {}, {}, true, kAllowedConsumers, kAllowedProviders}),

        std::make_pair(LolaServiceInstanceDeployment{1U, {}, {}, true, kAllowedConsumers, kAllowedProviders2},
                       LolaServiceInstanceDeployment{1U, {}, {}, true, kAllowedConsumers, kAllowedProviders})));

}  // namespace
}  // namespace score::mw::com::impl
