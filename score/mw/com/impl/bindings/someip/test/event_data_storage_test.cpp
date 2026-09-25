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
#include "score/mw/com/impl/bindings/someip/event_data_storage.h"

#include "score/memory/data_type_size_info.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <set>

namespace score::mw::com::impl::someip
{
namespace
{

using TestSampleType = std::uint32_t;

constexpr SlotIndexType kNumberOfSlots{4U};
const memory::DataTypeSizeInfo kSampleSizeInfo{sizeof(TestSampleType), alignof(TestSampleType)};

TEST(SomeIpEventDataStorageTest, GetNumberOfSlotsReturnsConfiguredValue)
{
    EventDataStorage unit{kNumberOfSlots, kSampleSizeInfo};

    EXPECT_EQ(unit.GetNumberOfSlots(), kNumberOfSlots);
}

TEST(SomeIpEventDataStorageTest, SlotsAreDistinctAndCorrectlyStrided)
{
    EventDataStorage unit{kNumberOfSlots, kSampleSizeInfo};

    std::set<const void*> slot_addresses{};
    const auto* const first_slot =
        static_cast<const std::uint8_t*>(unit.GetTypeErasedDataSlot(0U, sizeof(TestSampleType)));
    for (SlotIndexType slot_index = 0U; slot_index < kNumberOfSlots; ++slot_index)
    {
        auto* const slot = unit.GetTypeErasedDataSlot(slot_index, sizeof(TestSampleType));
        EXPECT_TRUE(slot_addresses.insert(slot).second);
        EXPECT_EQ(static_cast<const std::uint8_t*>(slot), first_slot + (slot_index * sizeof(TestSampleType)));
    }
}

TEST(SomeIpEventDataStorageTest, SlotsAreCorrectlyAligned)
{
    EventDataStorage unit{kNumberOfSlots, kSampleSizeInfo};

    for (SlotIndexType slot_index = 0U; slot_index < kNumberOfSlots; ++slot_index)
    {
        auto* const slot = unit.GetTypeErasedDataSlot(slot_index, sizeof(TestSampleType));
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(slot) % alignof(TestSampleType), 0U);
    }
}

TEST(SomeIpEventDataStorageTest, InitializeSlotsIsCalledOnceForEverySlot)
{
    EventDataStorage unit{kNumberOfSlots, kSampleSizeInfo};

    std::set<void*> initialized_slots{};
    unit.InitializeSlots(InitializeSampleCallback{[&initialized_slots](void* const sample_ptr) noexcept {
        score::cpp::ignore = initialized_slots.insert(sample_ptr);
        score::cpp::ignore = new (sample_ptr) TestSampleType{42U};
    }});

    EXPECT_EQ(initialized_slots.size(), kNumberOfSlots);
    for (SlotIndexType slot_index = 0U; slot_index < kNumberOfSlots; ++slot_index)
    {
        auto* const slot = unit.GetTypeErasedDataSlot(slot_index, sizeof(TestSampleType));
        EXPECT_EQ(*static_cast<TestSampleType*>(slot), 42U);
    }
}

TEST(SomeIpEventDataStorageTest, WrittenDataOfOneSlotDoesNotAffectOtherSlots)
{
    EventDataStorage unit{kNumberOfSlots, kSampleSizeInfo};
    unit.InitializeSlots(InitializeSampleCallback{[](void* const sample_ptr) noexcept {
        score::cpp::ignore = new (sample_ptr) TestSampleType{0U};
    }});

    *static_cast<TestSampleType*>(unit.GetTypeErasedDataSlot(2U, sizeof(TestSampleType))) = 7U;

    EXPECT_EQ(*static_cast<TestSampleType*>(unit.GetTypeErasedDataSlot(0U, sizeof(TestSampleType))), 0U);
    EXPECT_EQ(*static_cast<TestSampleType*>(unit.GetTypeErasedDataSlot(1U, sizeof(TestSampleType))), 0U);
    EXPECT_EQ(*static_cast<TestSampleType*>(unit.GetTypeErasedDataSlot(2U, sizeof(TestSampleType))), 7U);
    EXPECT_EQ(*static_cast<TestSampleType*>(unit.GetTypeErasedDataSlot(3U, sizeof(TestSampleType))), 0U);
}

TEST(SomeIpEventDataStorageDeathTest, GettingSlotOutOfBoundsTerminates)
{
    EventDataStorage unit{kNumberOfSlots, kSampleSizeInfo};

    EXPECT_DEATH(score::cpp::ignore = unit.GetTypeErasedDataSlot(kNumberOfSlots, sizeof(TestSampleType)), ".*");
}

TEST(SomeIpEventDataStorageDeathTest, GettingSlotWithMismatchingSizeTerminates)
{
    EventDataStorage unit{kNumberOfSlots, kSampleSizeInfo};

    EXPECT_DEATH(score::cpp::ignore = unit.GetTypeErasedDataSlot(0U, sizeof(TestSampleType) + 1U), ".*");
}

}  // namespace
}  // namespace score::mw::com::impl::someip
