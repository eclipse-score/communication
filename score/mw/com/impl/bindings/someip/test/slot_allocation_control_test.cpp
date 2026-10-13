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

TEST(SomeIpSlotAllocationControlTest, DefaultConstructor_AllocateSlot_NoSlotIsReturned)
{
    // Given a default control
    SlotAllocationControl unit{};

    // When allocating a slot, then none is available
    EXPECT_EQ(unit.GetNumberOfSlots(), 0U);
    EXPECT_FALSE(unit.AllocateSlot().has_value());
}

TEST(SomeIpSlotAllocationControlTest, DefaultControl_ResetWithThreeSlots_EachSlotIsAllocatedExactlyOnce)
{
    // Given a control reset with three slots
    SlotAllocationControl unit{};
    unit.Reset(kNumberOfSlots);
    ASSERT_EQ(unit.GetNumberOfSlots(), kNumberOfSlots);

    // When allocating, then each slot is returned once before exhaustion
    std::set<SlotIndexType> allocated_slots{};
    for (std::size_t slot = 0U; slot < kNumberOfSlots; ++slot)
    {
        const auto slot_index = unit.AllocateSlot();
        ASSERT_TRUE(slot_index.has_value());
        EXPECT_TRUE(allocated_slots.insert(slot_index.value()).second) << "slot handed out twice";
    }

    EXPECT_FALSE(unit.AllocateSlot().has_value());
}

TEST(SomeIpSlotAllocationControlTest, AllThreeSlotsAllocated_DiscardSlotOne_SlotOneCanBeAllocatedAgain)
{
    // Given an exhausted control
    SlotAllocationControl unit{};
    unit.Reset(kNumberOfSlots);
    for (std::size_t slot = 0U; slot < kNumberOfSlots; ++slot)
    {
        ASSERT_TRUE(unit.AllocateSlot().has_value());
    }
    ASSERT_FALSE(unit.AllocateSlot().has_value());

    // When discarding slot one
    unit.DiscardSlot(1U);

    // Then only slot one can be allocated again
    const auto reallocated_slot = unit.AllocateSlot();
    ASSERT_TRUE(reallocated_slot.has_value());
    EXPECT_EQ(reallocated_slot.value(), 1U);
    EXPECT_FALSE(unit.AllocateSlot().has_value());
}

TEST(SomeIpSlotAllocationControlTest, AllSlotsAllocated_DiscardSlotZeroTwice_OnlyOneSlotBecomesAvailable)
{
    // Given an exhausted control
    SlotAllocationControl unit{};
    unit.Reset(kNumberOfSlots);
    for (std::size_t slot = 0U; slot < kNumberOfSlots; ++slot)
    {
        ASSERT_TRUE(unit.AllocateSlot().has_value());
    }

    // When discarding the same slot twice
    unit.DiscardSlot(0U);
    unit.DiscardSlot(0U);

    // Then only one allocation succeeds
    ASSERT_TRUE(unit.AllocateSlot().has_value());
    EXPECT_FALSE(unit.AllocateSlot().has_value());
}

TEST(SomeIpSlotAllocationControlTest, ControlHasThreeSlots_DiscardOutOfRangeSlots_ConfiguredSlotsRemainAllocatable)
{
    // Given a control with one of three slots allocated
    SlotAllocationControl unit{};
    unit.Reset(kNumberOfSlots);
    ASSERT_TRUE(unit.AllocateSlot().has_value());

    // When discarding out-of-range slots
    unit.DiscardSlot(static_cast<SlotIndexType>(kNumberOfSlots));
    unit.DiscardSlot(static_cast<SlotIndexType>(kNumberOfSlots + 100U));

    // Then only the two remaining slots are available
    ASSERT_TRUE(unit.AllocateSlot().has_value());
    ASSERT_TRUE(unit.AllocateSlot().has_value());
    EXPECT_FALSE(unit.AllocateSlot().has_value());
}

TEST(SomeIpSlotAllocationControlTest, ControlHasThreeSlots_Clear_NoSlotsRemainAllocatable)
{
    // Given a control with three slots
    SlotAllocationControl unit{};
    unit.Reset(kNumberOfSlots);

    // When clearing the control
    unit.Clear();

    // Then no slots remain
    EXPECT_EQ(unit.GetNumberOfSlots(), 0U);
    EXPECT_FALSE(unit.AllocateSlot().has_value());
}

TEST(SomeIpSlotAllocationControlTest, AllSlotsAllocated_ResetWithThreeSlots_AllSlotsAreAllocatableAgain)
{
    // Given an exhausted control
    SlotAllocationControl unit{};
    unit.Reset(kNumberOfSlots);
    for (std::size_t slot = 0U; slot < kNumberOfSlots; ++slot)
    {
        ASSERT_TRUE(unit.AllocateSlot().has_value());
    }
    ASSERT_FALSE(unit.AllocateSlot().has_value());

    // When resetting with three slots
    unit.Reset(kNumberOfSlots);

    // Then all three slots can be allocated again
    for (std::size_t slot = 0U; slot < kNumberOfSlots; ++slot)
    {
        EXPECT_TRUE(unit.AllocateSlot().has_value()) << "at slot " << slot;
    }
}

}  // namespace
}  // namespace score::mw::com::impl::someip
