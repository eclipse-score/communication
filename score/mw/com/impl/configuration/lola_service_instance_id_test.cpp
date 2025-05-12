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
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"

#include "score/mw/com/impl/configuration/test/configuration_test_resources.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl
{
namespace
{

using LolaServiceInstanceIdFixture = ConfigurationStructsFixture;
TEST_F(LolaServiceInstanceIdFixture, CanCreateFromSerializedObject)
{
    LolaServiceInstanceId unit{10U};

    const auto serialized_unit{unit.Serialize()};

    LolaServiceInstanceId reconstructed_unit{serialized_unit};

    ExpectLolaServiceInstanceIdObjectsEqual(reconstructed_unit, unit);
}

TEST(LolaServiceInstanceIdTest, CanBeCopiedAndEqualCompared)
{
    const auto unit = LolaServiceInstanceId{10U};
    const auto unitCopy = unit;

    ASSERT_EQ(unit, unitCopy);
}

TEST(LolaServiceInstanceIdTest, DifferentIdsAreNotEqual)
{
    const auto unit = LolaServiceInstanceId{10U};
    const auto unit_2 = LolaServiceInstanceId{12U};

    ASSERT_FALSE(unit == unit_2);
}

TEST(LolaServiceInstanceIdTest, LessThanOperator)
{
    const auto unit = LolaServiceInstanceId{10U};
    const auto unit_2 = LolaServiceInstanceId{12U};

    ASSERT_TRUE(unit < unit_2);
    ASSERT_FALSE(unit_2 < unit);
}

TEST(LolaServiceInstanceIdDeathTest, CreatingFromSerializedObjectWithMismatchedSerializationVersionTerminates)
{
    const LolaServiceInstanceId unit{10U};

    const auto serialization_version_key = "serializationVersion";
    const std::uint32_t invalid_serialization_version = LolaServiceInstanceId::serializationVersion + 1;

    auto serialized_unit{unit.Serialize()};
    auto it = serialized_unit.find(serialization_version_key);
    ASSERT_NE(it, serialized_unit.end());
    it->second = json::Any{invalid_serialization_version};

    EXPECT_DEATH(LolaServiceInstanceId reconstructed_unit{serialized_unit}, ".*");
}

class LolaServiceInstanceIdHashFixture
    : public ::testing::TestWithParam<std::tuple<LolaServiceInstanceId, std::string_view>>
{
};

TEST_P(LolaServiceInstanceIdHashFixture, ToHashString)
{
    const auto param_tuple = GetParam();

    const auto unit = std::get<LolaServiceInstanceId>(param_tuple);
    const auto expected_hash_string = std::get<std::string_view>(param_tuple);

    const auto actual_hash_string = unit.ToHashString();

    EXPECT_EQ(actual_hash_string, expected_hash_string);
    EXPECT_EQ(actual_hash_string.size(), LolaServiceInstanceId::hashStringSize);
}

const std::vector<std::tuple<LolaServiceInstanceId, std::string_view>> instance_id_to_hash_string_variations{
    {LolaServiceInstanceId{0U}, "0000"},
    {LolaServiceInstanceId{1U}, "0001"},
    {LolaServiceInstanceId{10U}, "000a"},
    {LolaServiceInstanceId{255U}, "00ff"},
    {LolaServiceInstanceId{std::numeric_limits<LolaServiceInstanceId::InstanceId>::max()}, "ffff"}};
INSTANTIATE_TEST_SUITE_P(LolaServiceInstanceIdHashFixture,
                         LolaServiceInstanceIdHashFixture,
                         ::testing::ValuesIn(instance_id_to_hash_string_variations));

}  // namespace
}  // namespace score::mw::com::impl
