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

#include "score/mw/com/impl/bindings/lola/control_slot_types.h"
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
auto SkeletonEventDataControlLocalView<AtomicIndirectorType>::AllocateNextSlot() noexcept
    -> std::optional<SlotIndexType>
{
    std::optional<SlotIndexType> selected_index{};
    std::uint64_t retry_counter{0U};

    for (; retry_counter <= MAX_ALLOCATE_RETRIES; ++retry_counter)
    {
        selected_index = FindOldestUnusedSlot();

        if (!selected_index.has_value())
        {
            continue;
        }

        const auto slot_value = AtomicIndirectorType<EventSlotStatus::value_type>::load(
            state_slots_[selected_index.value()], std::memory_order_acquire);
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
        if (state_slots_[selected_index.value()].compare_exchange_weak(
                status_value_type, status_new_value_type, std::memory_order_acq_rel))
        {
            break;
        }
    }

    score::cpp::ignore = num_alloc_retries.fetch_add(retry_counter);

    if ((retry_counter >= MAX_ALLOCATE_RETRIES))
    {
        ++num_alloc_misses;
    }

    return selected_index;
}

template <template <class> class AtomicIndirectorType>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, no way for throwing std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto SkeletonEventDataControlLocalView<AtomicIndirectorType>::FindOldestUnusedSlot() const noexcept
    -> std::optional<SlotIndexType>
{
    EventSlotStatus::EventTimeStamp oldest_time_stamp{EventSlotStatus::TIMESTAMP_MAX};
    std::size_t current_index = 0U;
    std::optional<SlotIndexType> selected_index{};
    for (const auto& slot : state_slots_)
    {
        // coverity[autosar_cpp14_a5_3_2_violation]
        const EventSlotStatus status{
            AtomicIndirectorType<EventSlotStatus::value_type>::load(slot, std::memory_order_acquire)};

        if (status.IsInvalid())
        {
            selected_index = current_index;
            break;
        }

        const auto are_proxies_referencing_slot =
            status.GetReferenceCount() != static_cast<EventSlotStatus::SubscriberCount>(0U);
        if (!are_proxies_referencing_slot && !status.IsInWriting())
        {
            if (status.GetTimeStamp() < oldest_time_stamp)
            {
                oldest_time_stamp = status.GetTimeStamp();
                selected_index = current_index;
            }
        }
        static_assert(std::is_same_v<decltype(state_slots_.size()), decltype(current_index)>,
                      "FindOldestUnusedSlot: Overflow dangerous.");
        // Suppress "AUTOSAR C++14 A4-7-1" rule finding. This rule states: "An integer expression shall
        // not lead to data loss.".
        // As we are looping on the state slots, and current_index is incremented after handling each slot
        // so no way for an overflow as long as both variables have same data type.
        // coverity[autosar_cpp14_a4_7_1_violation : FALSE]
        ++current_index;
    }
    return selected_index;
}

template <template <class> class AtomicIndirectorType>
auto SkeletonEventDataControlLocalView<AtomicIndirectorType>::EventReady(
    const SlotIndexType slot_index,
    const EventSlotStatus::EventTimeStamp time_stamp) noexcept -> void
{
    const EventSlotStatus initial{time_stamp, 0U};
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(static_cast<std::size_t>(slot_index) < state_slots_.size());
    state_slots_[slot_index].store(static_cast<EventSlotStatus::value_type>(
        initial));  // no race-condition can happen, since event sender has to be single-threaded/non-concurrent per AoU
}

template <template <class> class AtomicIndirectorType>
auto SkeletonEventDataControlLocalView<AtomicIndirectorType>::Discard(const SlotIndexType slot_index) -> void
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(static_cast<std::size_t>(slot_index) < state_slots_.size());
    auto slot = static_cast<EventSlotStatus>(state_slots_[slot_index].load(std::memory_order_acquire));
    if (slot.IsInWriting())
    {
        slot.MarkInvalid();
        state_slots_[slot_index].store(static_cast<EventSlotStatus::value_type>(slot), std::memory_order_release);
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
