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
#include "score/mw/com/impl/configuration/lola_event_instance_deployment.h"

#include "score/mw/com/impl/configuration/test/configuration_test_resources.h"

#include <gtest/gtest.h>
#include <cstdint>
#include <limits>

namespace score::mw::com::impl
{
namespace
{

using LolaEventInstanceDeploymentFixture = ConfigurationStructsFixture;
TEST_F(LolaEventInstanceDeploymentFixture, CanCreateFromSerializedObjectWithOptionals)
{
    LolaEventInstanceDeployment unit{MakeLolaEventInstanceDeployment()};

    const auto serialized_unit{unit.Serialize()};

    LolaEventInstanceDeployment reconstructed_unit{serialized_unit};

    ExpectLolaEventInstanceDeploymentObjectsEqual(reconstructed_unit, unit);
}

TEST_F(LolaEventInstanceDeploymentFixture, CanCreateFromSerializedObjectWithoutOptionals)
{
    const std::optional<std::uint16_t> number_of_sample_slots{};
    const std::optional<std::uint8_t> max_subscribers{};
    const std::optional<std::uint8_t> max_concurrent_allocations{};
    const bool enforce_max_samples{true};
    constexpr std::uint8_t number_of_tracing_slots{0U};

    LolaEventInstanceDeployment unit{MakeLolaEventInstanceDeployment(number_of_sample_slots,
                                                                     max_subscribers,
                                                                     max_concurrent_allocations,
                                                                     enforce_max_samples,
                                                                     number_of_tracing_slots)};

    const auto serialized_unit{unit.Serialize()};

    LolaEventInstanceDeployment reconstructed_unit{serialized_unit};

    ExpectLolaEventInstanceDeploymentObjectsEqual(reconstructed_unit, unit);
}

TEST(LolaEventInstanceDeploymentDeathTest, CreatingFromSerializedObjectWithMismatchedSerializationVersionTerminates)
{
    LolaEventInstanceDeployment unit{MakeLolaEventInstanceDeployment()};

    const auto serialization_version_key = "serializationVersion";
    const std::uint32_t invalid_serialization_version = LolaEventInstanceDeployment::serializationVersion + 1;

    auto serialized_unit{unit.Serialize()};
    auto it = serialized_unit.find(serialization_version_key);
    ASSERT_NE(it, serialized_unit.end());
    it->second = json::Any{invalid_serialization_version};

    EXPECT_DEATH(LolaEventInstanceDeployment reconstructed_unit{serialized_unit}, ".*");
}

TEST(LolaEventInstanceDeploymentEqualityTest, EqualityOperatorForEqualStructs)
{
    const std::uint16_t number_of_sample_slots{};
    const std::optional<std::uint8_t> max_subscribers{};
    const std::optional<std::uint8_t> max_concurrent_allocations{};
    const bool enforce_max_samples{};
    constexpr std::uint8_t number_of_tracing_slots{0U};

    LolaEventInstanceDeployment unit{MakeLolaEventInstanceDeployment(number_of_sample_slots,
                                                                     max_subscribers,
                                                                     max_concurrent_allocations,
                                                                     enforce_max_samples,
                                                                     number_of_tracing_slots)};
    LolaEventInstanceDeployment unit_2{MakeLolaEventInstanceDeployment(number_of_sample_slots,
                                                                       max_subscribers,
                                                                       max_concurrent_allocations,
                                                                       enforce_max_samples,
                                                                       number_of_tracing_slots)};

    EXPECT_TRUE(unit == unit_2);
}

class LolaEventInstanceDeploymentEqualityFixture
    : public ::testing::TestWithParam<std::pair<LolaEventInstanceDeployment, LolaEventInstanceDeployment>>
{
};

TEST_P(LolaEventInstanceDeploymentEqualityFixture, EqualityOperatorForUnequalStructs)
{
    const auto param_pair = GetParam();

    const auto unit = param_pair.first;
    const auto unit_2 = param_pair.second;

    EXPECT_FALSE(unit == unit_2);
}

INSTANTIATE_TEST_SUITE_P(LolaEventInstanceDeploymentEqualityFixture,
                         LolaEventInstanceDeploymentEqualityFixture,
                         ::testing::ValuesIn({std::make_pair(LolaEventInstanceDeployment{10U, 11U, 12U, true, 1},
                                                             LolaEventInstanceDeployment{11U, 11U, 12U, true, 1}),
                                              std::make_pair(LolaEventInstanceDeployment{10U, 11U, 12U, true, 1},
                                                             LolaEventInstanceDeployment{10U, 12U, 12U, true, 1}),
                                              std::make_pair(LolaEventInstanceDeployment{10U, 11U, 12U, true, 1},
                                                             LolaEventInstanceDeployment{10U, 11U, 13U, true, 1}),
                                              std::make_pair(LolaEventInstanceDeployment{10U, 11U, 12U, true, 1},
                                                             LolaEventInstanceDeployment{10U, 11U, 12U, false, 1}),
                                              std::make_pair(LolaEventInstanceDeployment{10U, 11U, 12U, true, 1},
                                                             LolaEventInstanceDeployment{10U, 11U, 12U, true, 0})}));

TEST(LolaEventInstanceDeploymentGetSlotsTest, GetNumberOfSampleSlotsExcludingTracingSlotReturnOptionalByDefault)
{
    auto unit = MakeDefaultLolaEventInstanceDeployment();

    EXPECT_FALSE(unit.GetNumberOfSampleSlots().has_value());
    EXPECT_FALSE(unit.GetNumberOfSampleSlotsExcludingTracingSlot().has_value());
}

TEST(LolaEventInstanceDeploymentGetSlotsTracingEnabledTest, GetNumberOfSampleSlotsReturnsSetValue)
{
    auto unit = LolaEventInstanceDeployment({}, {}, 1U, true, 1);

    const std::uint16_t set_number_of_sample_slots{10U};
    unit.SetNumberOfSampleSlots(set_number_of_sample_slots);

    const auto number_of_sample_slots = unit.GetNumberOfSampleSlots();
    ASSERT_TRUE(number_of_sample_slots.has_value());
    EXPECT_EQ(number_of_sample_slots.value(), set_number_of_sample_slots + 1);

    const auto number_of_sample_slots_excluding_tracing = unit.GetNumberOfSampleSlotsExcludingTracingSlot();
    ASSERT_TRUE(number_of_sample_slots_excluding_tracing.has_value());
    EXPECT_EQ(number_of_sample_slots_excluding_tracing.value(), set_number_of_sample_slots);
}

TEST(LolaEventInstanceDeploymentGetSlotsTracingDisabledTest, GetNumberOfSampleSlotsReturnsSetValue)
{
    auto unit = MakeDefaultLolaEventInstanceDeployment();

    const std::uint16_t set_number_of_sample_slots{10U};
    unit.SetNumberOfSampleSlots(set_number_of_sample_slots);

    const auto number_of_sample_slots = unit.GetNumberOfSampleSlots();
    ASSERT_TRUE(number_of_sample_slots.has_value());
    EXPECT_EQ(number_of_sample_slots.value(), set_number_of_sample_slots);

    const auto number_of_sample_slots_excluding_tracing = unit.GetNumberOfSampleSlotsExcludingTracingSlot();
    ASSERT_TRUE(number_of_sample_slots_excluding_tracing.has_value());
    EXPECT_EQ(number_of_sample_slots_excluding_tracing.value(), set_number_of_sample_slots);
}

TEST(LolaEventInstanceDeploymentGetSlotsDeathTest,
     GetNumberOfSampleSlotsTerminatesWhenCombinedNumberOfSlotsExceedsMaxValue)
{
    // Given a LolaEventInstanceDeployment whose combined number of stample slots and number of tracing slots would
    // overflow a std::uint16_t
    constexpr auto max_number_of_sample_slots = std::numeric_limits<std::uint16_t>::max();
    constexpr auto number_of_tracing_slots = 1U;
    LolaEventInstanceDeployment unit{max_number_of_sample_slots, {}, 1U, true, number_of_tracing_slots};

    // When calling GetNumberOfSampleSlots
    // Then the program terminates
    EXPECT_DEATH(score::cpp::ignore = unit.GetNumberOfSampleSlots(), ".*");
}

}  // namespace
}  // namespace score::mw::com::impl
