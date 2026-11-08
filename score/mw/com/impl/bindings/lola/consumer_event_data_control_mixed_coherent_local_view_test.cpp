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
#include "score/mw/com/impl/bindings/lola/consumer_event_data_control_mixed_coherent_local_view.h"

#include "score/mw/com/impl/bindings/lola/event_data_control_mixed.h"
#include "score/mw/com/impl/bindings/lola/event_data_control_mixed_composite.h"
#include "score/mw/com/impl/bindings/lola/test_doubles/fake_memory_resource.h"

#include <gtest/gtest.h>

#include <array>

namespace score::mw::com::impl::lola
{
namespace
{

constexpr SlotIndexType kMaxSlots{5U};

class ConsumerEventDataControlMixedCoherentLocalViewFixture : public ::testing::Test
{
  public:
    void SendEvent(const EventSlotStatus::EventTimeStamp time_stamp)
    {
        const auto allocation_result = provider_.GetComposite().AllocateNextSlot();
        ASSERT_TRUE(allocation_result.allocated_slot_index.has_value());
        const auto slot_index = allocation_result.allocated_slot_index.value();
        provider_.EventReady(slot_index, time_stamp);
    }

    FakeMemoryResource memory_{};
    EventDataControlMixed event_data_control_mixed_{kMaxSlots, memory_};
    EventDataControlMixedComposite provider_{event_data_control_mixed_};
    ConsumerEventDataControlMixedCoherentLocalView unit_{event_data_control_mixed_};
};

TEST_F(ConsumerEventDataControlMixedCoherentLocalViewFixture, GetNewestIndexIsEmptyWhenNothingPublishedYet)
{
    EXPECT_FALSE(unit_.GetNewestIndex().has_value());
}

TEST_F(ConsumerEventDataControlMixedCoherentLocalViewFixture,
       GetNewSamplesSlotIndicesReturnsZeroWhenNothingPublishedYet)
{
    std::array<SlotIndexType, kMaxSlots> output{};
    EXPECT_EQ(unit_.GetNewSamplesSlotIndices(EventSlotStatus::INVALID_TIMESTAMP, output), 0U);
}

TEST_F(ConsumerEventDataControlMixedCoherentLocalViewFixture,
       GetNewSamplesSlotIndicesReturnsAllNewSamplesInAscendingOrder)
{
    SendEvent(1U);
    SendEvent(2U);
    SendEvent(3U);

    std::array<SlotIndexType, kMaxSlots> output{};
    const auto count = unit_.GetNewSamplesSlotIndices(EventSlotStatus::INVALID_TIMESTAMP, output);

    ASSERT_EQ(count, 3U);
    EXPECT_EQ(output[0], 0U);
    EXPECT_EQ(output[1], 1U);
    EXPECT_EQ(output[2], 2U);
}

TEST_F(ConsumerEventDataControlMixedCoherentLocalViewFixture,
       GetNewSamplesSlotIndicesOnlyReturnsSamplesNewerThanLastSearchTime)
{
    SendEvent(1U);
    SendEvent(2U);
    SendEvent(3U);

    std::array<SlotIndexType, kMaxSlots> output{};
    const auto count = unit_.GetNewSamplesSlotIndices(2U, output);

    ASSERT_EQ(count, 1U);
    EXPECT_EQ(output[0], 2U);
}

TEST_F(ConsumerEventDataControlMixedCoherentLocalViewFixture, GetNewSamplesSlotIndicesRespectsOutputBufferCapacity)
{
    SendEvent(1U);
    SendEvent(2U);
    SendEvent(3U);

    std::array<SlotIndexType, 2U> output{};
    const auto count = unit_.GetNewSamplesSlotIndices(EventSlotStatus::INVALID_TIMESTAMP, output);

    // the two newest samples are returned, in ascending order.
    ASSERT_EQ(count, 2U);
    EXPECT_EQ(output[0], 1U);
    EXPECT_EQ(output[1], 2U);
}

TEST_F(ConsumerEventDataControlMixedCoherentLocalViewFixture, GetNumNewEventsCountsSamplesNewerThanReferenceTime)
{
    SendEvent(1U);
    SendEvent(2U);
    SendEvent(3U);

    EXPECT_EQ(unit_.GetNumNewEvents(EventSlotStatus::INVALID_TIMESTAMP), 3U);
    EXPECT_EQ(unit_.GetNumNewEvents(1U), 2U);
    EXPECT_EQ(unit_.GetNumNewEvents(3U), 0U);
}

TEST_F(ConsumerEventDataControlMixedCoherentLocalViewFixture, OperatorIndexReturnsSlotStatus)
{
    SendEvent(1U);
    EXPECT_EQ(unit_[0U].GetTimeStamp(), 1U);
}

TEST_F(ConsumerEventDataControlMixedCoherentLocalViewFixture, GetMaxSampleSlotsReturnsConfiguredCapacity)
{
    EXPECT_EQ(unit_.GetMaxSampleSlots(), kMaxSlots);
}

TEST_F(ConsumerEventDataControlMixedCoherentLocalViewFixture, HandlesRingBufferWrapAround)
{
    for (SlotIndexType i = 0U; i < kMaxSlots + 2U; ++i)
    {
        SendEvent(static_cast<EventSlotStatus::EventTimeStamp>(i + 1U));
    }

    std::array<SlotIndexType, kMaxSlots> output{};
    const auto count = unit_.GetNewSamplesSlotIndices(EventSlotStatus::INVALID_TIMESTAMP, output);

    // only kMaxSlots samples can be held at once (the ring buffer wrapped around), in ascending publish order.
    ASSERT_EQ(count, kMaxSlots);
    EXPECT_EQ(output[0], 2U);
    EXPECT_EQ(output[1], 3U);
    EXPECT_EQ(output[2], 4U);
    EXPECT_EQ(output[3], 0U);
    EXPECT_EQ(output[4], 1U);
}

}  // namespace
}  // namespace score::mw::com::impl::lola
