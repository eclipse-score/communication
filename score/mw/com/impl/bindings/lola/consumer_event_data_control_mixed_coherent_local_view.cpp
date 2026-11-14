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

#include <score/assert.hpp>

#include <algorithm>
#include <atomic>

namespace score::mw::com::impl::lola
{

ConsumerEventDataControlMixedCoherentLocalView::ConsumerEventDataControlMixedCoherentLocalView(
    EventDataControlMixed& event_data_control) noexcept
    : state_slots_{event_data_control.data_control_.state_slots_.begin(),
                   event_data_control.data_control_.state_slots_.size()},
      event_data_control_{&event_data_control}
{
}

std::size_t ConsumerEventDataControlMixedCoherentLocalView::GetNewSamplesSlotIndices(
    const EventSlotStatus::EventTimeStamp last_search_time,
    const score::cpp::span<SlotIndexType> output_indices) const noexcept
{
    const auto newest_index = event_data_control_->newest_index_;
    if ((!newest_index.has_value()) || output_indices.empty())
    {
        return 0U;
    }

    const auto number_of_slots = state_slots_.size();
    std::size_t count{0U};
    SlotIndexType current_index = newest_index.value();
    for (std::size_t visited = 0U; visited < number_of_slots; ++visited)
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(static_cast<std::size_t>(current_index) < number_of_slots);
        const auto timestamp = (*this)[current_index].GetTimeStamp();
        if (timestamp <= last_search_time)
        {
            break;
        }

        output_indices[count] = current_index;
        ++count;
        if (count >= output_indices.size())
        {
            break;
        }

        current_index = (current_index == 0U) ? static_cast<SlotIndexType>(number_of_slots - 1U)
                                              : static_cast<SlotIndexType>(current_index - 1U);
    }

    std::reverse(output_indices.begin(), std::next(output_indices.begin(), static_cast<std::ptrdiff_t>(count)));
    return count;
}

std::size_t ConsumerEventDataControlMixedCoherentLocalView::GetNumNewEvents(
    const EventSlotStatus::EventTimeStamp reference_time) const noexcept
{
    const auto newest_index = event_data_control_->newest_index_;
    if (!newest_index.has_value())
    {
        return 0U;
    }

    const auto number_of_slots = state_slots_.size();
    std::size_t count{0U};
    SlotIndexType current_index = newest_index.value();
    for (std::size_t visited = 0U; visited < number_of_slots; ++visited)
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(static_cast<std::size_t>(current_index) < number_of_slots);
        if ((*this)[current_index].GetTimeStamp() <= reference_time)
        {
            break;
        }
        ++count;
        current_index = (current_index == 0U) ? static_cast<SlotIndexType>(number_of_slots - 1U)
                                              : static_cast<SlotIndexType>(current_index - 1U);
    }
    return count;
}

EventSlotStatus ConsumerEventDataControlMixedCoherentLocalView::operator[](
    const SlotIndexType slot_index) const noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(static_cast<std::size_t>(slot_index) < state_slots_.size());
    return EventSlotStatus{state_slots_[slot_index].load(std::memory_order_relaxed)};
}

std::optional<SlotIndexType> ConsumerEventDataControlMixedCoherentLocalView::GetNewestIndex() const noexcept
{
    return event_data_control_->newest_index_;
}

}  // namespace score::mw::com::impl::lola
