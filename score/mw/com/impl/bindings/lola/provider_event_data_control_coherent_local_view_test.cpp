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
#include "score/mw/com/impl/bindings/lola/provider_event_data_control_coherent_local_view.h"

#include "score/mw/com/impl/bindings/lola/event_data_control_coherent.h"
#include "score/mw/com/impl/bindings/lola/test_doubles/fake_memory_resource.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl::lola
{
namespace
{

constexpr SlotIndexType kMaxSlots{5U};

class ProviderEventDataControlCoherentLocalViewFixture : public ::testing::Test
{
  public:
    FakeMemoryResource memory_{};
    EventDataControlCoherent event_data_control_{kMaxSlots, memory_};
    ProviderEventDataControlCoherentLocalView unit_{event_data_control_};
};

TEST_F(ProviderEventDataControlCoherentLocalViewFixture, AllocateNextSlotReturnsZeroWhenNothingPublishedYet)
{
    EXPECT_EQ(unit_.AllocateNextSlot(), 0U);
}

TEST_F(ProviderEventDataControlCoherentLocalViewFixture, EventReadySetsTimestampAndAdvancesNewestIndex)
{
    const auto slot_index = unit_.AllocateNextSlot();
    unit_.EventReady(slot_index, 1U);

    EXPECT_EQ(unit_[slot_index].GetTimeStamp(), 1U);
    EXPECT_EQ(event_data_control_.newest_index_, slot_index);
}

TEST_F(ProviderEventDataControlCoherentLocalViewFixture, AllocateNextSlotWrapsAroundRingBuffer)
{
    for (SlotIndexType i = 0U; i < kMaxSlots; ++i)
    {
        const auto slot_index = unit_.AllocateNextSlot();
        EXPECT_EQ(slot_index, i);
        unit_.EventReady(slot_index, static_cast<CoherentEventSlotStatus::EventTimeStamp>(i + 1U));
    }

    // after kMaxSlots sends, the next slot wraps back around to 0
    EXPECT_EQ(unit_.AllocateNextSlot(), 0U);
}

TEST_F(ProviderEventDataControlCoherentLocalViewFixture, DiscardIsANoOp)
{
    const auto slot_index = unit_.AllocateNextSlot();
    unit_.Discard(slot_index);

    // Discard() must not alter the slot's state (not even mark it invalid), as AllocateNextSlot() never claimed any
    // state to begin with.
    EXPECT_TRUE(unit_[slot_index].IsInvalid());
    EXPECT_FALSE(event_data_control_.newest_index_.has_value());
}

}  // namespace
}  // namespace score::mw::com::impl::lola
