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
#include "score/memory/shared/memory_region_bounds.h"

#include <gtest/gtest.h>
#include <cstdint>

namespace score::memory::shared::test
{
namespace
{

using namespace ::testing;

constexpr std::uintptr_t kValidStartAddress{10U};
constexpr std::uintptr_t kValidEndAddress{20U};

constexpr std::uintptr_t kInvalidStartAddress{0U};
constexpr std::uintptr_t kInvalidEndAddress{0U};

TEST(MemoryRegionBoundsTest, DefaultConstructingHasNoValue)
{
    // When default constructing a MemoryRegionBounds
    const MemoryRegionBounds memory_region_bounds{};

    // Then it has no value
    EXPECT_FALSE(memory_region_bounds.has_value());
}

TEST(MemoryRegionBoundsTest, ConstructingWithValuesMarksHasValue)
{
    // When constructing a MemoryRegionBounds with valid bounds
    const MemoryRegionBounds memory_region_bounds{kValidStartAddress, kValidEndAddress};

    // Then it has a value
    EXPECT_TRUE(memory_region_bounds.has_value());
}

TEST(MemoryRegionBoundsTest, ConstructingWithInvalidValuesMarksHasNoValue)
{
    // When constructing a MemoryRegionBounds with invalid bounds (representing nullptrs)
    const MemoryRegionBounds memory_region_bounds{kInvalidStartAddress, kInvalidEndAddress};

    // Then it has no value
    EXPECT_FALSE(memory_region_bounds.has_value());
}

TEST(MemoryRegionBoundsTest, GettingAddressesReturnsValuesPassedToConstructor)
{
    // Given a MemoryRegionBounds constructed with valid bounds
    const MemoryRegionBounds memory_region_bounds{kValidStartAddress, kValidEndAddress};

    // When getting the start and end addresses
    const auto actual_start_address = memory_region_bounds.GetStartAddress();
    const auto actual_end_address = memory_region_bounds.GetEndAddress();

    // Then the addresses are the same as those passed to the constructor
    EXPECT_EQ(actual_start_address, kValidStartAddress);
    EXPECT_EQ(actual_end_address, kValidEndAddress);
}

TEST(MemoryRegionBoundsTest, SettingAddressesUpdatesAddresses)
{
    // Given a default constructed MemoryRegionBounds
    MemoryRegionBounds memory_region_bounds{};

    // When setting the addresses with valid values
    memory_region_bounds.Set(kValidStartAddress, kValidEndAddress);

    // Then the addresses are the same as those passed to the Set function
    const auto actual_start_address = memory_region_bounds.GetStartAddress();
    const auto actual_end_address = memory_region_bounds.GetEndAddress();
    EXPECT_EQ(actual_start_address, kValidStartAddress);
    EXPECT_EQ(actual_end_address, kValidEndAddress);
}

TEST(MemoryRegionBoundsTest, SettingMarksHasValue)
{
    // Given a default constructed MemoryRegionBounds
    MemoryRegionBounds memory_region_bounds{};

    // When setting the addresses with valid values
    memory_region_bounds.Set(kValidStartAddress, kValidEndAddress);

    // Then it has a value
    EXPECT_TRUE(memory_region_bounds.has_value());
}

TEST(MemoryRegionBoundsTest, ResettingClearsAddresses)
{
    // Given a MemoryRegionBounds constructed with valid bounds
    MemoryRegionBounds memory_region_bounds{kValidStartAddress, kValidEndAddress};

    // When calling Reset
    memory_region_bounds.Reset();

    // Then the addresses are invalid
    const auto actual_start_address = memory_region_bounds.GetStartAddress();
    const auto actual_end_address = memory_region_bounds.GetEndAddress();
    EXPECT_EQ(actual_start_address, kInvalidStartAddress);
    EXPECT_EQ(actual_end_address, kInvalidEndAddress);
}

TEST(MemoryRegionBoundsTest, ResettingMarksHasNoValue)
{
    // Given a MemoryRegionBounds constructed with valid bounds
    MemoryRegionBounds memory_region_bounds{kValidStartAddress, kValidEndAddress};

    // When calling Reset
    memory_region_bounds.Reset();

    // Then it has no value
    EXPECT_FALSE(memory_region_bounds.has_value());
}

TEST(MemoryRegionBoundsDeathTest, ConstructingWithOneValidAndOneInvalidValueTerminates)
{
    // When constructing a MemoryRegionBounds with one valid and one invalid address
    // Then we terminate
    EXPECT_DEATH(MemoryRegionBounds(kValidStartAddress, kInvalidEndAddress), ".*");
}

TEST(MemoryRegionBoundsDeathTest, SettingOneValidAndOneInvalidValueTerminates)
{
    // Given a default constructed MemoryRegionBounds
    MemoryRegionBounds memory_region_bounds{};

    // When setting the addresses with one valid and one invalid address
    // Then we terminate
    EXPECT_DEATH(memory_region_bounds.Set(kValidStartAddress, kInvalidEndAddress), ".*");
}

TEST(MemoryRegionBoundsEqualToOperatorTest, ComparingTwoMemoryRegionBoundsWithSameValidAddressesReturnsTrue)
{
    // Given two MemoryRegionBounds constructed with the same valid bounds
    const MemoryRegionBounds memory_region_bounds_1{kValidStartAddress, kValidEndAddress};
    const MemoryRegionBounds memory_region_bounds_2{kValidStartAddress, kValidEndAddress};

    // When comparing the two
    // Then the result is true
    EXPECT_TRUE(memory_region_bounds_1 == memory_region_bounds_2);
    EXPECT_FALSE(memory_region_bounds_1 != memory_region_bounds_2);
}

TEST(MemoryRegionBoundsEqualToOperatorTest, ComparingTwoMemoryRegionBoundsWithDifferentStartAddressesReturnsFalse)
{
    // Given two MemoryRegionBounds constructed with the same valid end address but different start addresses
    const MemoryRegionBounds memory_region_bounds_1{kValidStartAddress, kValidEndAddress};
    const MemoryRegionBounds memory_region_bounds_2{kValidStartAddress + 1U, kValidEndAddress};

    // When comparing the two
    // Then the result is false
    EXPECT_FALSE(memory_region_bounds_1 == memory_region_bounds_2);
    EXPECT_TRUE(memory_region_bounds_1 != memory_region_bounds_2);
}

TEST(MemoryRegionBoundsEqualToOperatorTest, ComparingTwoMemoryRegionBoundsWithDifferentEndAddressesReturnsFalse)
{
    // Given two MemoryRegionBounds constructed with the same valid start address but different end addresses
    const MemoryRegionBounds memory_region_bounds_1{kValidStartAddress, kValidEndAddress};
    const MemoryRegionBounds memory_region_bounds_2{kValidStartAddress, kValidEndAddress + 1U};

    // When comparing the two
    // Then the result is false
    EXPECT_FALSE(memory_region_bounds_1 == memory_region_bounds_2);
    EXPECT_TRUE(memory_region_bounds_1 != memory_region_bounds_2);
}

}  // namespace
}  // namespace score::memory::shared::test
