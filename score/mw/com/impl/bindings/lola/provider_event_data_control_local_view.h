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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_PROVIDER_EVENT_DATA_CONTROL_LOCAL_VIEW_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_PROVIDER_EVENT_DATA_CONTROL_LOCAL_VIEW_H

#include "score/mw/com/impl/bindings/lola/control_slot_types.h"
#include "score/mw/com/impl/bindings/lola/event_data_control.h"
#include "score/mw/com/impl/bindings/lola/event_slot_status.h"

#include "score/concurrency/atomic_indirector.h"

#include <score/assert.hpp>
#include <score/span.hpp>

#include <atomic>
#include <cstdint>
#include <optional>

namespace score::mw::com::impl::lola
{

// Suppress The rule AUTOSAR C++14 M3-2-3: "A type, object or function that is used in multiple translation units shall
// be declared in one and only one file."
// This is a forward declaration that does not vioalate this rule.
// coverity[autosar_cpp14_m3_2_3_violation]
template <template <class> class T>
class EventDataControlComposite;

/// \brief View class which provides functionality for interacting with EventDataControl.
///
/// \details EventDataControl contains control information for an event which is stored in shared memory. Accessing the
/// control information directly in shared memory during runtime requires using (dereferencing, copying etc.) OffsetPtrs
/// which can negatively affect performance. Therefore, the data in EventDataControl is created / opened once during
/// Skeleton / Proxy creation, and then is accessed during runtime via EventDataControlLocal.
template <template <class> class AtomicIndirectorType = concurrency::AtomicIndirectorReal>
class ProviderEventDataControlLocalView final
{
    template <template <typename> class T>
    // Suppress "AUTOSAR C++14 A11-3-1", The rule declares: "Friend declarations shall not be used".
    // In order that users do not depend on implementation details, we only expose on user facing classes the bare
    // necessary. Thus, we have friend classes that expose internals for our implementation. Design decision for better
    // encapsulation.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class EventDataControlComposite;

  public:
    struct SlotInfo
    {
        SlotIndexType slot_index;
        EventSlotStatus::value_type slot_value;
    };

    using LocalEventControlSlots = score::cpp::span<ControlSlotType>;

    ProviderEventDataControlLocalView(EventDataControl& event_data_control) noexcept;

    ~ProviderEventDataControlLocalView() noexcept = default;

    ProviderEventDataControlLocalView(const ProviderEventDataControlLocalView&) = delete;
    ProviderEventDataControlLocalView& operator=(const ProviderEventDataControlLocalView&) = delete;
    ProviderEventDataControlLocalView(ProviderEventDataControlLocalView&&) noexcept = delete;
    ProviderEventDataControlLocalView& operator=(ProviderEventDataControlLocalView&& other) noexcept = delete;

    /// \brief Checks for the oldest unused slot and acquires for writing (thread-safe, wait-free)
    ///
    /// \details This method will perform retries (bounded) on data-races. In order to ensure that _always_
    /// a slot is found, it needs to be ensured that:
    /// * enough slots are allocated (sum of all possible max allocations by consumer + 1)
    /// * enough retries are performed (currently max number of parallel actions is restricted to 50 (number of
    /// possible transactions (2) * number of parallel actions = number of retries))
    ///
    /// \return reserved slot for writing if found, empty otherwise
    /// \post EventReady() is invoked to withdraw write-ownership
    std::optional<SlotIndexType> AllocateNextSlot() noexcept;

    /// \brief Indicates that a slot is ready for reading - writing has finished. (thread-safe, wait-free)
    /// \pre AllocateNextSlot() was invoked to obtain write-ownership
    void EventReady(const SlotIndexType slot_index, const EventSlotStatus::EventTimeStamp time_stamp) noexcept;

    /// \brief Marks selected slot as invalid, if it was not yet marked as ready
    ///
    /// \details We don't discard elements that are already ready, since it is possible that a user might already
    /// read them. This just might be the case if a SampleAllocateePtr is destroyed after invoking Send().
    ///
    /// \pre AllocateNextSlot() was invoked to obtain write-ownership
    void Discard(const SlotIndexType slot_index);

    std::optional<EventSlotStatus::value_type> TryAllocateSlot(const SlotInfo slot_info) noexcept;

    /// \brief Directly access EventSlotStatus for one specific slot
    EventSlotStatus operator[](const SlotIndexType slot_index) const noexcept;

    /// \brief Marks all Slots which are `InWriting` as `Invalid`.
    /// \details This function shall _only_ be called on skeleton side and _only_ if a previous skeleton instance died.
    void RemoveAllocationsForWriting() noexcept;

    // helper for performance indication (no production usage)
    static void DumpPerformanceCounters();
    static void ResetPerformanceCounters();

  private:
    /// \brief Finds oldest unused slot within control slots, if there is any.
    /// \return if an unused slot is found, returns its index, otherwise, an empty optional is returned.
    std::optional<ProviderEventDataControlLocalView::SlotInfo> FindOldestUnusedSlot() const noexcept;

    /// \brief Logs performance metrics for slot allocation attempts.
    void LogPerformanceMetrics(std::uint64_t retry_counter) noexcept;

    /// \brief Sets the slot value for the given slot index.
    ///
    /// This is a helper function which is used by EventDataControlComposite to reset the value of a slot in case of a
    /// failed multi-slot allocation.
    void SetSlotValue(const SlotInfo slot_info) noexcept;

    LocalEventControlSlots state_slots_;

    // helper variables to calculated performance indicators
    static inline std::atomic_uint_fast64_t num_alloc_misses{0U};
    static inline std::atomic_uint_fast64_t num_alloc_retries{0U};
};

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be
// called implicitly". std::terminate() is implicitly called from 'state_slots_[]' which might leds to a
// segmentation fault in case the index goes outside the range. As we already do an index check before
// accessing, so no way for segmentation fault which leds to calling std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
template <template <class> class AtomicIndirectorType>
inline auto ProviderEventDataControlLocalView<AtomicIndirectorType>::TryAllocateSlot(const SlotInfo slot_info) noexcept
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

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from 'selected_index.value()' in case the selected_index doesn't
// have a value but as we check before with 'has_value()' so no way for throwing std::bad_optional_access which leds
// to std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
template <template <class> class AtomicIndirectorType>
inline auto ProviderEventDataControlLocalView<AtomicIndirectorType>::AllocateNextSlot() noexcept
    -> std::optional<SlotIndexType>
{
    constexpr std::uint64_t max_allocate_retries{100U};

    for (std::uint64_t retry_counter{0U}; retry_counter <= max_allocate_retries; ++retry_counter)
    {
        auto oldest_unused_slot_info_result = FindOldestUnusedSlot();
        if (!oldest_unused_slot_info_result.has_value())
        {
            continue;
        }

        if (TryAllocateSlot(oldest_unused_slot_info_result.value()).has_value())
        {
            // ToDo: Don't call non-inlined "optional" func on the hot path! Make it conditional/configurable.
            // LogPerformanceMetrics(retry_counter);
            return oldest_unused_slot_info_result.value().slot_index;
        }
    }
    // ToDo: Don't call non-inlined "optional" func on the hot path! Make it conditional/configurable.
    // LogPerformanceMetrics(retry_counter);
    return {};
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be
// called implicitly". std::terminate() is implicitly called from 'state_slots_[]' which might leds to a
// segmentation fault in case the index goes outside the range. As we already do an index check before
// accessing, so no way for segmentation fault which leds to calling std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
template <template <class> class AtomicIndirectorType>
inline auto ProviderEventDataControlLocalView<AtomicIndirectorType>::EventReady(
    const SlotIndexType slot_index,
    const EventSlotStatus::EventTimeStamp time_stamp) noexcept -> void
{
    const EventSlotStatus initial{time_stamp, 0U};
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(static_cast<std::size_t>(slot_index) < state_slots_.size());
    state_slots_[slot_index].store(
        static_cast<EventSlotStatus::value_type>(initial));  // no race-condition can happen, since event sender has
                                                             // to be single-threaded/non-concurrent per AoU
}

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_PROVIDER_EVENT_DATA_CONTROL_LOCAL_VIEW_H
