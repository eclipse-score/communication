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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_EVENT_DATA_CONTROL_LOCAL_VIEW_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_EVENT_DATA_CONTROL_LOCAL_VIEW_H

#include "score/mw/com/impl/bindings/lola/control_slot_indicator.h"
#include "score/mw/com/impl/bindings/lola/control_slot_types.h"
#include "score/mw/com/impl/bindings/lola/event_data_control.h"
#include "score/mw/com/impl/bindings/lola/event_slot_status.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_set.h"

#include "score/memory/shared/atomic_indirector.h"

#include <score/span.hpp>

#include <atomic>

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
template <template <class> class AtomicIndirectorType = memory::shared::AtomicIndirectorReal>
class SkeletonEventDataControlLocalView final
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

    template <template <typename> class T>
    // Suppress "AUTOSAR C++14 A11-3-1", The rule declares: "Friend declarations shall not be used".
    // In order that users do not depend on implementation details, we only expose on user facing classes the bare
    // necessary. Thus, we have friend classes that expose internals for our implementation. Design decision for better
    // encapsulation.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class EventDataControlComposite;

  public:
    using LocalEventControlSlots = score::cpp::span<ControlSlotType>;

    SkeletonEventDataControlLocalView(EventDataControl& event_data_control) noexcept;

    ~SkeletonEventDataControlLocalView() noexcept = default;

    SkeletonEventDataControlLocalView(const SkeletonEventDataControlLocalView&) = delete;
    SkeletonEventDataControlLocalView& operator=(const SkeletonEventDataControlLocalView&) = delete;
    SkeletonEventDataControlLocalView(SkeletonEventDataControlLocalView&&) noexcept = delete;
    SkeletonEventDataControlLocalView& operator=(SkeletonEventDataControlLocalView&& other) noexcept = delete;

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

    /// \brief Indicates that a consumer is finished reading (thread-safe, wait-free).
    /// \pre ReferenceNextEvent() was invoked to obtain read-ownership
    ///
    /// \details Will not record the transaction in any TransactionLog. This function is called by the
    /// TransactionLog::DereferenceSlotCallback created within TransactionLogSet::RollbackProxyTransactions and
    /// RollbackSkeletonTracingTransactions. In these cases, the transaction will be recorded within
    /// TransactionLog::RollbackIncrementTransactions resp. RollbackSubscribeTransactions before calling the callback.
    void DereferenceEventWithoutTransactionLogging(const SlotIndexType event_slot_index) noexcept;

    // /// \brief Directly access EventSlotStatus for one specific slot
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

    // helper for performance indication (no production usage)
    static void DumpPerformanceCounters();
    static void ResetPerformanceCounters();

  private:
    /// \brief Finds oldest unused slot within control slots, if there is any.
    /// \return if an unused slot is found, returns a ControlSlotIndicator, which is valid, otherwise an invalid
    ///         ControlSlotIndicator is returned.
    ControlSlotIndicator FindOldestUnusedSlot() noexcept;

    LocalEventControlSlots state_slots_;

    std::reference_wrapper<TransactionLogSet> transaction_log_set_;

    // helper variables to calculated performance indicators
    static inline std::atomic_uint_fast64_t num_alloc_misses{0U};
    static inline std::atomic_uint_fast64_t num_alloc_retries{0U};
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_EVENT_DATA_CONTROL_LOCAL_VIEW_H
