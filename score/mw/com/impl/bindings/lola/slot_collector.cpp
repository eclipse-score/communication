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

#include <iterator>

namespace score::mw::com::impl::lola
{

SlotCollector::SlotCollector(EventDataControl& event_data_control,
                             const std::size_t max_slots,
                             TransactionLogSet::TransactionLogIndex transaction_log_index) noexcept
    : event_data_control_{event_data_control},
      last_ts_{0U},
      collected_slots_(max_slots),
      transaction_log_index_{transaction_log_index}
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(max_slots > 0U, "Pre-allocated slot vector must not be empty!");
}

std::size_t SlotCollector::GetNumNewSamplesAvailable() const noexcept
{
    return event_data_control_.get().GetNumNewEvents(last_ts_);
}

SlotCollector::SlotIndicators SlotCollector::GetNewSamplesSlotIndices(const std::size_t max_count) noexcept
{
    // CollectSlots() returns an iterator pointing to one past the end (end being the oldest collected slot, begin being
    // the newest/youngest collected slot)
    auto collected_slots_end_iterator = CollectSlots(max_count);

    EventSlotStatus::EventTimeStamp newest_delivered{last_ts_};
    // we are iterating the collected slots from the oldest event (smallest timestamp) to the newest event
    // (largest timestamp) here.
    for (auto slot = std::make_reverse_iterator(collected_slots_end_iterator); slot != collected_slots_.crend(); ++slot)
    {
        const EventSlotStatus slot_status{slot->GetSlot().load()};
        newest_delivered = std::max(newest_delivered, slot_status.GetTimeStamp());
    }

    last_ts_ = newest_delivered;

    return {std::make_reverse_iterator(collected_slots_end_iterator), collected_slots_.rend()};
}

SlotCollector::SlotIndicatorVector::iterator SlotCollector::CollectSlots(const std::size_t max_count) noexcept
{
    EventSlotStatus::EventTimeStamp current_highest = EventSlotStatus::TIMESTSCORE_LANGUAGE_FUTURECPP_MAX;
    auto collected_slot = collected_slots_.begin();

    // Defensive programming: We check in the constructor that collected_slots_ must not be empty
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(collected_slot != collected_slots_.cend());
    for (std::size_t count = 0U; count < max_count; ++count)
    {
        ControlSlotIndicator slot =
            event_data_control_.get().ReferenceNextEvent(last_ts_, transaction_log_index_, current_highest);
        if (!(slot.IsValid()))
        {
            break;
        }
        const EventSlotStatus event_status{slot.GetSlot().load()};
        current_highest = event_status.GetTimeStamp();
        *collected_slot = slot;
        ++collected_slot;

        if (collected_slot == collected_slots_.end())
        {
            break;
        }
    }

    return collected_slot;
}

}  // namespace score::mw::com::impl::lola
