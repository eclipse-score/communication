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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_DATA_CONTROL_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_DATA_CONTROL_H

#include "score/mw/com/impl/bindings/lola/event_slot_status.h"

#include "score/mw/com/impl/bindings/lola/control_slot_indicator.h"
#include "score/mw/com/impl/bindings/lola/control_slot_types.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_id.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_set.h"

#include "score/memory/shared/memory_resource_proxy.h"
#include "score/memory/shared/polymorphic_offset_ptr_allocator.h"

#include "score/memory/shared/atomic_indirector.h"

#include "score/containers/dynamic_array.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

namespace score::mw::com::impl::lola
{
// Suppress The rule AUTOSAR C++14 M3-2-3: "A type, object or function that is used in multiple translation units shall
// be declared in one and only one file."
// This is a forward declaration that does not vioalate this rule.
// coverity[autosar_cpp14_m3_2_3_violation]
class EventDataControlAttorney;
// Suppress The rule AUTOSAR C++14 M3-2-3: "A type, object or function that is used in multiple translation units shall
// be declared in one and only one file."
// This is a forward declaration that does not vioalate this rule.
// coverity[autosar_cpp14_m3_2_3_violation]
class EventDataControlCompositeAttorney;

namespace detail_event_data_control_composite
{
// Suppress The rule AUTOSAR C++14 M3-2-3: "A type, object or function that is used in multiple translation units shall
// be declared in one and only one file."
// This is a forward declaration that does not vioalate this rule.
// coverity[autosar_cpp14_m3_2_3_violation]
template <template <class> class T>
class EventDataControlCompositeImpl;
}  // namespace detail_event_data_control_composite

namespace detail_event_data_control
{

/// \brief EventDataControlImpl encapsulates the overall control information for one event. It is stored in Shared
/// Memory.
///
/// \details Underlying EventDataControlImpl holds a dynamic array of multiple slots, which hold EventSlotStatus. The
/// event has another equally sized dynamic array of slots which will contain the data. Both data points (data and
/// control information) are related by their slot index. The number of slots is configured on construction (start-up of
/// a process).
///
/// It is one of the corner stone elements of our LoLa IPC for Events!
template <template <class> class AtomicIndirectorType = memory::shared::AtomicIndirectorReal>
class EventDataControlImpl final
{
    // Suppress "AUTOSAR C++14 A11-3-1", The rule declares: "Friend declarations shall not be used".
    // The "EventDataControlAttorney" class is a helper, which sets the internal state of
    // "EventDataControlImpl" accessing private members and used for testing purposes only.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class lola::EventDataControlAttorney;
    // Suppress "AUTOSAR C++14 A11-3-1", The rule declares: "Friend declarations shall not be used".
    // The "EventDataControlCompositeAttorney" class is a helper, which sets the internal state of
    // "EventDataControlImpl" accessing private members and used for testing purposes only.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class lola::EventDataControlCompositeAttorney;

  public:
    using EventControlSlots =
        score::containers::DynamicArray<ControlSlotType, memory::shared::PolymorphicOffsetPtrAllocator<ControlSlotType>>;
    static_assert(ControlSlotType::is_always_lock_free, "According to high level design, SlotType must be lock free.");

    /// \brief Will construct EventDataControlImpl and dynamically allocate memory on provided resource on
    /// construction
    /// \param max_slots The number of slots that shall be allocated (const afterwards)
    /// \param proxy The memory resource proxy where the memory shall be allocated (e.g. Shared Memory)
    /// \param max_number_combined_subscribers The max number of subscribers which can subscribe to the SkeletonEvent
    ///        owning this EventDataControl at any one time.
    EventDataControlImpl(
        const SlotIndexType max_slots,
        const score::memory::shared::MemoryResourceProxy* const proxy,
        const LolaEventInstanceDeployment::SubscriberCountType max_number_combined_subscribers) noexcept;
    ~EventDataControlImpl() noexcept = default;

    EventDataControlImpl(const EventDataControlImpl&) = delete;
    EventDataControlImpl& operator=(const EventDataControlImpl&) = delete;
    EventDataControlImpl(EventDataControlImpl&&) noexcept = delete;
    EventDataControlImpl& operator=(EventDataControlImpl&& other) noexcept = delete;

    /// \brief Checks for the oldest unused slot and acquires for writing (thread-safe, wait-free)
    ///
    /// \details This method will perform retries (bounded) on data-races. In order to ensure that _always_
    /// a slot is found, it needs to be ensured that:
    /// * enough slots are allocated (sum of all possible max allocations by consumer + 1)
    /// * enough retries are performed (currently max number of parallel actions is restricted to 50 (number of
    /// possible transactions (2) * number of parallel actions = number of retries))
    ///
    /// \return reserved slot for writing in the form of a valid ControlSlotIndicator if found, invalid
    ///         ControlSlotIndicator otherwise
    /// \post EventReady() is invoked to withdraw write-ownership
    ControlSlotIndicator AllocateNextSlot() noexcept;

    /// \brief Indicates that a slot is ready for reading - writing has finished. (thread-safe, wait-free)
    /// \pre AllocateNextSlot() was invoked to obtain write-ownership
    void EventReady(ControlSlotIndicator slot_indicator, const EventSlotStatus::EventTimeStamp time_stamp) noexcept;

    /// \brief Marks selected slot as invalid, if it was not yet marked as ready
    ///
    /// \details We don't discard elements that are already ready, since it is possible that a user might already
    /// read them. This just might be the case if a SampleAllocateePtr is destroyed after invoking Send().
    ///
    /// \pre AllocateNextSlot() was invoked to obtain write-ownership
    void Discard(ControlSlotIndicator slot_indicator);

    /// \brief Increments refcount of given slot by one (given it is in the correct state i.e. being accessible/
    ///        readable)
    /// \details This is a specific feature - not used by the standard proxy/consumer, which is using
    ///          ReferenceNextEvent(). This API has been introduced in the context of IPC-Tracing, where a skeleton is
    ///          referencing/using a slot it just has allocated to trace out the content via Trace-API and
    ///          de-referencing it after tracing of the slot data has been accomplished.
    ///          IMPORTANT: This function is _ONLY_ threadsafe with another function incrementing the ref count of a
    ///          slot (e.g. via ReferenceNextEvent). The function must _NOT_ be called in a context in which another
    ///          thread could mark the slot as invalid or marked for writing concurrently with this function. If this
    ///          function is called by the SkeletonEvent itself before handing out a SampleAllocateePtr to the user,
    ///          then it is safe.
    /// \param slot_index index of the slot to be referenced.
    void ReferenceSpecificEvent(const SlotIndexType slot_index,
                                const TransactionLogSet::TransactionLogIndex transaction_log_index) noexcept;

    /// \brief Will search for the next slot that shall be read, after the last time and mark it for reading
    /// \param last_search_time The time stamp the last time a search for an event was performed
    ///
    /// \details This method will perform retries (bounded) on data-races. I.e. if a viable slot failed to be marked
    /// for reading because of a data race, retries are made.
    ///
    /// \return Will return a valid ControlSlotIndicator indicating/pointing to an event, which is the youngest/newest
    ///         one between last_search_time and upper_limit. I.e. its timestamp is between last_search_time and
    ///         upper_limit and any other event with timestamp between last_search_time and upper_limit has a smaller
    ///         timestamp (is older).
    ///         If no such event exists, an invalid ControlSlotIndicator is returned.
    /// \post DereferenceEvent() is invoked to withdraw read-ownership
    ControlSlotIndicator ReferenceNextEvent(
        const EventSlotStatus::EventTimeStamp last_search_time,
        const TransactionLogSet::TransactionLogIndex transaction_log_index,
        const EventSlotStatus::EventTimeStamp upper_limit = EventSlotStatus::TIMESTSCORE_LANGUAGE_FUTURECPP_MAX) noexcept;

    /// \brief Returns number/count of events within event slots, which are newer than the given timestamp.
    /// \param reference_time given reference timestamp.
    /// \return number/count of available events, which are newer than the given reference_time.
    std::size_t GetNumNewEvents(const EventSlotStatus::EventTimeStamp reference_time) const noexcept;

    /// \brief Indicates that a consumer is finished reading (thread-safe, wait-free)
    /// \pre ReferenceNextEvent() was invoked to obtain read-ownership
    ///
    /// \details Will also record the transaction in the TransactionLog corresponding to transaction_log_index
    void DereferenceEvent(ControlSlotIndicator slot_indicator,
                          const TransactionLogSet::TransactionLogIndex transaction_log_index) noexcept;

    /// \brief Indicates that a consumer is finished reading (thread-safe, wait-free).
    /// \pre ReferenceNextEvent() was invoked to obtain read-ownership
    ///
    /// \details Will not record the transaction in any TransactionLog. This function is called by the
    /// TransactionLog::DereferenceSlotCallback created within TransactionLogSet::RollbackProxyTransactions resp.
    /// RollbackSkeletonTracingTransactions. In these cases, the transaction will be recorded within
    /// TransactionLog::RollbackIncrementTransactions resp. RollbackSubscribeTransactions before calling the callback.
    void DereferenceEventWithoutTransactionLogging(const SlotIndexType event_slot_index) noexcept;

    /// \brief Directly access EventSlotStatus for one specific slot
    EventSlotStatus operator[](const SlotIndexType slot_index) const noexcept;

    /// \brief Marks all Slots which are `InWriting` as `Invalid`.
    /// \details This function shall _only_ be called on skeleton side and _only_ if a previous skeleton instance died.
    void RemoveAllocationsForWriting() noexcept;

    // Suppress "AUTOSAR C++14 A9-3-1" rule finding: "Member functions shall not return non-const “raw” pointers or
    // references to private or protected data owned by the class.".
    // To avoid overhead such as shared_ptr in the result, a non-const reference to the instance is returned instead.
    // This instance exposes another sub-API that can change the its state and therefore also the state of instance
    // holder. API callers get the reference and use it in place without leaving the scope, so the reference remains
    // valid.
    // coverity[autosar_cpp14_a9_3_1_violation]
    TransactionLogSet& GetTransactionLogSet() noexcept
    {
        // coverity[autosar_cpp14_a9_3_1_violation] see above
        return transaction_log_set_;
    }

    /// \brief Returns the max sample slots set on creation of EventDataControl
    std::size_t GetMaxSampleSlots() const noexcept
    {
        return state_slots_.size();
    }

    // helper for performance indication (no production usage)
    static void DumpPerformanceCounters();
    static void ResetPerformanceCounters();

  private:
    /// \brief Finds oldest unused slot within control slots, if there is any.
    /// \return if an unused slot is found, returns a ControlSlotIndicator, which is valid, otherwise an invalid
    ///         ControlSlotIndicator is returned.
    ControlSlotIndicator FindOldestUnusedSlot() noexcept;

    // Shared Memory ready :)!
    // we don't implement a smarter structure and just iterate through it, because we believe that by
    // cache optimization this is way faster than e.g. a tree, since a tree also needs to be
    // implemented wait-free.
    EventControlSlots state_slots_;

    TransactionLogSet transaction_log_set_;

    // helper variables to calculated performance indicators
    static inline std::atomic_uint_fast64_t num_alloc_misses{0U};
    static inline std::atomic_uint_fast64_t num_ref_misses{0U};
    static inline std::atomic_uint_fast64_t num_alloc_retries{0U};
    static inline std::atomic_uint_fast64_t num_ref_retries{0U};

    template <template <typename> class T>
    // Suppress "AUTOSAR C++14 A11-3-1", The rule declares: "Friend declarations shall not be used".
    // In order that users do not depend on implementation details, we only expose on user facing classes the bare
    // necessary. Thus, we have friend classes that expose internals for our implementation. Design decision for better
    // encapsulation.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class detail_event_data_control_composite::EventDataControlCompositeImpl;
};

}  // namespace detail_event_data_control

using EventDataControl = detail_event_data_control::EventDataControlImpl<>;

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_DATA_CONTROL_H
