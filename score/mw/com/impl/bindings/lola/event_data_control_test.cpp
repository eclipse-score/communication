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
#include "score/mw/com/impl/bindings/lola/event_data_control.h"

#include "score/mw/com/impl/bindings/lola/event_slot_status.h"

#include "score/containers/dynamic_array.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl::lola
{

namespace
{

TEST(EventDataControlTest, EventDataControlUsesADynamicArrayToRepresentSlots)
{
    // Our detailed design (aas/lib/memory/design/shared_memory/OffsetPtrDesign.md#dynamic-array-considerations)
    // requires that we use a DynamicArray to represent our slots so that bounds checking is done.
    static_assert(
        std::is_same_v<score::containers::DynamicArray<
                           std::atomic<EventSlotStatus::value_type>,
                           memory::shared::PolymorphicOffsetPtrAllocator<std::atomic<EventSlotStatus::value_type>>>,
                       EventDataControl::EventControlSlots>,
        "EventDataControl should use a dynamic array to represent slots.");
}

}  // namespace
}  // namespace score::mw::com::impl::lola
