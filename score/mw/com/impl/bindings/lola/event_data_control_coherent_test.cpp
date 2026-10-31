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
#include "score/mw/com/impl/bindings/lola/event_data_control_coherent.h"

#include "score/mw/com/impl/bindings/lola/coherent_event_slot_status.h"
#include "score/mw/com/impl/bindings/lola/test_doubles/fake_memory_resource.h"

#include "score/containers/dynamic_array.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl::lola
{
namespace
{

TEST(EventDataControlCoherentTest, UsesADynamicArrayToRepresentSlots)
{
    // Our detailed design (aas/lib/memory/design/shared_memory/OffsetPtrDesign.md#dynamic-array-considerations)
    // requires that we use a DynamicArray to represent our slots so that bounds checking is done.
    static_assert(
        std::is_same_v<
            score::containers::DynamicArray<CoherentControlSlotType,
                                            memory::shared::PolymorphicOffsetPtrAllocator<CoherentControlSlotType>>,
            EventDataControlCoherent::EventControlSlots>,
        "EventDataControlCoherent should use a dynamic array to represent slots.");
}

TEST(EventDataControlCoherentTest, ConstructionAllocatesGivenNumberOfSlotsAndNewestIndexIsEmpty)
{
    FakeMemoryResource memory{};
    constexpr SlotIndexType kMaxSlots{5U};

    const EventDataControlCoherent unit{kMaxSlots, memory};

    EXPECT_EQ(unit.state_slots_.size(), kMaxSlots);
    EXPECT_FALSE(unit.newest_index_.has_value());
}

}  // namespace
}  // namespace score::mw::com::impl::lola
