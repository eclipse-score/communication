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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_TRANSACTION_LOG_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_TRANSACTION_LOG_H

#include "score/mw/com/impl/bindings/lola/transaction_log_slot.h"

#include "score/memory/shared/memory_resource_proxy.h"
#include "score/memory/shared/polymorphic_offset_ptr_allocator.h"
#include "score/result/result.h"

#include <score/callback.hpp>

#include <cstdint>
#include <vector>

namespace score::mw::com::impl::lola
{

/// \brief Class which contains the state of a Proxy service element's (i.e. ProxyEvent / ProxyField) interaction with
///        shared memory.
///
/// Each Proxy service element instance will have its own TransactionLog which will record any Subscribe / Unsubscribe
/// calls as well as increments / decrements to the reference count of the corresponding Skeleton service element. The
/// TransactionLog has a Rollback function which undoes any previous operations that were recorded in the TransactionLog
/// so that the service element can be recreated (e.g. in the case of a crash).
class TransactionLog
{
    // Suppress "AUTOSAR C++14 A11-3-1", The rule declares: "Friend declarations shall not be used".
    // The "TransactionLogAttorney" class is a helper, which sets the internal state of "TransactionLog" accessing
    // private members and used for testing purposes only.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class TransactionLogAttorney;

  public:
    /// \todo Add a central location in which all the type aliases are placed so that these types align with their
    /// usages in other parts of the code base.
    using SlotIndexType = std::uint16_t;
    using MaxSampleCountType = std::uint16_t;

    using TransactionLogSlots =
        std::vector<TransactionLogSlot, memory::shared::PolymorphicOffsetPtrAllocator<TransactionLogSlot>>;

    /// \brief Callbacks called during Roll back
    ///
    /// These callbacks will be provided by reference and may be called multiple times by TransactionLogSet. Therefore,
    /// it should be ensured that it is safe to call these callbacks multiple times without violating any invariants in
    /// the state of the callbacks.
    using DereferenceSlotCallback = score::cpp::callback<void(SlotIndexType slot_index)>;
    using UnsubscribeCallback = score::cpp::callback<void(MaxSampleCountType subscription_max_sample_count)>;

    TransactionLog(std::size_t number_of_slots, const memory::shared::MemoryResourceProxy* proxy) noexcept;

    /// \brief Record Subscription / Unsubscription transactions
    ///
    /// The expected sequence for a full subscription and unsubscription is as follows:
    /// Initial state:                Begin -> false, End -> false
    /// SubscribeTransactionBegin:    Begin -> true,  End -> false
    /// SubscribeTransactionCommit:   Begin -> true,  End -> true
    /// UnsubscribeTransactionBegin:  Begin -> false, End -> true
    /// UnsubscribeTransactionCommit: Begin -> false, End -> false
    ///
    /// We set begin to false in UnsubscribeTransactionBegin so that we can differentiate between a failure during
    /// subscription or unsubscription.
    void SubscribeTransactionBegin(const std::size_t subscription_max_sample_count) noexcept;
    void SubscribeTransactionCommit() noexcept;
    void SubscribeTransactionAbort() noexcept;

    void UnsubscribeTransactionBegin() noexcept;
    void UnsubscribeTransactionCommit() noexcept;

    void ReferenceTransactionBegin(SlotIndexType slot_index) noexcept;
    void ReferenceTransactionCommit(SlotIndexType slot_index) noexcept;
    void ReferenceTransactionAbort(SlotIndexType slot_index) noexcept;

    void DereferenceTransactionBegin(SlotIndexType slot_index) noexcept;
    void DereferenceTransactionCommit(SlotIndexType slot_index) noexcept;

    /// \brief Rollback all previous increments and subscriptions that were recorded in the transaction log.
    /// \param dereference_slot_callback Callback which will decrement the slot in EventDataControl with the provided
    ///        index.
    /// \param unsubscribe_callback Callback which will perform the unsubscribe with the stored
    ///        subscription_max_sample_count_.
    ///
    /// This function should be called when trying to create a Proxy service element that had previously crashed. It
    /// will decrement all reference counts that the old Proxy had incremented in the EventDataControl which were
    /// recorded in this TransactionLog.
    ResultBlank RollbackProxyElementLog(const DereferenceSlotCallback& dereference_slot_callback,
                                        const UnsubscribeCallback& unsubscribe_callback) noexcept;

    /// \brief Rollback all previous increments that were recorded in the transaction log.
    /// \param dereference_slot_callback Callback which will decrement the slot in EventDataControl with the provided
    ///        index.
    ///
    /// This function should be called when trying to create a Skeleton service element that had previously crashed. It
    /// will decrement all reference counts that the old Skeleton (due to tracing) had incremented in the
    /// EventDataControl which were recorded in this TransactionLog.
    ResultBlank RollbackSkeletonTracingElementLog(const DereferenceSlotCallback& dereference_slot_callback) noexcept;

    /// \brief Checks whether the TransactionLog contains any transactions
    /// \return Returns true if there is at least one Subscribe transaction or Reference transaction that hasn't been
    ///         finished with a completed Unsubscribe or Dereference transaction.
    bool ContainsTransactions() const noexcept;

  private:
    ResultBlank RollbackIncrementTransactions(const DereferenceSlotCallback& dereference_slot_callback) noexcept;
    ResultBlank RollbackSubscribeTransactions(const UnsubscribeCallback& unsubscribe_callback) noexcept;

    /// \brief Vector containing one TransactionLogSlot for each slot in the corresponding control vector.
    TransactionLogSlots reference_count_slots_;

    /// \brief TransactionLogSlot in shared memory which will record subscribe / unsubscribe transactions.
    TransactionLogSlot subscribe_transactions_;

    /// \brief The max sample count used for the recorded subscription transaction.
    ///
    /// This is set in SubscribeTransactionBegin() and used in the UnsubscribeCallback which is called during Rollback()
    score::cpp::optional<MaxSampleCountType> subscription_max_sample_count_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_TRANSACTION_LOG_H
