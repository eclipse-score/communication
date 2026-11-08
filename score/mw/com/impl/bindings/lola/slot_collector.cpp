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
#include "score/mw/com/impl/bindings/lola/slot_collector.h"

#include <score/assert.hpp>
#include <score/span.hpp>

#include <algorithm>
#include <iterator>
#include <optional>

namespace score::mw::com::impl::lola
{

SlotCollector::SlotCollector(ConsumerEventDataControlLocalView<>& event_data_control_local,
                             const std::size_t max_slots) noexcept
    : event_data_control_local_{event_data_control_local}, last_ts_{0U}, collected_slots_(max_slots)
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(max_slots > 0U, "Pre-allocated slot vector must not be empty!");
}

SlotCollector::SlotCollector(ConsumerEventDataControlCoherentLocalView& event_data_control_local,
                             const std::size_t max_slots) noexcept
    : event_data_control_local_{event_data_control_local}, last_ts_{0U}, collected_slots_(max_slots)
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(max_slots > 0U, "Pre-allocated slot vector must not be empty!");
}

SlotCollector::SlotCollector(ConsumerEventDataControlMixedCoherentLocalView& event_data_control_local,
                             const std::size_t max_slots) noexcept
    : event_data_control_local_{event_data_control_local}, last_ts_{0U}, collected_slots_(max_slots)
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(max_slots > 0U, "Pre-allocated slot vector must not be empty!");
}

std::size_t SlotCollector::GetNumNewSamplesAvailable() const noexcept
{
    return std::visit(
        [this](const auto& event_data_control_local) noexcept -> std::size_t {
            return event_data_control_local.get().GetNumNewEvents(last_ts_);
        },
        event_data_control_local_);
}

SlotCollector::SlotIndices SlotCollector::GetNewSamplesSlotIndices(const std::size_t max_count) noexcept
{
    // CollectSlots() return an iterator pointing to one past the end (end being the oldest collected slot, begin
    // being the newest/youngest collected slot). Which CollectSlots() overload is invoked is determined by ordinary
    // C++ overload resolution, based on the concrete local-view type held by event_data_control_local_ for this
    // particular SlotCollector instance (the generic lambda below is instantiated once per std::variant
    // alternative).
    const auto collected_slots_end_const_iterator = std::visit(
        [this, max_count](auto& event_data_control_local) noexcept -> SlotIndexVector::const_iterator {
            return CollectSlots(event_data_control_local.get(), max_count);
        },
        event_data_control_local_);

    EventSlotStatus::EventTimeStamp newest_delivered{last_ts_};
    // we are iterating the collected slots from the oldest event (smallest timestamp) to the newest event
    // (largest timestamp) here.
    for (auto slot = std::make_reverse_iterator(collected_slots_end_const_iterator); slot != collected_slots_.crend();
         ++slot)
    {
        const auto timestamp = std::visit(
            [slot](const auto& event_data_control_local) noexcept -> EventSlotStatus::EventTimeStamp {
                return event_data_control_local.get()[*slot].GetTimeStamp();
            },
            event_data_control_local_);
        newest_delivered = std::max(newest_delivered, timestamp);
    }

    last_ts_ = newest_delivered;

    return {std::make_reverse_iterator(collected_slots_end_const_iterator), collected_slots_.crend()};
}

SlotCollector::SlotIndexVector::const_iterator SlotCollector::CollectSlots(
    ConsumerEventDataControlLocalView<>& event_data_control_local,
    const std::size_t max_count) noexcept
{
    EventSlotStatus::EventTimeStamp current_highest = EventSlotStatus::TIMESTAMP_MAX;
    auto collected_slot = collected_slots_.begin();

    // Defensive programming: We check in the constructor that collected_slots_ must not be empty
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(collected_slot != collected_slots_.cend());
    for (std::size_t count = 0U; count < max_count; ++count)
    {
        const std::optional<SlotIndexType> slot =
            event_data_control_local.ReferenceNextEvent(last_ts_, current_highest);
        if (!(slot.has_value()))
        {
            break;
        }
        const EventSlotStatus event_status{event_data_control_local[*slot]};
        current_highest = event_status.GetTimeStamp();
        *collected_slot = *slot;
        ++collected_slot;

        if (collected_slot == collected_slots_.end())
        {
            break;
        }
    }

    return collected_slot;
}

SlotCollector::SlotIndexVector::const_iterator SlotCollector::CollectSlots(
    ConsumerEventDataControlCoherentLocalView& event_data_control_local,
    const std::size_t max_count) noexcept
{
    return CollectSlotsCoherentLike(event_data_control_local, max_count);
}

SlotCollector::SlotIndexVector::const_iterator SlotCollector::CollectSlots(
    ConsumerEventDataControlMixedCoherentLocalView& event_data_control_local,
    const std::size_t max_count) noexcept
{
    return CollectSlotsCoherentLike(event_data_control_local, max_count);
}

template <typename CoherentLikeLocalView>
SlotCollector::SlotIndexVector::const_iterator SlotCollector::CollectSlotsCoherentLike(
    CoherentLikeLocalView& event_data_control_local,
    const std::size_t max_count) noexcept
{
    // Defensive programming: We check in the constructor that collected_slots_ must not be empty
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(!collected_slots_.empty());
    const auto num_slots = std::min(max_count, collected_slots_.size());
    const score::cpp::span<SlotIndexType> output_indices{collected_slots_.data(), num_slots};
    // GetNewSamplesSlotIndices() fills output_indices in ascending (oldest-to-newest) order, whereas collected_slots_
    // is expected (by the shared code in GetNewSamplesSlotIndices() above) to hold the newest slot first (descending
    // order), so we reverse the written range below to match CollectSlots()'s convention.
    const auto num_collected = event_data_control_local.GetNewSamplesSlotIndices(last_ts_, output_indices);
    const auto collected_slots_end = std::next(collected_slots_.begin(), static_cast<std::ptrdiff_t>(num_collected));
    std::reverse(collected_slots_.begin(), collected_slots_end);

    return collected_slots_end;
}

// Explicit instantiations for the two coherent-flavoured local view types used by GetNewSamplesSlotIndices() above.
template SlotCollector::SlotIndexVector::const_iterator SlotCollector::CollectSlotsCoherentLike(
    ConsumerEventDataControlCoherentLocalView&,
    const std::size_t) noexcept;
template SlotCollector::SlotIndexVector::const_iterator SlotCollector::CollectSlotsCoherentLike(
    ConsumerEventDataControlMixedCoherentLocalView&,
    const std::size_t) noexcept;

}  // namespace score::mw::com::impl::lola
