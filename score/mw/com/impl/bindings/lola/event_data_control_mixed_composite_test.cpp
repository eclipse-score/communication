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
#include "score/mw/com/impl/bindings/lola/event_data_control_mixed_composite.h"

#include "score/mw/com/impl/bindings/lola/event_data_control_mixed.h"
#include "score/mw/com/impl/bindings/lola/test_doubles/fake_memory_resource.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl::lola
{
namespace
{

constexpr SlotIndexType kMaxSlots{5U};

class EventDataControlMixedCompositeFixture : public ::testing::Test
{
  public:
    FakeMemoryResource memory_{};
    EventDataControlMixed event_data_control_mixed_qm_{kMaxSlots, memory_};
    EventDataControlMixed event_data_control_mixed_asil_b_{kMaxSlots, memory_};
};

using EventDataControlMixedCompositeQmOnlyFixture = EventDataControlMixedCompositeFixture;
TEST_F(EventDataControlMixedCompositeQmOnlyFixture, EventReadyUpdatesNewestIndexOnQmOnly)
{
    EventDataControlMixedComposite unit{event_data_control_mixed_qm_};

    const auto allocation_result = unit.GetComposite().AllocateNextSlot();
    ASSERT_TRUE(allocation_result.allocated_slot_index.has_value());
    const auto slot_index = allocation_result.allocated_slot_index.value();

    unit.EventReady(slot_index, 42U);

    EXPECT_EQ(event_data_control_mixed_qm_.newest_index_, slot_index);
    EXPECT_EQ(unit.GetComposite().GetLatestTimestamp(), 42U);
}

using EventDataControlMixedCompositeQmAndAsilBFixture = EventDataControlMixedCompositeFixture;
TEST_F(EventDataControlMixedCompositeQmAndAsilBFixture, EventReadyUpdatesNewestIndexOnQmAndAsilB)
{
    EventDataControlMixedComposite unit{event_data_control_mixed_qm_, &event_data_control_mixed_asil_b_};

    const auto allocation_result = unit.GetComposite().AllocateNextSlot();
    ASSERT_TRUE(allocation_result.allocated_slot_index.has_value());
    const auto slot_index = allocation_result.allocated_slot_index.value();

    unit.EventReady(slot_index, 42U);

    EXPECT_EQ(event_data_control_mixed_qm_.newest_index_, slot_index);
    EXPECT_EQ(event_data_control_mixed_asil_b_.newest_index_, slot_index);
}

}  // namespace
}  // namespace score::mw::com::impl::lola
