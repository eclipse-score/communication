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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_TRANSACTION_LOG_LOCAL_VIEW_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_TRANSACTION_LOG_LOCAL_VIEW_H

#include "score/mw/com/impl/bindings/lola/control_slot_types.h"
#include "score/mw/com/impl/bindings/lola/transaction_log.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_slot.h"

#include "score/memory/shared/memory_resource_proxy.h"
#include "score/memory/shared/polymorphic_offset_ptr_allocator.h"
#include "score/result/result.h"

#include <score/assert.hpp>
#include <score/callback.hpp>
#include <score/span.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>

namespace score::mw::com::impl::lola
{

/// \brief View class which provides functionality for interacting with Transactionlog.
class TransactionLogLocalView
{
  public:
    using TransactionLogSlotsLocalView = score::cpp::span<TransactionLog::TransactionLogSlots::value_type>;

    /// \brief Callbacks called during Roll back
    ///
    /// These callbacks will be provided by reference and may be called multiple times by TransactionLogSet. Therefore,
    /// it should be ensured that it is safe to call these callbacks multiple times without violating any invariants in
    /// the state of the callbacks.
    using DereferenceSlotCallback = score::cpp::callback<void(TransactionLog::SlotIndexType slot_index)>;
    using UnsubscribeCallback =
        score::cpp::callback<void(TransactionLog::MaxSampleCountType subscription_max_sample_count)>;

    TransactionLogLocalView(TransactionLog& transaction_log) noexcept;

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

    // Hot-path optimization: ReferenceTransaction{Begin,Commit,Abort} are on the GetNewSamples() call chain (invoked
    // from ConsumerEventDataControlLocalView::ReferenceNextEvent() for every candidate slot). They are defined inline
    // here so the trivial fast path (Transaction-END bit not set) requires no function call. The rare, slow
    // contention-handling path (END bit set) is delegated to the out-of-line WaitForTransactionEndToBecomeFalse()
    // helper, keeping the inlined body small.
    void ReferenceTransactionBegin(SlotIndexType slot_index) noexcept
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION(slot_index < reference_count_slots_local_.size());
        TransactionLogSlot& slot = reference_count_slots_local_[static_cast<std::size_t>(slot_index)];
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION(!slot.GetTransactionBegin());
        if (slot.GetTransactionEnd())
        {
            WaitForTransactionEndToBecomeFalse(slot);
        }
        slot.SetTransactionBegin(true);
    }
    void ReferenceTransactionCommit(SlotIndexType slot_index) noexcept
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION(slot_index < reference_count_slots_local_.size());
        TransactionLogSlot& slot = reference_count_slots_local_[static_cast<std::size_t>(slot_index)];
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION(slot.GetTransactionBegin());
        if (slot.GetTransactionEnd())
        {
            WaitForTransactionEndToBecomeFalse(slot);
        }
        slot.SetTransactionEnd(true);
    }
    void ReferenceTransactionAbort(SlotIndexType slot_index) noexcept
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION(slot_index < reference_count_slots_local_.size());
        TransactionLogSlot& slot = reference_count_slots_local_[static_cast<std::size_t>(slot_index)];
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION(slot.GetTransactionBegin());
        if (slot.GetTransactionEnd())
        {
            WaitForTransactionEndToBecomeFalse(slot);
        }
        slot.SetTransactionBegin(false);
    }

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
    Result<void> RollbackProxyElementLog(const DereferenceSlotCallback& dereference_slot_callback,
                                         const UnsubscribeCallback& unsubscribe_callback) noexcept;

    /// \brief Rollback all previous increments that were recorded in the transaction log.
    /// \param dereference_slot_callback Callback which will decrement the slot in EventDataControl with the provided
    ///        index.
    ///
    /// This function should be called when trying to create a Skeleton service element that had previously crashed. It
    /// will decrement all reference counts that the old Skeleton (due to tracing) had incremented in the
    /// EventDataControl which were recorded in this TransactionLog.
    Result<void> RollbackSkeletonTracingElementLog(const DereferenceSlotCallback& dereference_slot_callback) noexcept;

    /// \brief Checks whether the TransactionLog contains any transactions
    /// \return Returns true if there is at least one Subscribe transaction or Reference transaction that hasn't been
    ///         finished with a completed Unsubscribe or Dereference transaction.
    bool ContainsTransactions() const noexcept;

  private:
    /// \brief Slow contention-handling path for the Reference* transactions: waits for the Transaction-END bit of the
    /// given slot to become false (retrying with backoff, terminating if it never clears). Kept out-of-line so the
    /// inlined Reference* fast paths stay small.
    static void WaitForTransactionEndToBecomeFalse(TransactionLogSlot& slot) noexcept;

    Result<void> RollbackIncrementTransactions(const DereferenceSlotCallback& dereference_slot_callback) noexcept;
    Result<void> RollbackSubscribeTransactions(const UnsubscribeCallback& unsubscribe_callback) noexcept;

    /// \brief View pointing to DynamicArray containing one TransactionLogSlot for each slot in the corresponding
    /// control vector.
    TransactionLogSlotsLocalView reference_count_slots_local_;

    /// \brief TransactionLogSlot in shared memory which will record subscribe / unsubscribe transactions.
    std::reference_wrapper<TransactionLogSlot> subscribe_transactions_;

    /// \brief The max sample count used for the recorded subscription transaction.
    ///
    /// This is set in SubscribeTransactionBegin() and used in the UnsubscribeCallback which is called during Rollback()
    std::reference_wrapper<std::optional<TransactionLog::MaxSampleCountType>> subscription_max_sample_count_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_TRANSACTION_LOG_LOCAL_VIEW_H
