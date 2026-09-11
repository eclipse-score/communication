/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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
#include "score/mw/com/impl/bindings/someip/slot_allocation_control.h"

#include <gtest/gtest.h>

#include <set>

namespace score::mw::com::impl::someip
{
namespace
{

constexpr std::size_t kNumberOfSlots{3U};

TEST(SomeIpSlotAllocationControlTest, NoSlotCanBeAllocatedBeforeReset)
{
    SlotAllocationControl unit{};

    EXPECT_EQ(unit.GetNumberOfSlots(), 0U);
    EXPECT_FALSE(unit.AllocateSlot().has_value());
}

TEST(SomeIpSlotAllocationControlTest, ResetMakesEverySlotAllocatableExactlyOnce)
{
    SlotAllocationControl unit{};
    unit.Reset(kNumberOfSlots);
    ASSERT_EQ(unit.GetNumberOfSlots(), kNumberOfSlots);

    std::set<SlotIndexType> allocated_slots{};
    for (std::size_t slot = 0U; slot < kNumberOfSlots; ++slot)
    {
        const auto slot_index = unit.AllocateSlot();
        ASSERT_TRUE(slot_index.has_value());
        EXPECT_TRUE(allocated_slots.insert(slot_index.value()).second) << "slot handed out twice";
    }

    EXPECT_FALSE(unit.AllocateSlot().has_value());
}

TEST(SomeIpSlotAllocationControlTest, DiscardedSlotBecomesAllocatableAgain)
{
    SlotAllocationControl unit{};
    unit.Reset(kNumberOfSlots);
    for (std::size_t slot = 0U; slot < kNumberOfSlots; ++slot)
    {
        ASSERT_TRUE(unit.AllocateSlot().has_value());
    }
    ASSERT_FALSE(unit.AllocateSlot().has_value());

    unit.DiscardSlot(1U);

    const auto reallocated_slot = unit.AllocateSlot();
    ASSERT_TRUE(reallocated_slot.has_value());
    EXPECT_EQ(reallocated_slot.value(), 1U);
    EXPECT_FALSE(unit.AllocateSlot().has_value());
}

TEST(SomeIpSlotAllocationControlTest, DiscardingTheSameSlotTwiceDoesNotFreeAnAdditionalSlot)
{
    SlotAllocationControl unit{};
    unit.Reset(kNumberOfSlots);
    for (std::size_t slot = 0U; slot < kNumberOfSlots; ++slot)
    {
        ASSERT_TRUE(unit.AllocateSlot().has_value());
    }

    unit.DiscardSlot(0U);
    unit.DiscardSlot(0U);

    // Exactly one slot was returned, so exactly one further allocation must succeed.
    ASSERT_TRUE(unit.AllocateSlot().has_value());
    EXPECT_FALSE(unit.AllocateSlot().has_value());
}

TEST(SomeIpSlotAllocationControlTest, DiscardingAnOutOfRangeSlotIsIgnored)
{
    SlotAllocationControl unit{};
    unit.Reset(kNumberOfSlots);
    ASSERT_TRUE(unit.AllocateSlot().has_value());

    unit.DiscardSlot(static_cast<SlotIndexType>(kNumberOfSlots));
    unit.DiscardSlot(static_cast<SlotIndexType>(kNumberOfSlots + 100U));

    // The two remaining slots are still the only allocatable ones.
    ASSERT_TRUE(unit.AllocateSlot().has_value());
    ASSERT_TRUE(unit.AllocateSlot().has_value());
    EXPECT_FALSE(unit.AllocateSlot().has_value());
}

TEST(SomeIpSlotAllocationControlTest, ClearDropsAllSlots)
{
    SlotAllocationControl unit{};
    unit.Reset(kNumberOfSlots);

    unit.Clear();

    EXPECT_EQ(unit.GetNumberOfSlots(), 0U);
    EXPECT_FALSE(unit.AllocateSlot().has_value());
}

TEST(SomeIpSlotAllocationControlTest, ResetReleasesPreviouslyAllocatedSlots)
{
    SlotAllocationControl unit{};
    unit.Reset(kNumberOfSlots);
    for (std::size_t slot = 0U; slot < kNumberOfSlots; ++slot)
    {
        ASSERT_TRUE(unit.AllocateSlot().has_value());
    }
    ASSERT_FALSE(unit.AllocateSlot().has_value());

    unit.Reset(kNumberOfSlots);

    for (std::size_t slot = 0U; slot < kNumberOfSlots; ++slot)
    {
        EXPECT_TRUE(unit.AllocateSlot().has_value()) << "at slot " << slot;
    }
}

}  // namespace
}  // namespace score::mw::com::impl::someip
