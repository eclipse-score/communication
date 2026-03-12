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
#include "score/mw/com/impl/bindings/lola/skeleton_event_data_control_local_view.h"

#include "score/mw/com/impl/bindings/lola/event_slot_status.h"

#include <score/assert.hpp>

#include <atomic>
#include <iostream>
#include <limits>

namespace score::mw::com::impl::lola
{

namespace
{

constexpr auto MAX_ALLOCATE_RETRIES = 100U;

}  // namespace

template <template <class> class AtomicIndirectorType>
SkeletonEventDataControlLocalView<AtomicIndirectorType>::SkeletonEventDataControlLocalView(
    EventDataControl& event_data_control) noexcept
    : state_slots_{event_data_control.state_slots_.begin(), event_data_control.state_slots_.size()},
      transaction_log_set_{event_data_control.transaction_log_set_}
{
}

template <template <class> class AtomicIndirectorType>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from 'selected_index.value()' in case the selected_index doesn't
// have a value but as we check before with 'has_value()' so no way for throwing std::bad_optional_access which leds
// to std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto SkeletonEventDataControlLocalView<AtomicIndirectorType>::AllocateNextSlot() noexcept -> ControlSlotIndicator
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

        const auto& slot = selected_slot.GetSlot();
        const auto slot_value =
            AtomicIndirectorType<EventSlotStatus::value_type>::load(slot, std::memory_order_acquire);
        EventSlotStatus status{slot_value};

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
auto SkeletonEventDataControlLocalView<AtomicIndirectorType>::FindOldestUnusedSlot() noexcept -> ControlSlotIndicator
{
    EventSlotStatus::EventTimeStamp oldest_time_stamp{EventSlotStatus::TIMESTAMP_MAX};

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
        const EventSlotStatus status{
            AtomicIndirectorType<EventSlotStatus::value_type>::load(*it, std::memory_order_acquire)};

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
auto SkeletonEventDataControlLocalView<AtomicIndirectorType>::EventReady(
    ControlSlotIndicator slot_indicator,
    const EventSlotStatus::EventTimeStamp time_stamp) noexcept -> void
{
    const EventSlotStatus initial{time_stamp, 0U};
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(slot_indicator.IsValid());
    slot_indicator.GetSlot().store(static_cast<EventSlotStatus::value_type>(
        initial));  // no race-condition can happen, since event sender has to be single-threaded/non-concurrent per AoU
}

template <template <class> class AtomicIndirectorType>
auto SkeletonEventDataControlLocalView<AtomicIndirectorType>::Discard(ControlSlotIndicator slot_indicator) -> void
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
void SkeletonEventDataControlLocalView<AtomicIndirectorType>::DereferenceEventWithoutTransactionLogging(
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
auto SkeletonEventDataControlLocalView<AtomicIndirectorType>::operator[](const SlotIndexType slot_index) const noexcept
    -> EventSlotStatus
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(static_cast<std::size_t>(slot_index) < state_slots_.size());
    return static_cast<EventSlotStatus>(state_slots_[slot_index].load(std::memory_order_acquire));
}

template <template <class> class AtomicIndirectorType>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, no way for throwing std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto SkeletonEventDataControlLocalView<AtomicIndirectorType>::RemoveAllocationsForWriting() noexcept -> void
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
auto SkeletonEventDataControlLocalView<AtomicIndirectorType>::DumpPerformanceCounters() -> void
{
    std::cout << "EventDataControl performance breakdown\n"
              << "======================================\n"
              << "\nnum_alloc_misses:  " << num_alloc_misses << "\nnum_alloc_retries: "
              << num_alloc_retries
              // Suppress AUTOSAR C++14 M8-4-4, rule finding: "A function identifier shall either be used to call the
              // function or it shall be preceded by &". Passing std::endl to std::cout object with the stream operator
              // follows the idiomatic way that both features in conjunction were designed in the C++ standard.
              // coverity[autosar_cpp14_m8_4_4_violation : FALSE]
              << std::endl;
}

template <template <class> class AtomicIndirectorType>
auto SkeletonEventDataControlLocalView<AtomicIndirectorType>::ResetPerformanceCounters() -> void
{
    num_alloc_misses.store(0U);
    num_alloc_retries.store(0U);
}

template class SkeletonEventDataControlLocalView<memory::shared::AtomicIndirectorReal>;
template class SkeletonEventDataControlLocalView<memory::shared::AtomicIndirectorMock>;

}  // namespace score::mw::com::impl::lola
