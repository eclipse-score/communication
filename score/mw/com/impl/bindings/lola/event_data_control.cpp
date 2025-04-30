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

#include <score/assert.hpp>

#include <atomic>
#include <iostream>
#include <limits>

namespace score::mw::com::impl::lola::detail_event_data_control
{

namespace
{

constexpr auto MAX_ALLOCATE_RETRIES = 100U;
constexpr auto MAX_REFERENCE_RETRIES = 100U;

}  // namespace

template <template <class> class AtomicIndirectorType>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". we can't mark the constructor of 'score::containers::DynamicArray' as noexcept because this will generate
// coverity findings in all users. The only way to throw exception by the constructor is that we run out of memory, and
// as we assume that the user has memory so no way to throw std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation]
EventDataControlImpl<AtomicIndirectorType>::EventDataControlImpl(
    const SlotIndexType max_slots,
    const score::memory::shared::MemoryResourceProxy* const proxy,
    const LolaEventInstanceDeployment::SubscriberCountType max_number_combined_subscribers) noexcept
    : state_slots_{max_slots, proxy}, transaction_log_set_{max_number_combined_subscribers, max_slots, proxy}
{
}

template <template <class> class AtomicIndirectorType>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from 'selected_index.value()' in case the selected_index doesn't
// have a value but as we check before with 'has_value()' so no way for throwing std::bad_optional_access which leds
// to std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto EventDataControlImpl<AtomicIndirectorType>::AllocateNextSlot() noexcept -> ControlSlotIndicator
{
    // initially we have a default constructed "invalid" control-slot-indicator
    ControlSlotIndicator selected_slot{};
    std::uint64_t retry_counter{0U};

    for (; retry_counter <= MAX_ALLOCATE_RETRIES; ++retry_counter)
    {
        selected_slot = FindOldestUnusedSlot();

        if (!selected_slot.IsValid())
        {
            continue;
        }

        EventSlotStatus status{selected_slot.GetSlot().load(std::memory_order_acquire)};

        // we need to check that this is still the same, since it is possible that is has changed after we found it
        // earlier
        if ((status.GetReferenceCount() != static_cast<EventSlotStatus::SubscriberCount>(0)) || status.IsInWriting())
        {
            continue;
        }

        EventSlotStatus status_new{};  // This will set the refcount to 0 by default
        status_new.MarkInWriting();

        auto status_value_type = static_cast<EventSlotStatus::value_type&>(status);
        auto status_new_value_type = static_cast<EventSlotStatus::value_type&>(status_new);
        if (selected_slot.GetSlot().compare_exchange_weak(
                status_value_type, status_new_value_type, std::memory_order_acq_rel))
        {
            break;
        }
    }

    score::cpp::ignore = num_alloc_retries.fetch_add(retry_counter);

    if ((retry_counter >= MAX_ALLOCATE_RETRIES) && (!selected_slot.IsValid()))
    {
        ++num_alloc_misses;
    }

    return selected_slot;
}

template <template <class> class AtomicIndirectorType>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, no way for throwing std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto EventDataControlImpl<AtomicIndirectorType>::FindOldestUnusedSlot() noexcept -> ControlSlotIndicator
{
    EventSlotStatus::EventTimeStamp oldest_time_stamp{EventSlotStatus::TIMESTSCORE_LANGUAGE_FUTURECPP_MAX};

    ControlSlotIndicator selected_slot{};
    // Suppress "AUTOSAR C++14 A5-2-2" finding rule. This rule states: "Traditional C-style casts shall not be used".
    // There is no C-style cast happening here! "current_index" and "it" are automatically deduced.
    // Suppress "AUTOSAR C++14 M6-5-5" finding rule. This rule states: "A loop-control-variable other than the
    // loop-counter shall not be modified within condition or expression".
    // Both variables (current_index, it) depict exactly the same element once via index addressing and once via
    // iterator/raw-pointer! The func has to return both, and we want both to be fully in sync, therefore we increment
    // them both symmetrically, since in this case, it is less error-prone!
    // Suppress "AUTOSAR C++14 A4-7-1" finding rule. This rule states: "An integer expression shall not lead to data
    // loss". This can not be the case as the type of current_index is correctly deduced from SlotIndexType and it is
    // assured, that state_slots_ doesn't contain more elements than SlotIndexType can represent.
    // Suppress "AUTOSAR C++14 M5-0-15". This rule states:"indexing shall be the only form of pointer arithmetic.".
    // Rationale: Tolerated due to containers providing pointer-like iterators.
    // The Coverity tool considers these iterators as raw pointers.
    // Suppress "AUTOSAR C++14 M5-2-10". This rule states: "The increment (++) and decrement (--) operators shall not be
    // mixed with other operators in an expression".
    // Rationale: We are only using increment operators here on separate variables! That we use it on separate variables
    // is explained above for M6-5-5.
    // coverity[autosar_cpp14_a5_2_2_violation]
    for (auto [current_index, it] = std::make_tuple(SlotIndexType{0U}, state_slots_.begin()); it != state_slots_.end();
         // coverity[autosar_cpp14_m6_5_5_violation]
         // coverity[autosar_cpp14_a4_7_1_violation]
         // coverity[autosar_cpp14_m5_0_15_violation]
         // coverity[autosar_cpp14_m5_2_10_violation]
         ++it,
                              // coverity[autosar_cpp14_a4_7_1_violation]
                              // coverity[autosar_cpp14_m6_5_5_violation]
         ++current_index)
    {
        // coverity[autosar_cpp14_a5_3_2_violation]
        const EventSlotStatus status{it->load(std::memory_order_acquire)};

        if (status.IsInvalid())
        {
            selected_slot = {current_index, *it};
            break;
        }

        const auto are_proxies_referencing_slot =
            status.GetReferenceCount() != static_cast<EventSlotStatus::SubscriberCount>(0U);
        if (!are_proxies_referencing_slot && !status.IsInWriting())
        {
            if (status.GetTimeStamp() < oldest_time_stamp)
            {
                oldest_time_stamp = status.GetTimeStamp();
                selected_slot = {current_index, *it};
            }
        }
    }
    return selected_slot;
}

template <template <class> class AtomicIndirectorType>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from 'state_slots_[]' which might leds to a segmentation fault
// in case the index goes outside the range. As we already do an index check before accessing, so no way for
// segmentation fault which leds to calling std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto EventDataControlImpl<AtomicIndirectorType>::EventReady(ControlSlotIndicator slot_indicator,
                                                            const EventSlotStatus::EventTimeStamp time_stamp) noexcept
    -> void
{
    const EventSlotStatus initial{time_stamp, 0U};
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(slot_indicator.IsValid());
    slot_indicator.GetSlot().store(static_cast<EventSlotStatus::value_type>(
        initial));  // no race-condition can happen, since event sender has to be single-threaded/non-concurrent per AoU
}

template <template <class> class AtomicIndirectorType>
auto EventDataControlImpl<AtomicIndirectorType>::Discard(ControlSlotIndicator slot_indicator) -> void
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(slot_indicator.IsValid());
    auto slot = static_cast<EventSlotStatus>(slot_indicator.GetSlot().load(std::memory_order_acquire));
    if (slot.IsInWriting())
    {
        slot.MarkInvalid();
        slot_indicator.GetSlot().store(static_cast<EventSlotStatus::value_type>(slot), std::memory_order_release);
    }
}

template <template <class> class AtomicIndirectorType>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from 'state_slots_[]' which might leds to a segmentation fault
// in case the index goes outside the range. As we already do an index check before accessing, so no way for
// segmentation fault which leds to calling std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto EventDataControlImpl<AtomicIndirectorType>::ReferenceSpecificEvent(
    const SlotIndexType slot_index,
    const TransactionLogSet::TransactionLogIndex transaction_log_index) noexcept -> void
{
    // Sanity check that the slot is currently ready for reading. It's up to the caller to ensure that this function is
    // not called in a context in which the status can change to in writing or invalid while this function is running.
    const auto slot_current_status =
        static_cast<EventSlotStatus>(state_slots_[slot_index].load(std::memory_order_relaxed));
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(!(slot_current_status.IsInWriting() || slot_current_status.IsInvalid()),
                                 "An event slot can only be referenced once it's ready for reading.");

    auto& transaction_log = transaction_log_set_.GetTransactionLog(transaction_log_index);

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
// implicitly". std::terminate() is implicitly called from 'possible_index.value()' in case the possible_index doesn't
// have a value but as we check before with 'has_value()' so no way for throwing std::bad_optional_access which leds
// to std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto EventDataControlImpl<AtomicIndirectorType>::ReferenceNextEvent(
    const EventSlotStatus::EventTimeStamp last_search_time,
    const TransactionLogSet::TransactionLogIndex transaction_log_index,
    const EventSlotStatus::EventTimeStamp upper_limit) noexcept -> ControlSlotIndicator
{
    // function can only finish with result, if use count was able to be increased
    ControlSlotIndicator possible_slot{};

    auto& transaction_log = transaction_log_set_.GetTransactionLog(transaction_log_index);

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
            // As we are looping on the state slots, and current_index is incremented after handling each slot
            // so no way for an overflow as long as both variables have same data type.
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
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(status_new_val != std::numeric_limits<std::size_t>::max(),
                               "EventDataControlImpl::ReferenceNextEvent failed: status_new_val reached the maximum "
                               "value, an overflow dangerous");
        // Suppress "AUTOSAR C++14 A4-7-1" rule finding. This rule states: "An integer expression shall
        // not lead to data loss.".
        // No way for an overflow as long as both variables have same data type, and status_new_val doesn't reach the
        // maximum value.
        // coverity[autosar_cpp14_a4_7_1_violation : FALSE]
        ++status_new_val;

        auto candidate_slot_status_value = static_cast<EventSlotStatus::value_type&>(candidate_slot_status);

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
// implicitly". This is a false positive, no way for throwing std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
std::size_t EventDataControlImpl<AtomicIndirectorType>::GetNumNewEvents(
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
        if (slot_status.IsTimeStampBetween(reference_time, EventSlotStatus::TIMESTSCORE_LANGUAGE_FUTURECPP_MAX))
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
auto EventDataControlImpl<AtomicIndirectorType>::DereferenceEvent(
    ControlSlotIndicator slot_indicator,
    const TransactionLogSet::TransactionLogIndex transaction_log_index) noexcept -> void
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(slot_indicator.IsValid());
    auto& transaction_log = transaction_log_set_.GetTransactionLog(transaction_log_index);
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
void EventDataControlImpl<AtomicIndirectorType>::DereferenceEventWithoutTransactionLogging(
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
auto EventDataControlImpl<AtomicIndirectorType>::operator[](const SlotIndexType slot_index) const noexcept
    -> EventSlotStatus
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(static_cast<std::size_t>(slot_index) < state_slots_.size());
    return static_cast<EventSlotStatus>(state_slots_[slot_index].load(std::memory_order_acquire));
}

template <template <class> class AtomicIndirectorType>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, no way for throwing std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto EventDataControlImpl<AtomicIndirectorType>::RemoveAllocationsForWriting() noexcept -> void
{
    // Suppres "AUTOSAR C++14 A5-3-2" finding rule. This rule states: "Null pointers shall not be dereferenced.".
    // The "slot" variable must never be a null pointer, since DynamicArray allocates its elements when it is created.
    // coverity[autosar_cpp14_a5_3_2_violation]
    for (auto& slot : state_slots_)
    {
        // coverity[autosar_cpp14_a5_3_2_violation]
        EventSlotStatus status{
            AtomicIndirectorType<EventSlotStatus::value_type>::load(slot, std::memory_order_acquire)};

        if (status.IsInWriting())
        {
            EventSlotStatus status_new{};

            auto status_value_type = static_cast<EventSlotStatus::value_type&>(status);
            auto status_new_value_type = static_cast<EventSlotStatus::value_type&>(status_new);
            // coverity[autosar_cpp14_a5_3_2_violation]
            if (slot.compare_exchange_strong(status_value_type, status_new_value_type, std::memory_order_acq_rel) ==
                false)
            {
                // atomic could not be changed, contract violation (other skeleton must be dead, nobody other should
                // change the slot)
                std::terminate();
            }
        }
    }
}

template <template <class> class AtomicIndirectorType>
auto EventDataControlImpl<AtomicIndirectorType>::DumpPerformanceCounters() -> void
{
    std::cout << "EventDataControlImpl performance breakdown\n"
              << "======================================\n"
              << "\nnum_alloc_misses:  " << num_alloc_misses << "\nnum_ref_misses     " << num_ref_misses
              << "\nnum_alloc_retries: " << num_alloc_retries << "\nnum_ref_retries:   "
              << num_ref_retries
              // Suppress AUTOSAR C++14 M8-4-4, rule finding: "A function identifier shall either be used to call the
              // function or it shall be preceded by &". Passing std::endl to std::cout object with the stream operator
              // follows the idiomatic way that both features in conjunction were designed in the C++ standard.
              // coverity[autosar_cpp14_m8_4_4_violation : FALSE]
              << std::endl;
}

template <template <class> class AtomicIndirectorType>
auto EventDataControlImpl<AtomicIndirectorType>::ResetPerformanceCounters() -> void
{
    num_alloc_misses.store(0U);
    num_ref_misses.store(0U);
    num_alloc_retries.store(0U);
    num_ref_retries.store(0U);
}

template class EventDataControlImpl<memory::shared::AtomicIndirectorReal>;
template class EventDataControlImpl<memory::shared::AtomicIndirectorMock>;

}  // namespace score::mw::com::impl::lola::detail_event_data_control
