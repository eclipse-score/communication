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
#include "score/mw/com/impl/bindings/lola/event_data_control_mixed.h"

#include "score/mw/com/impl/bindings/lola/event_data_control.h"
#include "score/mw/com/impl/bindings/lola/test_doubles/fake_memory_resource.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl::lola
{
namespace
{

TEST(EventDataControlMixedTest, EmbedsAPlainEventDataControl)
{
    // Our design requires that EventDataControlMixed embeds an EventDataControl by composition, so that non-coherent
    // consumers of a CoherencyType::kMixed event can access it exactly as for a CoherencyType::kNonCoherent event
    // (via ConsumerEventDataControlLocalView, constructed directly from the embedded data_control_).
    static_assert(std::is_same_v<EventDataControl, decltype(EventDataControlMixed::data_control_)>,
                  "EventDataControlMixed should embed a plain EventDataControl.");
}

TEST(EventDataControlMixedTest, ConstructionAllocatesGivenNumberOfSlotsAndNewestIndexIsEmpty)
{
    FakeMemoryResource memory{};
    constexpr SlotIndexType kMaxSlots{5U};

    const EventDataControlMixed unit{kMaxSlots, memory};

    EXPECT_EQ(unit.data_control_.state_slots_.size(), kMaxSlots);
    EXPECT_FALSE(unit.newest_index_.has_value());
}

}  // namespace
}  // namespace score::mw::com::impl::lola
