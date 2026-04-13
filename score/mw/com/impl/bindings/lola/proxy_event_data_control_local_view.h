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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_PROXY_EVENT_DATA_CONTROL_LOCAL_VIEW_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_PROXY_EVENT_DATA_CONTROL_LOCAL_VIEW_H

#include "score/mw/com/impl/bindings/lola/control_slot_types.h"
#include "score/mw/com/impl/bindings/lola/event_data_control.h"
#include "score/mw/com/impl/bindings/lola/event_slot_status.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_set.h"

#include "score/memory/shared/atomic_indirector.h"

#include <score/span.hpp>

#include <atomic>
#include <cstddef>
#include <optional>

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

/// \brief View class which provides functionality for interacting with EventDataControl.
///
/// \details EventDataControl contains control information for an event which is stored in shared memory. Accessing the
/// control information directly in shared memory during runtime requires using (dereferencing, copying etc.) OffsetPtrs
/// which can negatively affect performance. Therefore, the data in EventDataControl is created / opened once during
/// Skeleton / Proxy creation, and then is accessed during runtime via EventDataControlLocal.
template <template <class> class AtomicIndirectorType = memory::shared::AtomicIndirectorReal>
class ProxyEventDataControlLocalView final
{
    // Suppress "AUTOSAR C++14 A11-3-1", The rule declares: "Friend declarations shall not be used".
    // The "EventDataControlAttorney" class is a helper, which sets the internal state of
    // "EventDataControl" accessing private members and used for testing purposes only.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class lola::EventDataControlAttorney;
    // Suppress "AUTOSAR C++14 A11-3-1", The rule declares: "Friend declarations shall not be used".
    // The "EventDataControlCompositeAttorney" class is a helper, which sets the internal state of
    // "EventDataControl" accessing private members and used for testing purposes only.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class lola::EventDataControlCompositeAttorney;

  public:
    using LocalEventControlSlots = score::cpp::span<ControlSlotType>;

    ProxyEventDataControlLocalView(EventDataControl& event_data_control_shared) noexcept;

    ~ProxyEventDataControlLocalView() noexcept = default;

    ProxyEventDataControlLocalView(const ProxyEventDataControlLocalView&) = delete;
    ProxyEventDataControlLocalView& operator=(const ProxyEventDataControlLocalView&) = delete;
    ProxyEventDataControlLocalView(ProxyEventDataControlLocalView&&) noexcept = delete;
    ProxyEventDataControlLocalView& operator=(ProxyEventDataControlLocalView&& other) noexcept = delete;

    /// \brief Will search for the next slot that shall be read, after the last time and mark it for reading
    /// \param last_search_time The time stamp the last time a search for an event was performed
    ///
    /// \details This method will perform retries (bounded) on data-races. I.e. if a viable slot failed to be marked
    /// for reading because of a data race, retries are made.
    ///
    /// \return Will return the index of an event, which is the youngest/newest one between last_search_time and
    ///         upper_limit. I.e. its timestamp is between last_search_time and upper_limit and all other events with
    ///         timestamps between last_search_time and upper_limit have a smaller timestamp (are older). If no such
    ///         event exists, an empty optional is returned.
    /// \post DereferenceEvent() is invoked to withdraw read-ownership
    std::optional<SlotIndexType> ReferenceNextEvent(
        const EventSlotStatus::EventTimeStamp last_search_time,
        const TransactionLogSet::TransactionLogIndex transaction_log_index,
        const EventSlotStatus::EventTimeStamp upper_limit = EventSlotStatus::TIMESTAMP_MAX) noexcept;

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
                                const TransactionLogSet::TransactionLogIndex transaction_log_index);

    /// \brief Returns number/count of events within event slots, which are newer than the given timestamp.
    /// \param reference_time given reference timestamp.
    /// \return number/count of available events, which are newer than the given reference_time.
    std::size_t GetNumNewEvents(const EventSlotStatus::EventTimeStamp reference_time) const noexcept;

    /// \brief Indicates that a consumer is finished reading (thread-safe, wait-free)
    /// \pre ReferenceNextEvent() was invoked to obtain read-ownership
    ///
    /// \details Will also record the transaction in the TransactionLog corresponding to transaction_log_index
    void DereferenceEvent(const SlotIndexType slot_index,
                          const TransactionLogSet::TransactionLogIndex transaction_log_index) noexcept;

    /// \brief Indicates that a consumer is finished reading (thread-safe, wait-free).
    /// \pre ReferenceNextEvent() was invoked to obtain read-ownership
    ///
    /// \details Will not record the transaction in any TransactionLog. This function is called by the
    /// TransactionLog::DereferenceSlotCallback created within TransactionLogSet::RollbackProxyTransactions and
    /// RollbackSkeletonTracingTransactions. In these cases, the transaction will be recorded within
    /// TransactionLog::RollbackIncrementTransactions resp. RollbackSubscribeTransactions before calling the callback.
    void DereferenceEventWithoutTransactionLogging(const SlotIndexType event_slot_index) noexcept;

    /// \brief Directly access EventSlotStatus for one specific slot
    EventSlotStatus operator[](const SlotIndexType slot_index) const noexcept;

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
    LocalEventControlSlots state_slots_;

    std::reference_wrapper<TransactionLogSet> transaction_log_set_;

    // helper variables to calculated performance indicators
    static inline std::atomic_uint_fast64_t num_ref_misses{0U};
    static inline std::atomic_uint_fast64_t num_ref_retries{0U};
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_PROXY_EVENT_DATA_CONTROL_LOCAL_VIEW_H
