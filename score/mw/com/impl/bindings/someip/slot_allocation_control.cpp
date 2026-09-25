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

namespace score::mw::com::impl::someip
{

void SlotAllocationControl::Reset(const std::size_t number_of_slots)
{
    slot_in_use_.assign(number_of_slots, false);
}

void SlotAllocationControl::Clear() noexcept
{
    slot_in_use_.clear();
}

std::optional<SlotIndexType> SlotAllocationControl::AllocateSlot() noexcept
{
    for (std::size_t slot_index = 0U; slot_index < slot_in_use_.size(); ++slot_index)
    {
        if (!slot_in_use_[slot_index])
        {
            slot_in_use_[slot_index] = true;
            return static_cast<SlotIndexType>(slot_index);
        }
    }
    return std::nullopt;
}

void SlotAllocationControl::DiscardSlot(const SlotIndexType slot_index) noexcept
{
    if (static_cast<std::size_t>(slot_index) >= slot_in_use_.size())
    {
        return;
    }
    slot_in_use_[slot_index] = false;
}

std::size_t SlotAllocationControl::GetNumberOfSlots() const noexcept
{
    return slot_in_use_.size();
}

}  // namespace score::mw::com::impl::someip
