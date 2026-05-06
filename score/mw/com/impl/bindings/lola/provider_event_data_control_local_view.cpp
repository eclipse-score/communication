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
#include "score/mw/com/impl/bindings/lola/provider_event_data_control_local_view.h"

#include "score/mw/com/impl/bindings/lola/control_slot_types.h"
#include "score/mw/com/impl/bindings/lola/event_slot_status.h"

#include <score/assert.hpp>

#include <atomic>
#include <iostream>

namespace score::mw::com::impl::lola
{

namespace
{

constexpr auto MAX_ALLOCATE_RETRIES = 100U;

}  // namespace

template <template <class> class AtomicIndirectorType>
ProviderEventDataControlLocalView<AtomicIndirectorType>::ProviderEventDataControlLocalView(
    EventDataControl& event_data_control) noexcept
    : state_slots_{event_data_control.state_slots_.begin(), event_data_control.state_slots_.size()}
{
}

template <template <class> class AtomicIndirectorType>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from 'selected_index.value()' in case the selected_index doesn't
// have a value but as we check before with 'has_value()' so no way for throwing std::bad_optional_access which leds
// to std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto ProviderEventDataControlLocalView<AtomicIndirectorType>::AllocateNextSlot() noexcept
    -> std::optional<SlotIndexType>
{
    std::uint64_t retry_counter{0U};

    for (; retry_counter <= MAX_ALLOCATE_RETRIES; ++retry_counter)
    {
        auto oldest_unused_slot_info_result = FindOldestUnusedSlot();
        if (!oldest_unused_slot_info_result.has_value())
        {
            continue;
        }

        if (TryAllocateSlot(oldest_unused_slot_info_result.value()).has_value())
        {
            LogPerformanceMetrics(retry_counter);
            return oldest_unused_slot_info_result.value().slot_index;
        }
    }
    LogPerformanceMetrics(retry_counter);
    return {};
}

template <template <class> class AtomicIndirectorType>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, no way for throwing std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto ProviderEventDataControlLocalView<AtomicIndirectorType>::FindOldestUnusedSlot() const noexcept
    -> std::optional<ProviderEventDataControlLocalView::SlotInfo>
{
    EventSlotStatus::EventTimeStamp oldest_time_stamp{EventSlotStatus::TIMESTAMP_MAX};
    std::optional<ProviderEventDataControlLocalView::SlotInfo> slot_info{};
    for (SlotIndexType slot_index = 0U;
         // Suppress "AUTOSAR C++14 A4-7-1" rule finding. This rule states: "An integer expression shall not lead to
         // loss.". As the maximum number of slots is std::uint16_t, so there is no case for a data loss here.
         // coverity[autosar_cpp14_a4_7_1_violation]
         slot_index < static_cast<SlotIndexType>(state_slots_.size());
         ++slot_index)
    {
        // coverity[autosar_cpp14_a5_3_2_violation]
        const EventSlotStatus status{AtomicIndirectorType<EventSlotStatus::value_type>::load(
            state_slots_[slot_index], std::memory_order_acquire)};

        if (status.IsInvalid())
        {
            slot_info = {{slot_index, static_cast<EventSlotStatus::value_type>(status)}};
            return slot_info;
        }

        const auto are_proxies_referencing_slot =
            status.GetReferenceCount() != static_cast<EventSlotStatus::SubscriberCount>(0U);
        if (!are_proxies_referencing_slot && !status.IsInWriting())
        {
            if (status.GetTimeStamp() < oldest_time_stamp)
            {
                oldest_time_stamp = status.GetTimeStamp();
                slot_info = {{slot_index, static_cast<EventSlotStatus::value_type>(status)}};
            }
        }
    }
    return slot_info;
}

template <template <class> class AtomicIndirectorType>
void ProviderEventDataControlLocalView<AtomicIndirectorType>::SetSlotValue(const SlotInfo slot_info) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(static_cast<std::size_t>(slot_info.slot_index) < state_slots_.size());
    state_slots_[slot_info.slot_index].store(slot_info.slot_value, std::memory_order_release);
}

template <template <class> class AtomicIndirectorType>
auto ProviderEventDataControlLocalView<AtomicIndirectorType>::EventReady(
    const SlotIndexType slot_index,
    const EventSlotStatus::EventTimeStamp time_stamp) noexcept -> void
{
    const EventSlotStatus initial{time_stamp, 0U};
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(static_cast<std::size_t>(slot_index) < state_slots_.size());
    state_slots_[slot_index].store(
        static_cast<EventSlotStatus::value_type>(initial));  // no race-condition can happen, since event sender has
                                                             // to be single-threaded/non-concurrent per AoU
}

template <template <class> class AtomicIndirectorType>
auto ProviderEventDataControlLocalView<AtomicIndirectorType>::Discard(const SlotIndexType slot_index) -> void
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
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be
// called implicitly". std::terminate() is implicitly called from 'state_slots_[]' which might leds to a
// segmentation fault in case the index goes outside the range. As we already do an index check before
// accessing, so no way for segmentation fault which leds to calling std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto ProviderEventDataControlLocalView<AtomicIndirectorType>::TryAllocateSlot(const SlotInfo slot_info) noexcept
    -> std::optional<EventSlotStatus::value_type>
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(static_cast<std::size_t>(slot_info.slot_index) < state_slots_.size());

    EventSlotStatus in_writing{};
    in_writing.MarkInWriting();
    const auto in_writing_value = static_cast<EventSlotStatus::value_type>(in_writing);

    auto old_slot_value = slot_info.slot_value;
    const auto was_slot_allocated = AtomicIndirectorType<EventSlotStatus::value_type>::compare_exchange_strong(
        state_slots_[slot_info.slot_index], old_slot_value, in_writing_value, std::memory_order_acq_rel);
    if (!was_slot_allocated)
    {
        return {};
    }
    return old_slot_value;
}

template <template <class> class AtomicIndirectorType>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be
// called implicitly". std::terminate() is implicitly called from 'state_slots_[]' which might leds to a
// segmentation fault in case the index goes outside the range. As we already do an index check before accessing, so
// no way for segmentation fault which leds to calling std::terminate(). coverity[autosar_cpp14_a15_5_3_violation :
// FALSE]
auto ProviderEventDataControlLocalView<AtomicIndirectorType>::operator[](const SlotIndexType slot_index) const noexcept
    -> EventSlotStatus
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(static_cast<std::size_t>(slot_index) < state_slots_.size());
    return static_cast<EventSlotStatus>(state_slots_[slot_index].load(std::memory_order_acquire));
}

template <template <class> class AtomicIndirectorType>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be
// called implicitly". This is a false positive, no way for throwing std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto ProviderEventDataControlLocalView<AtomicIndirectorType>::RemoveAllocationsForWriting() noexcept -> void
{
    // Suppres "AUTOSAR C++14 A5-3-2" finding rule. This rule states: "Null pointers shall not be dereferenced.".
    // The "slot" variable must never be a null pointer, since DynamicArray allocates its elements when it is
    // created. coverity[autosar_cpp14_a5_3_2_violation]
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
auto ProviderEventDataControlLocalView<AtomicIndirectorType>::DumpPerformanceCounters() -> void
{
    std::cout << "EventDataControl performance breakdown\n"
              << "======================================\n"
              << "\nnum_alloc_misses:  " << num_alloc_misses << "\nnum_alloc_retries: "
              << num_alloc_retries
              // Suppress AUTOSAR C++14 M8-4-4, rule finding: "A function identifier shall either be used to call
              // the function or it shall be preceded by &". Passing std::endl to std::cout object with the stream
              // operator follows the idiomatic way that both features in conjunction were designed in the C++
              // standard. coverity[autosar_cpp14_m8_4_4_violation : FALSE]
              << std::endl;
}

template <template <class> class AtomicIndirectorType>
void ProviderEventDataControlLocalView<AtomicIndirectorType>::LogPerformanceMetrics(
    std::uint64_t retry_counter) noexcept
{
    score::cpp::ignore = num_alloc_retries.fetch_add(retry_counter);

    if ((retry_counter >= 100U))
    {
        ++num_alloc_misses;
    }
}

template <template <class> class AtomicIndirectorType>
auto ProviderEventDataControlLocalView<AtomicIndirectorType>::ResetPerformanceCounters() -> void
{
    num_alloc_misses.store(0U);
    num_alloc_retries.store(0U);
}

template class ProviderEventDataControlLocalView<memory::shared::AtomicIndirectorReal>;
template class ProviderEventDataControlLocalView<memory::shared::AtomicIndirectorMock>;

}  // namespace score::mw::com::impl::lola
