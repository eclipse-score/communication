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
#include "score/mw/com/impl/bindings/lola/event_data_control_test_resources.h"

#include <score/assert.hpp>

namespace score::mw::com::impl::lola
{

EventDataControlCompositeAttorney::EventDataControlCompositeAttorney(
    EventDataControlComposite& event_data_control_composite) noexcept
    : event_data_control_composite_{event_data_control_composite}
{
}

void EventDataControlCompositeAttorney::PrepareAllocateNextSlot(
    const std::pair<score::cpp::optional<SlotIndexType>, bool> expected_result) noexcept
{
    event_data_control_composite_.ignore_qm_control_ = expected_result.second;
    const auto slot_index =
        expected_result.first.has_value() ? expected_result.first.value() : std::numeric_limits<SlotIndexType>::max();
    // we want, that event_data_control_composite_ returns slot_index from call to AllocateNextSlot().
    // To achieve this, we set the reference-count of all slots but slot_index to a value > 0.
    EventSlotStatus::EventTimeStamp timestamp{1};
    const auto number_states_slots = event_data_control_composite_.asil_qm_control_->state_slots_.size();
    for (std::size_t index{0}; index < number_states_slots; index++)
    {
        EventSlotStatus status{};
        status.SetTimeStamp(timestamp++);

        if (expected_result.first.has_value() && index == slot_index)
        {
            status.SetReferenceCount(0U);
        }
        else
        {
            status.SetReferenceCount(1U);
        }
        event_data_control_composite_.asil_qm_control_->state_slots_[index].store(
            static_cast<EventSlotStatus::value_type>(status));
        if (event_data_control_composite_.asil_b_control_ != nullptr)
        {
            event_data_control_composite_.asil_b_control_->state_slots_[index].store(
                static_cast<EventSlotStatus::value_type>(status));
        }
    }
}

void EventDataControlCompositeAttorney::SetQmControlDisconnected(const bool expected_result) noexcept
{
    event_data_control_composite_.ignore_qm_control_ = expected_result;
}

std::pair<EventSlotStatus, score::cpp::optional<EventSlotStatus>> EventDataControlCompositeAttorney::GetSlotStatus(
    const SlotIndexType slot_index) const noexcept
{
    return {
        EventSlotStatus(event_data_control_composite_.asil_qm_control_->state_slots_[slot_index]),
        event_data_control_composite_.asil_b_control_ != nullptr
            ? score::cpp::optional<EventSlotStatus>(event_data_control_composite_.asil_b_control_->state_slots_[slot_index])
            : score::cpp::optional<EventSlotStatus>{}};
}

EventDataControlAttorney::EventDataControlAttorney(EventDataControl& event_data_control) noexcept
    : event_data_control_{event_data_control}
{
}

void EventDataControlAttorney::PrepareAllocateNextSlot(const score::cpp::optional<SlotIndexType> expected_result) noexcept
{
    const auto slot_index =
        expected_result.has_value() ? expected_result.value() : std::numeric_limits<SlotIndexType>::max();
    const auto number_state_slots = event_data_control_.state_slots_.size();
    // we want, that event_data_control_composite_ returns slot_index from call to AllocateNextSlot().
    // To achieve this, we set the reference-count of all slots but slot_index to a value > 0.
    EventSlotStatus::EventTimeStamp timestamp{1};
    for (std::size_t index{0}; index < number_state_slots; index++)
    {
        EventSlotStatus status{};
        status.SetTimeStamp(timestamp++);

        if (expected_result.has_value() && index == slot_index)
        {
            status.SetReferenceCount(0U);
        }
        else
        {
            status.SetReferenceCount(1U);
        }
        event_data_control_.state_slots_[index].store(static_cast<EventSlotStatus::value_type>(status));
    }
}

void EventDataControlAttorney::PrepareReferenceNextEvent(const score::cpp::optional<SlotIndexType> expected_result,
                                                         const EventSlotStatus::EventTimeStamp last_search_time,
                                                         const EventSlotStatus::EventTimeStamp upper_limit) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(upper_limit > last_search_time, "upper_limit needs to be larger than last_search_time");
    const auto number_state_slots = event_data_control_.state_slots_.size();
    // default constructed invalid status slot
    const EventSlotStatus invalid_status{};
    const EventSlotStatus valid_status{(last_search_time + 1U), 0U};
    for (std::size_t index{0}; index < number_state_slots; index++)
    {
        if (expected_result.has_value() && index == expected_result.value())
        {
            event_data_control_.state_slots_[index].store(static_cast<EventSlotStatus::value_type>(valid_status));
        }
        else
        {
            event_data_control_.state_slots_[index].store(static_cast<EventSlotStatus::value_type>(invalid_status));
        }
    }
}

void EventDataControlAttorney::PrepareGetNumNewEvents(const std::size_t expected_result,
                                                      const EventSlotStatus::EventTimeStamp reference_time) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(expected_result <= event_data_control_.state_slots_.size(),
                       "Can't expect to get more new events back, than slots exist!");

    const auto number_state_slots = event_data_control_.state_slots_.size();
    // default constructed invalid status slot
    const EventSlotStatus invalid_status{};
    EventSlotStatus valid_status{(reference_time + 1U), 0U};
    for (std::size_t index{0}; index < number_state_slots; index++)
    {
        if (index < expected_result)
        {
            event_data_control_.state_slots_[index].store(static_cast<EventSlotStatus::value_type>(valid_status));
            valid_status.SetTimeStamp(valid_status.GetTimeStamp() + 1U);
        }
        else
        {
            event_data_control_.state_slots_[index].store(static_cast<EventSlotStatus::value_type>(invalid_status));
        }
    }
}

}  // namespace score::mw::com::impl::lola
