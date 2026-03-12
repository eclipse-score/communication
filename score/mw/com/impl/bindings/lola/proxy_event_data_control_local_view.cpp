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
#include "score/mw/com/impl/bindings/lola/proxy_event_data_control_local_view.h"

#include "score/mw/com/impl/bindings/lola/event_slot_status.h"

#include <score/assert.hpp>

#include <atomic>
#include <iostream>
#include <limits>

namespace score::mw::com::impl::lola
{

namespace
{

constexpr auto MAX_REFERENCE_RETRIES = 100U;

}  // namespace

template <template <class> class AtomicIndirectorType>
ProxyEventDataControlLocalView<AtomicIndirectorType>::ProxyEventDataControlLocalView(
    EventDataControl& event_data_control_shared) noexcept
    : state_slots_{event_data_control_shared.state_slots_.begin(), event_data_control_shared.state_slots_.size()},
      transaction_log_set_{event_data_control_shared.transaction_log_set_}
{
}

template <template <class> class AtomicIndirectorType>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from 'possible_index.value()' in case the possible_index doesn't
// have a value but as we check before with 'has_value()' so no way for throwing std::bad_optional_access which leds
// to std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto ProxyEventDataControlLocalView<AtomicIndirectorType>::ReferenceNextEvent(
    const EventSlotStatus::EventTimeStamp last_search_time,
    const TransactionLogSet::TransactionLogIndex transaction_log_index,
    const EventSlotStatus::EventTimeStamp upper_limit) noexcept -> ControlSlotIndicator
{
    // function can only finish with result, if use count was able to be increased
    ControlSlotIndicator possible_slot{};

    auto& transaction_log = transaction_log_set_.get().GetTransactionLog(transaction_log_index);

    // possible optimization: We can remember a history of possible candidates, then we don't need to fully reiterate
    std::uint64_t counter = 0U;
    for (; counter < MAX_REFERENCE_RETRIES; counter++)
    {
        // resetting possible slot for this iteration
        possible_slot.Reset();

        // initialize candidate_slot_status with last_search_time. candidate_slot_status.timestamp always reflects
        // "highest new timestamp". The sentinel, if we did find a possible candidate is always possible_index.
        EventSlotStatus candidate_slot_status{last_search_time, 0U};

        SlotIndexType current_index = 0U;
        // Suppres "AUTOSAR C++14 A5-3-2" finding rule. This rule states: "Null pointers shall not be dereferenced.".
        // The "slot" variable must never be a null pointer, since DynamicArray allocates its elements when it is
        // created.
        // coverity[autosar_cpp14_a5_3_2_violation]
        for (auto& slot : state_slots_)
        {
            // coverity[autosar_cpp14_a5_3_2_violation]
            const EventSlotStatus slot_status{slot.load(std::memory_order_relaxed)};
            if (slot_status.IsTimeStampBetween(candidate_slot_status.GetTimeStamp(), upper_limit))
            {
                possible_slot = {current_index, slot};
                candidate_slot_status = slot_status;
            }

            // Suppress "AUTOSAR C++14 A4-7-1" rule finding. This rule states: "An integer expression shall
            // not lead to data loss.".
            // On construction of state_slots_, it is already assured, that the number of slots/size can never
            // be larger than a SlotIndexType, so no way an overflow can happen.
            // coverity[autosar_cpp14_a4_7_1_violation : FALSE]
            ++current_index;
        }

        if (!possible_slot.IsValid())
        {
            return {};  // no sample within searched timestamp range exists.
        }

        std::size_t status_new_val{candidate_slot_status};

        // An assertion to make sure that status_new_val will never overflow after assigning candidate_slot_status
        // value.
        static_assert(std::is_same_v<decltype(candidate_slot_status)::value_type, decltype(status_new_val)>,
                      "ReferenceNextEvent: status_new_val overflow dangerous.");
        // As status_new_val increment will take place and in case status_new_val has the maximum limit, an error
        // message logged and terminate to avoid status_new_val overflow.
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
            status_new_val != std::numeric_limits<std::size_t>::max(),
            "EventDataControl::ReferenceNextEvent failed: status_new_val reached the maximum "
            "value, an overflow dangerous");
        // Suppress "AUTOSAR C++14 A4-7-1" rule finding. This rule states: "An integer expression shall
        // not lead to data loss.".
        // No way for an overflow as long as both variables have same data type, and status_new_val doesn't reach the
        // maximum value.
        // coverity[autosar_cpp14_a4_7_1_violation : FALSE]
        ++status_new_val;

        auto& candidate_slot_status_value = static_cast<EventSlotStatus::value_type&>(candidate_slot_status);

        auto possible_index_value = possible_slot.GetIndex();
        auto& slot_value = possible_slot.GetSlot();

        transaction_log.ReferenceTransactionBegin(possible_index_value);
        if (AtomicIndirectorType<EventSlotStatus::value_type>::compare_exchange_weak(
                slot_value, candidate_slot_status_value, status_new_val, std::memory_order_acq_rel))
        {
            transaction_log.ReferenceTransactionCommit(possible_index_value);
            break;
        }
        transaction_log.ReferenceTransactionAbort(possible_index_value);
    }

    num_ref_retries += counter;

    if (counter < MAX_REFERENCE_RETRIES)
    {
        return possible_slot;
    }

    ++num_ref_misses;

    // if this happens it means we have a wrong configuration in the system, see doc-string
    return {};
}

template <template <class> class AtomicIndirectorType>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from 'state_slots_[]' which might leds to a segmentation fault
// in case the index goes outside the range. As we already do an index check before accessing, so no way for
// segmentation fault which leds to calling std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto ProxyEventDataControlLocalView<AtomicIndirectorType>::ReferenceSpecificEvent(
    const SlotIndexType slot_index,
    const TransactionLogSet::TransactionLogIndex transaction_log_index) noexcept -> void
{
    // Sanity check that the slot is currently ready for reading. It's up to the caller to ensure that this function is
    // not called in a context in which the status can change to in writing or invalid while this function is running.
    const auto slot_current_status =
        static_cast<EventSlotStatus>(state_slots_[slot_index].load(std::memory_order_relaxed));
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
        !(slot_current_status.IsInWriting() || slot_current_status.IsInvalid()),
        "An event slot can only be referenced once it's ready for reading.");

    auto& transaction_log = transaction_log_set_.get().GetTransactionLog(transaction_log_index);

    // Since this function must be called when the slot is already ready for reading, we can simply increment the ref
    // count.
    transaction_log.ReferenceTransactionBegin(slot_index);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(static_cast<std::size_t>(slot_index) < state_slots_.size());
    const auto old_slot_value = AtomicIndirectorType<EventSlotStatus::value_type>::fetch_add(
        state_slots_[slot_index], static_cast<EventSlotStatus::value_type>(1U), std::memory_order_acq_rel);

    // If the slot value overflows then the value is completely invalid which is an unrecoverable error. If we try to
    // restart the provider, then it should contain an uncommitted reference transaction which will cause the restart to
    // fail.
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(EventSlotStatus{old_slot_value}.GetReferenceCount() !=
                                                          std::numeric_limits<EventSlotStatus::SubscriberCount>::max(),
                                                      "Reference count overflowed which cannot be recovered from.");
    transaction_log.ReferenceTransactionCommit(slot_index);
}

template <template <class> class AtomicIndirectorType>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, no way for throwing std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
std::size_t ProxyEventDataControlLocalView<AtomicIndirectorType>::GetNumNewEvents(
    const EventSlotStatus::EventTimeStamp reference_time) const noexcept
{
    std::size_t result{0U};
    // Suppres "AUTOSAR C++14 A5-3-2" finding rule. This rule states: "Null pointers shall not be dereferenced.".
    // The "slot" variable must never be a null pointer, since DynamicArray allocates its elements when it is created.
    // coverity[autosar_cpp14_a5_3_2_violation]
    for (const auto& slot : state_slots_)
    {
        // coverity[autosar_cpp14_a5_3_2_violation]
        const EventSlotStatus slot_status{slot.load(std::memory_order_relaxed)};
        if (slot_status.IsTimeStampBetween(reference_time, EventSlotStatus::TIMESTAMP_MAX))
        {
            // Suppress "AUTOSAR C++14 A4-7-1" rule finding. This rule states: "An integer expression shall not lead to
            // loss.". This is a false positive as the maximum number of slots is std::uint16_t, so there is no case for
            // an overflow here.
            // coverity[autosar_cpp14_a4_7_1_violation : FALSE]
            result++;
        }
    }
    return result;
}

template <template <class> class AtomicIndirectorType>
auto ProxyEventDataControlLocalView<AtomicIndirectorType>::DereferenceEvent(
    ControlSlotIndicator slot_indicator,
    const TransactionLogSet::TransactionLogIndex transaction_log_index) noexcept -> void
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(slot_indicator.IsValid());
    auto& transaction_log = transaction_log_set_.get().GetTransactionLog(transaction_log_index);
    transaction_log.DereferenceTransactionBegin(slot_indicator.GetIndex());
    score::cpp::ignore = slot_indicator.GetSlot().fetch_sub(1U, std::memory_order_acq_rel);
    transaction_log.DereferenceTransactionCommit(slot_indicator.GetIndex());
}

template <template <class> class AtomicIndirectorType>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from 'state_slots_[]' which might leds to a segmentation fault
// in case the index goes outside the range. As we already do an index check before accessing, so no way for
// segmentation fault which leds to calling std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
void ProxyEventDataControlLocalView<AtomicIndirectorType>::DereferenceEventWithoutTransactionLogging(
    const SlotIndexType event_slot_index) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(static_cast<std::size_t>(event_slot_index) < state_slots_.size());
    score::cpp::ignore = state_slots_[event_slot_index].fetch_sub(1U, std::memory_order_acq_rel);
}

template <template <class> class AtomicIndirectorType>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from 'state_slots_[]' which might leds to a segmentation fault
// in case the index goes outside the range. As we already do an index check before accessing, so no way for
// segmentation fault which leds to calling std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto ProxyEventDataControlLocalView<AtomicIndirectorType>::operator[](const SlotIndexType slot_index) const noexcept
    -> EventSlotStatus
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(static_cast<std::size_t>(slot_index) < state_slots_.size());
    return static_cast<EventSlotStatus>(state_slots_[slot_index].load(std::memory_order_acquire));
}

template <template <class> class AtomicIndirectorType>
auto ProxyEventDataControlLocalView<AtomicIndirectorType>::DumpPerformanceCounters() -> void
{
    std::cout << "EventDataControl performance breakdown\n"
              << "======================================\n"
              << "\nnum_ref_misses     " << num_ref_misses << "\nnum_ref_retries:   "
              << num_ref_retries
              // Suppress AUTOSAR C++14 M8-4-4, rule finding: "A function identifier shall either be used to call the
              // function or it shall be preceded by &". Passing std::endl to std::cout object with the stream operator
              // follows the idiomatic way that both features in conjunction were designed in the C++ standard.
              // coverity[autosar_cpp14_m8_4_4_violation : FALSE]
              << std::endl;
}

template <template <class> class AtomicIndirectorType>
auto ProxyEventDataControlLocalView<AtomicIndirectorType>::ResetPerformanceCounters() -> void
{
    num_ref_misses.store(0U);
    num_ref_retries.store(0U);
}

template class ProxyEventDataControlLocalView<memory::shared::AtomicIndirectorReal>;
template class ProxyEventDataControlLocalView<memory::shared::AtomicIndirectorMock>;

}  // namespace score::mw::com::impl::lola
