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
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"

#include "lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_event_id.h"
#include "score/mw/com/impl/configuration/lola_field_id.h"
#include "score/mw/com/impl/configuration/test/configuration_test_resources.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace score::mw::com::impl
{
namespace
{

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

const LolaServiceId kLolaServiceId{123U};
const std::string kDummyEventName{"my_dummy_event"};
const std::string kDummyFieldName{"my_dummy_field"};
const LolaEventId kDummyLolaEventId{456U};
const LolaFieldId kDummyLolaFieldId{678U};

using LolaServiceTypeDeploymentFixture = ConfigurationStructsFixture;
TEST_F(LolaServiceTypeDeploymentFixture, CanCreateFromSerializedObject)
{
    const LolaServiceTypeDeployment unit{MakeLolaServiceTypeDeployment()};

    const auto serialized_unit{unit.Serialize()};

    LolaServiceTypeDeployment reconstructed_unit{serialized_unit};

    ExpectLolaServiceTypeDeploymentObjectsEqual(reconstructed_unit, unit);
}

TEST_F(LolaServiceTypeDeploymentFixture, ComparingSameDeploymentsReturnsTrue)
{
    // When comparing two LolaServiceTypeDeployments containing the same data
    const LolaServiceTypeDeployment unit{MakeLolaServiceTypeDeployment(kLolaServiceId)};
    const LolaServiceTypeDeployment unit2{MakeLolaServiceTypeDeployment(kLolaServiceId)};
    const auto are_equal = unit == unit2;

    // Then the result is true
    EXPECT_TRUE(are_equal);
}

TEST_F(LolaServiceTypeDeploymentFixture, ComparingDifferentDeploymentsReturnsFalse)
{
    // When comparing two LolaServiceTypeDeployments containing different data
    const LolaServiceTypeDeployment unit{MakeLolaServiceTypeDeployment(kLolaServiceId)};
    const LolaServiceTypeDeployment unit2{MakeLolaServiceTypeDeployment(kLolaServiceId + 1U)};
    const auto are_equal = unit == unit2;

    // Then the result is false
    EXPECT_FALSE(are_equal);
}

using LolaServiceTypeDeploymentGetServiceElementFixture = ConfigurationStructsFixture;
TEST_F(LolaServiceTypeDeploymentGetServiceElementFixture, ReturnsEventIdThatExistsInDeployment)
{
    // Given a LolaServiceTypeDeployment containing an event and field
    const LolaServiceTypeDeployment unit{
        kLolaServiceId, {{kDummyEventName, kDummyLolaEventId}}, {{kDummyFieldName, kDummyLolaFieldId}}};

    // When getting a LolaEventId using the correct event name
    const auto& event_id = GetServiceElementId<ServiceElementType::EVENT>(unit, kDummyEventName);

    // Then the returned object is the same as the one in the configuration
    EXPECT_EQ(event_id, kDummyLolaEventId);
}

TEST_F(LolaServiceTypeDeploymentGetServiceElementFixture, ReturnsFieldIdThatExistsInDeployment)
{
    // Given a LolaServiceTypeDeployment containing an event and field
    const LolaServiceTypeDeployment unit{
        kLolaServiceId, {{kDummyEventName, kDummyLolaEventId}}, {{kDummyFieldName, kDummyLolaFieldId}}};

    // When getting a LolaFieldId using the correct field name
    const auto& field_id = GetServiceElementId<ServiceElementType::FIELD>(unit, kDummyFieldName);

    // Then the returned object is the same as the one in the configuration
    EXPECT_EQ(field_id, kDummyLolaFieldId);
}

using LolaServiceTypeDeploymentGetServiceElementDeathTest = LolaServiceTypeDeploymentGetServiceElementFixture;
TEST_F(LolaServiceTypeDeploymentGetServiceElementDeathTest, GettingEventIdThatDoesNotExistInDeploymentTerminates)
{
    // Given a LolaServiceTypeDeployment containing an event and field
    const LolaServiceTypeDeployment unit{
        kLolaServiceId, {{kDummyEventName, kDummyLolaEventId}}, {{kDummyFieldName, kDummyLolaFieldId}}};

    // When getting a LolaEventId using an incorrect event name
    // Then the program termintaes
    EXPECT_DEATH(score::cpp::ignore = GetServiceElementId<ServiceElementType::EVENT>(unit, kDummyFieldName), ".*");
}

TEST_F(LolaServiceTypeDeploymentGetServiceElementDeathTest, GettingFieldIdThatDoesNotExistInDeploymentTerminates)
{
    // Given a LolaServiceTypeDeployment containing an event and field
    const LolaServiceTypeDeployment unit{
        kLolaServiceId, {{kDummyEventName, kDummyLolaEventId}}, {{kDummyFieldName, kDummyLolaFieldId}}};

    // When getting a LolaFieldId using an incorrect field name
    // Then the program termintaes
    EXPECT_DEATH(score::cpp::ignore = GetServiceElementId<ServiceElementType::FIELD>(unit, kDummyEventName), ".*");
}

TEST(LolaServiceTypeDeploymentDeathTest, CreatingFromSerializedObjectWithMismatchedSerializationVersionTerminates)
{
    const LolaServiceTypeDeployment unit{MakeLolaServiceTypeDeployment()};

    const auto serialization_version_key = "serializationVersion";
    const std::uint32_t invalid_serialization_version = LolaServiceTypeDeployment::serializationVersion + 1;

    auto serialized_unit{unit.Serialize()};
    auto it = serialized_unit.find(serialization_version_key);
    ASSERT_NE(it, serialized_unit.end());
    it->second = json::Any{invalid_serialization_version};

    EXPECT_DEATH(LolaServiceTypeDeployment reconstructed_unit{serialized_unit}, ".*");
}

class LolaServiceTypeDeploymentHashFixture
    : public ::testing::TestWithParam<std::tuple<LolaServiceTypeDeployment, std::string_view>>
{
};

TEST_P(LolaServiceTypeDeploymentHashFixture, ToHashString)
{
    const auto param_tuple = GetParam();

    const auto unit = std::get<LolaServiceTypeDeployment>(param_tuple);
    const auto expected_hash_string = std::get<std::string_view>(param_tuple);

    const auto actual_hash_string = unit.ToHashString();

    EXPECT_EQ(actual_hash_string, expected_hash_string);
    EXPECT_EQ(actual_hash_string.size(), LolaServiceTypeDeployment::hashStringSize);
}

const std::vector<std::tuple<LolaServiceTypeDeployment, std::string_view>> instance_id_to_hash_string_variations{
    {LolaServiceTypeDeployment{0U}, "0000"},
    {LolaServiceTypeDeployment{1U}, "0001"},
    {LolaServiceTypeDeployment{10U}, "000a"},
    {LolaServiceTypeDeployment{255U}, "00ff"},
    {LolaServiceTypeDeployment{std::numeric_limits<LolaServiceId>::max()}, "ffff"}};
INSTANTIATE_TEST_SUITE_P(LolaServiceTypeDeploymentHashFixture,
                         LolaServiceTypeDeploymentHashFixture,
                         ::testing::ValuesIn(instance_id_to_hash_string_variations));

}  // namespace
}  // namespace score::mw::com::impl
