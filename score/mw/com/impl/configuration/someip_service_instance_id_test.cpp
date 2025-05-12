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
#include "score/mw/com/impl/configuration/someip_service_instance_id.h"

#include "score/mw/com/impl/configuration/test/configuration_test_resources.h"

#include <gtest/gtest.h>
#include <limits>

namespace score::mw::com::impl
{
namespace
{

using SomeIpServiceInstanceIdFixture = ConfigurationStructsFixture;
TEST_F(SomeIpServiceInstanceIdFixture, CanCreateFromSerializedObject)
{
    SomeIpServiceInstanceId unit{10U};

    const auto serialized_unit{unit.Serialize()};

    SomeIpServiceInstanceId reconstructed_unit{serialized_unit};

    ExpectSomeIpServiceInstanceIdObjectsEqual(reconstructed_unit, unit);
}

TEST(SomeIpServiceInstanceIdTest, CanBeCopiedAndEqualCompared)
{
    const auto unit = SomeIpServiceInstanceId{10U};
    const auto unitCopy = unit;

    ASSERT_EQ(unit, unitCopy);
}

TEST(SomeIpServiceInstanceIdTest, DifferentIdsAreNotEqual)
{
    const auto unit = SomeIpServiceInstanceId{10U};
    const auto unit_2 = SomeIpServiceInstanceId{12U};

    ASSERT_FALSE(unit == unit_2);
}

TEST(SomeIpServiceInstanceIdTest, LessThanOperator)
{
    const auto unit = SomeIpServiceInstanceId{10U};
    const auto unit_2 = SomeIpServiceInstanceId{12U};

    ASSERT_TRUE(unit < unit_2);
    ASSERT_FALSE(unit_2 < unit);
}

TEST(SomeIpServiceInstanceIdDeathTest, CreatingFromSerializedObjectWithMismatchedSerializationVersionTerminates)
{
    const SomeIpServiceInstanceId unit{10U};

    const auto serialization_version_key = "serializationVersion";
    const std::uint32_t invalid_serialization_version = SomeIpServiceInstanceId::serializationVersion + 1;

    auto serialized_unit{unit.Serialize()};
    auto it = serialized_unit.find(serialization_version_key);
    ASSERT_NE(it, serialized_unit.end());
    it->second = json::Any{invalid_serialization_version};

    EXPECT_DEATH(SomeIpServiceInstanceId reconstructed_unit{serialized_unit}, ".*");
}

class SomeIpServiceInstanceIdHashFixture
    : public ::testing::TestWithParam<std::tuple<SomeIpServiceInstanceId, std::string_view>>
{
};

TEST_P(SomeIpServiceInstanceIdHashFixture, ToHashString)
{
    const auto param_tuple = GetParam();

    const auto unit = std::get<SomeIpServiceInstanceId>(param_tuple);
    const auto expected_hash_string = std::get<std::string_view>(param_tuple);

    const auto actual_hash_string = unit.ToHashString();

    EXPECT_EQ(actual_hash_string, expected_hash_string);
    EXPECT_EQ(actual_hash_string.size(), SomeIpServiceInstanceId::hashStringSize);
}

const std::vector<std::tuple<SomeIpServiceInstanceId, std::string_view>> instance_id_to_hash_string_variations{
    {SomeIpServiceInstanceId{0U}, "0000"},
    {SomeIpServiceInstanceId{1U}, "0001"},
    {SomeIpServiceInstanceId{10U}, "000a"},
    {SomeIpServiceInstanceId{255U}, "00ff"},
    {SomeIpServiceInstanceId{std::numeric_limits<SomeIpServiceInstanceId::InstanceId>::max()}, "ffff"}};
INSTANTIATE_TEST_SUITE_P(SomeIpServiceInstanceIdHashFixture,
                         SomeIpServiceInstanceIdHashFixture,
                         ::testing::ValuesIn(instance_id_to_hash_string_variations));

}  // namespace
}  // namespace score::mw::com::impl
