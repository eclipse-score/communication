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
#include "score/mw/com/impl/bindings/lola/event_data_control_coherent_composite.h"

#include "score/mw/com/impl/bindings/lola/event_data_control_coherent.h"
#include "score/mw/com/impl/bindings/lola/test_doubles/fake_memory_resource.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl::lola
{
namespace
{

constexpr SlotIndexType kMaxSlots{5U};

class EventDataControlCoherentCompositeFixture : public ::testing::Test
{
  public:
    FakeMemoryResource memory_{};
    EventDataControlCoherent event_data_control_qm_{kMaxSlots, memory_};
    EventDataControlCoherent event_data_control_asil_b_{kMaxSlots, memory_};
    ProviderEventDataControlCoherentLocalView provider_view_qm_{event_data_control_qm_};
    ProviderEventDataControlCoherentLocalView provider_view_asil_b_{event_data_control_asil_b_};
};

using EventDataControlCoherentCompositeQmOnlyFixture = EventDataControlCoherentCompositeFixture;
TEST_F(EventDataControlCoherentCompositeQmOnlyFixture, EventReadyOnlyWritesToQmWhenNoAsilBPresent)
{
    EventDataControlCoherentComposite unit{event_data_control_qm_};

    const auto slot_index = unit.AllocateNextSlot();
    unit.EventReady(slot_index, 1U);

    EXPECT_EQ(provider_view_qm_[slot_index].GetTimeStamp(), 1U);
    EXPECT_TRUE(provider_view_asil_b_[slot_index].IsInvalid());
}

TEST_F(EventDataControlCoherentCompositeFixture, EventReadyWritesToBothQmAndAsilB)
{
    EventDataControlCoherentComposite unit{event_data_control_qm_, &event_data_control_asil_b_};

    const auto slot_index = unit.AllocateNextSlot();
    unit.EventReady(slot_index, 1U);

    EXPECT_EQ(provider_view_qm_[slot_index].GetTimeStamp(), 1U);
    EXPECT_EQ(provider_view_asil_b_[slot_index].GetTimeStamp(), 1U);
}

TEST_F(EventDataControlCoherentCompositeFixture, AllocateNextSlotIsDerivedFromQmView)
{
    EventDataControlCoherentComposite unit{event_data_control_qm_, &event_data_control_asil_b_};

    for (SlotIndexType i = 0U; i < kMaxSlots; ++i)
    {
        const auto slot_index = unit.AllocateNextSlot();
        EXPECT_EQ(slot_index, i);
        unit.EventReady(slot_index, static_cast<CoherentEventSlotStatus::EventTimeStamp>(i + 1U));
    }

    // after kMaxSlots sends, the next slot wraps back around to 0, for both QM and ASIL-B (which are kept in
    // lock-step by always having AllocateNextSlot()/EventReady() applied to both).
    EXPECT_EQ(unit.AllocateNextSlot(), 0U);
}

TEST_F(EventDataControlCoherentCompositeFixture, DiscardIsANoOpOnBothQmAndAsilB)
{
    EventDataControlCoherentComposite unit{event_data_control_qm_, &event_data_control_asil_b_};

    const auto slot_index = unit.AllocateNextSlot();
    unit.Discard(slot_index);

    EXPECT_TRUE(provider_view_qm_[slot_index].IsInvalid());
    EXPECT_TRUE(provider_view_asil_b_[slot_index].IsInvalid());
}

TEST_F(EventDataControlCoherentCompositeFixture, GetLatestTimestampReturnsInvalidTimestampWhenNothingPublishedYet)
{
    EventDataControlCoherentComposite unit{event_data_control_qm_, &event_data_control_asil_b_};

    EXPECT_EQ(unit.GetLatestTimestamp(), CoherentEventSlotStatus::INVALID_TIMESTAMP);
}

TEST_F(EventDataControlCoherentCompositeFixture, GetLatestTimestampReturnsTimestampOfMostRecentlyPublishedSlot)
{
    EventDataControlCoherentComposite unit{event_data_control_qm_, &event_data_control_asil_b_};

    const auto first_slot_index = unit.AllocateNextSlot();
    unit.EventReady(first_slot_index, 1U);
    const auto second_slot_index = unit.AllocateNextSlot();
    unit.EventReady(second_slot_index, 2U);

    EXPECT_EQ(unit.GetLatestTimestamp(), 2U);
}

TEST_F(EventDataControlCoherentCompositeFixture, IsQmControlDisconnectedIsFalseWhenNoAsilBPresent)
{
    EventDataControlCoherentComposite unit{event_data_control_qm_};

    static_cast<void>(unit.AllocateNextSlot());

    EXPECT_FALSE(unit.IsQmControlDisconnected());
}

TEST_F(EventDataControlCoherentCompositeFixture, IsQmControlDisconnectedIsFalseWhenQmAndAsilBAgree)
{
    EventDataControlCoherentComposite unit{event_data_control_qm_, &event_data_control_asil_b_};

    const auto slot_index = unit.AllocateNextSlot();
    unit.EventReady(slot_index, 1U);
    static_cast<void>(unit.AllocateNextSlot());

    EXPECT_FALSE(unit.IsQmControlDisconnected());
}

TEST_F(EventDataControlCoherentCompositeFixture, DivergingQmAndAsilBResultsDisconnectsQmControlAndPrefersAsilB)
{
    EventDataControlCoherentComposite unit{event_data_control_qm_, &event_data_control_asil_b_};

    // Simulate a corrupted/misbehaving QM control section by only advancing the ASIL-B control's newest_index_
    // (bypassing the composite, i.e. not going through unit.AllocateNextSlot()/EventReady()).
    provider_view_asil_b_.EventReady(0U, 5U);

    const auto slot_index = unit.AllocateNextSlot();

    // The QM view still proposes slot 0, while the ASIL-B view (already advanced) proposes slot 1: a divergence,
    // which is resolved in favour of the (trusted) ASIL-B view.
    EXPECT_EQ(slot_index, 1U);
    EXPECT_TRUE(unit.IsQmControlDisconnected());
}

TEST_F(EventDataControlCoherentCompositeFixture, EventReadyDoesNotWriteToQmControlOnceDisconnected)
{
    EventDataControlCoherentComposite unit{event_data_control_qm_, &event_data_control_asil_b_};
    provider_view_asil_b_.EventReady(0U, 5U);
    const auto slot_index = unit.AllocateNextSlot();
    ASSERT_TRUE(unit.IsQmControlDisconnected());

    unit.EventReady(slot_index, 7U);

    EXPECT_TRUE(provider_view_qm_[slot_index].IsInvalid());
    EXPECT_EQ(provider_view_asil_b_[slot_index].GetTimeStamp(), 7U);
}

TEST_F(EventDataControlCoherentCompositeFixture, DiscardDoesNotTouchQmControlOnceDisconnected)
{
    EventDataControlCoherentComposite unit{event_data_control_qm_, &event_data_control_asil_b_};
    provider_view_asil_b_.EventReady(0U, 5U);
    const auto slot_index = unit.AllocateNextSlot();
    ASSERT_TRUE(unit.IsQmControlDisconnected());

    // Discard() is a no-op for both views regardless, but this still exercises the ignore_qm_control_ gated branch.
    unit.Discard(slot_index);

    EXPECT_TRUE(provider_view_qm_[slot_index].IsInvalid());
}

TEST_F(EventDataControlCoherentCompositeFixture, GetLatestTimestampPrefersAsilBOverQm)
{
    EventDataControlCoherentComposite unit{event_data_control_qm_, &event_data_control_asil_b_};

    // Directly diverge QM and ASIL-B (bypassing the composite) to verify GetLatestTimestamp() prefers ASIL-B.
    provider_view_qm_.EventReady(0U, 1U);
    provider_view_asil_b_.EventReady(0U, 2U);

    EXPECT_EQ(unit.GetLatestTimestamp(), 2U);
}

}  // namespace
}  // namespace score::mw::com::impl::lola
