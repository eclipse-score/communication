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

#include <score/assert.hpp>

namespace score::mw::com::impl::lola
{

ProviderEventDataControlCoherentLocalView::ProviderEventDataControlCoherentLocalView(
    EventDataControlCoherent& event_data_control) noexcept
    : state_slots_{event_data_control.state_slots_.begin(), event_data_control.state_slots_.size()},
      event_data_control_{&event_data_control}
{
}

SlotIndexType ProviderEventDataControlCoherentLocalView::AllocateNextSlot() const noexcept
{
    const auto& newest_index = event_data_control_->newest_index_;
    if (!newest_index.has_value())
    {
        return 0U;
    }
    const auto number_of_slots = static_cast<SlotIndexType>(state_slots_.size());
    return static_cast<SlotIndexType>((newest_index.value() + 1U) % number_of_slots);
}

void ProviderEventDataControlCoherentLocalView::EventReady(
    const SlotIndexType slot_index,
    const CoherentEventSlotStatus::EventTimeStamp time_stamp) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(static_cast<std::size_t>(slot_index) < state_slots_.size());
    state_slots_[slot_index].SetTimeStamp(time_stamp);
    event_data_control_->newest_index_ = slot_index;
}

void ProviderEventDataControlCoherentLocalView::Discard(const SlotIndexType slot_index) const noexcept
{
    // Intentionally a no-op, see class-level documentation.
    static_cast<void>(slot_index);
}

CoherentEventSlotStatus ProviderEventDataControlCoherentLocalView::operator[](
    const SlotIndexType slot_index) const noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(static_cast<std::size_t>(slot_index) < state_slots_.size());
    return state_slots_[slot_index];
}

std::optional<SlotIndexType> ProviderEventDataControlCoherentLocalView::GetNewestIndex() const noexcept
{
    return event_data_control_->newest_index_;
}

}  // namespace score::mw::com::impl::lola
