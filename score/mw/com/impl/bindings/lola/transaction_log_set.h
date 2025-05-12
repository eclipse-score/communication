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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_TRANSACTION_LOG_SET_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_TRANSACTION_LOG_SET_H

#include "score/containers/dynamic_array.h"
#include "score/mw/com/impl/bindings/lola/transaction_log.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_id.h"
#include "score/mw/com/impl/configuration/lola_event_instance_deployment.h"
#include "score/mw/com/impl/util/copyable_atomic.h"

#include "score/memory/shared/memory_resource_proxy.h"
#include "score/memory/shared/polymorphic_offset_ptr_allocator.h"
#include "score/result/result.h"

#include <atomic>

namespace score::mw::com::impl::lola
{

/// \brief A TransactionLogSet instance keeps track of all the TransactionLogs for all the Proxy service elements
///        corresponding to a specific Skeleton service element. It also tracks a separate TransactionLog for the
///        Skeleton service element in case tracing is enabled for the given service element.
/// \details
/// Synchronisation: The TransactionLogSet consists of elements (of type TransactionLogNode) of a DynamicArray
/// containing: A TransactionLogId and a TransactionLog.
/// Each TransactionLog will be used by a single Proxy service element in a single thread. However, proxies in different
/// processes/threads can concurrently try to allocate such an element (i.e. a transaction log represented by a
/// TransactionLogNode) for their use during Subscribe/RegisterProxyElement.
/// Therefore, the allocation of such an element in the DynamicArray has to be synchronized.
/// This is achieved by a lock-free/atomics based approach. The transaction_log_id_ member of a TransactionLogNode is an
/// atomic, which serves the synchronization purpose. It will be initialized with kInvalidTransactionLogId and
/// concurrent proxies will try to allocate such a TransactionLogNode via compare_exchange to its own TransactionLogId
/// (uid).
/// Once such an allocation was successful, the proxy thread will uniquely work on this TransactionLogNode until it
/// releases it again in Unsubscribe/Unregister.
/// Note, that there can be also a concurrency of proxies within the same process, all working with the same,
/// TransactionLogId, which happens in the seldom case, that there are multiple proxies for the very same service
/// instance within one process. Their potential concurrency, when trying to rollback transaction logs with the same
/// TransactionLogId is synchronized via transaction_log_mutex_, which is a process local standard mutex.
///
/// We use a DynamicArray instead of a map because we need to set the maximum size of the data structure (i.e. one
/// element per Proxy service element) and this is either not possible or not trivial with a hash map. Also the
/// implementation of the lock-free algo, which needs a fast/repeated iteration over all elements would have been
/// harder. We think that iterating over this DynamicArray should be very quick due to the limited size of the
/// DynamicArray and CPU caching (similar to the control DynamicArray in EventDataControl).
class TransactionLogSet
{
    // Suppress "AUTOSAR C++14 A11-3-1", The rule declares: "Friend declarations shall not be used".
    // The "TransactionLogSetAttorney" class is a helper, which sets the internal state of "TransactionLogSet" accessing
    // private members and used for testing purposes only.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class TransactionLogSetAttorney;

  public:
    using TransactionLogIndex = LolaEventInstanceDeployment::SubscriberCountType;

    /// \brief Struct that stores the status of a given TransactionLog
    class TransactionLogNode
    {
      public:
        TransactionLogNode(const std::size_t number_of_slots,
                           const memory::shared::MemoryResourceProxy* const proxy) noexcept
            : needs_rollback_{false},
              transaction_log_id_{kInvalidTransactionLogId},
              transaction_log_(number_of_slots, proxy)
        {
        }

        bool IsActive() const noexcept
        {
            return (transaction_log_id_.GetUnderlying().load() != kInvalidTransactionLogId);
        }

        bool NeedsRollback() const noexcept
        {
            return needs_rollback_;
        }

        /// \brief tries to acquire a given TransactionLogNode for the given transaction_log_id
        /// \return false, if it was not assigned a kInvalidTransactionLogId before and therefore the change failed.
        bool TryAcquire(TransactionLogId transaction_log_id) noexcept;

        /// \brief Checks, whether the instance is currently assigned to transaction_log_id
        bool TryAcquireForRead(TransactionLogId transaction_log_id) noexcept;

        void Release()
        {
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(transaction_log_id_.GetUnderlying().load() != kInvalidTransactionLogId,
                                   "Trying to Release() TransactionLogNode, which was not acquired.");
            transaction_log_id_.GetUnderlying().store(kInvalidTransactionLogId);
        }

        void MarkNeedsRollback(const bool needs_rollback) noexcept
        {
            needs_rollback_ = needs_rollback;
        }

        TransactionLogId GetTransactionLogId() const noexcept
        {
            return transaction_log_id_;
        }

        // Suppress "AUTOSAR C++14 A9-3-1" rule finding: "Member functions shall not return non-const “raw” pointers or
        // references to private or protected data owned by the class.".
        // To avoid overhead such as shared_ptr in the result, a non-const reference to the instance is returned
        // instead. This instance exposes another sub-API that can change its state and therefore also the state of
        // instance holder. API callers get the reference and use it in place without leaving the scope, so the
        // reference remains valid.
        // coverity[autosar_cpp14_a9_3_1_violation]
        TransactionLog& GetTransactionLog() noexcept
        {
            // coverity[autosar_cpp14_a9_3_1_violation] see above
            return transaction_log_;
        }

        void Reset() noexcept;

      private:
        /// \brief Whether or not the TransactionLog was created before a process crash.
        ///
        /// Will be set on Proxy::Create by the first Proxy in the same process with the same transaction_log_id. Will
        /// be cleared once Rollback is called on transaction_log.
        bool needs_rollback_;

        /// \brief Expresses, who (which proxy process) currently owns this transaction log. An (initially set) value
        ///        of kInvalidTransactionLogId means, that it is not yet owned by anybody.
        /// \details This is an atomic as our lock-free synchronization mechanism to synchronize access to
        ///          TransactionLogSet#proxy_transaction_logs_ is built upon this atomic!
        CopyableAtomic<TransactionLogId> transaction_log_id_;
        TransactionLog transaction_log_;
    };

    /// \brief Sentinel index value used to identify the skeleton_tracing_transaction_log_
    ///
    /// This value will be returned by RegisterSkeletonTracingElement() and when passed to GetTransactionLog(), the
    /// skeleton_tracing_transaction_log_ will be returned. We do this rather than having an additional
    /// GetTransactionLog overload for returned skeleton_tracing_transaction_log_ so that calling code can be agnostic
    /// to whether they're dealing with a proxy or skeleton transaction log.
    // Suppress "AUTOSAR C++14 A0-1-1", The rule states: "A project shall not contain instances of non-volatile
    // variables being given values that are not subsequently used"
    // Variable is used in the implementation file (cpp).
    // coverity[autosar_cpp14_a0_1_1_violation : FALSE]
    static constexpr const TransactionLogIndex kSkeletonIndexSentinel{std::numeric_limits<TransactionLogIndex>::max()};

    /// \brief Constructor
    /// \param max_number_of_logs The maximum number of logs that can be registered via Register().
    /// \param number_of_slots number of slots each of the transaction logs within the TransactionLogSet will contain.
    ///        It is deduced by the number_of_slots, the skeleton created for the related event/field service element.
    /// \param proxy The MemoryResourceProxy that will be used by the DynamicArray of transaction logs
    TransactionLogSet(const TransactionLogIndex max_number_of_logs,
                      const std::size_t number_of_slots,
                      const memory::shared::MemoryResourceProxy* const proxy) noexcept;
    ~TransactionLogSet() noexcept = default;

    TransactionLogSet(const TransactionLogSet&) = delete;
    TransactionLogSet& operator=(const TransactionLogSet&) = delete;
    TransactionLogSet(TransactionLogSet&&) noexcept = delete;
    TransactionLogSet& operator=(TransactionLogSet&& other) noexcept = delete;

    void MarkTransactionLogsNeedRollback(const TransactionLogId& transaction_log_id) noexcept;

    /// \brief Rolls back all Proxy TransactionLogs corresponding to the provided TransactionLogId.
    /// \return Returns an a blank result if the rollback succeeded or did not need to be done (because there's no
    ///         TransactionLog associated with the provided TransactionLogId or another Proxy instance with the same
    ///         TransactionLogId in the same process already performed the rollback), otherwise, an error.
    ///
    /// Multiple instances of the same Proxy service element will have the same transaction_log_id. Therefore, the first
    /// call to RollbackProxyTransactions per process will rollback _all_ the TransactionLogs corresponding to
    /// transaction_log_id. Any further calls to RollbackProxyTransactions within the same process will not perform any
    /// rollbacks. This prevents one thread calling RollbackProxyTransactions and then registering a new TransactionLog.
    /// Then another thread with the same transaction_log_id calls RollbackProxyTransactions which would rollback and
    /// destroy the newly created TransactionLog.
    ResultBlank RollbackProxyTransactions(const TransactionLogId& transaction_log_id,
                                          const TransactionLog::DereferenceSlotCallback dereference_slot_callback,
                                          const TransactionLog::UnsubscribeCallback unsubscribe_callback) noexcept;

    /// \brief If a Skeleton TransactionLog exists, performs a rollback on it.
    ResultBlank RollbackSkeletonTracingTransactions(
        const TransactionLog::DereferenceSlotCallback dereference_slot_callback) noexcept;

    /// \brief Creates a new transaction log in the DynamicArray of transaction logs.
    ///
    /// Will terminate if transaction_log_id already exists within the DynamicArray of transaction logs.
    score::Result<TransactionLogSet::TransactionLogIndex> RegisterProxyElement(
        const TransactionLogId& transaction_log_id) noexcept;

    /// \brief Creates a new skeleton tracing transaction log
    /// \return Returns kSkeletonIndexSentinel which is a special sentinel value which will return the registered
    /// skeleton tracing transaction log when passing the sentinel value to GetTransactionLog.
    ///
    /// Will terminate if a skeleton tracing transaction log was already registered.
    TransactionLogIndex RegisterSkeletonTracingElement() noexcept;

    /// \brief Deletes the element (by resetting its TransactionLogId to initial/unused state) in the DynamicArray of
    ///        transaction logs corresponding to the provided index.
    ///
    /// Must not be called concurrently with GetTransactionLog() with the same transaction_log_index.
    void Unregister(const TransactionLogIndex transaction_log_index) noexcept;

    /// \brief Returns a reference to a TransactionLog corresponding to the provided index.
    ///
    /// Must not be called concurrently with Unregister() with the same transaction_log_index.
    TransactionLog& GetTransactionLog(const TransactionLogIndex transaction_log_index) noexcept;

  private:
    using TransactionLogCollection =
        score::containers::DynamicArray<TransactionLogNode,
                                      memory::shared::PolymorphicOffsetPtrAllocator<TransactionLogNode>>;

    /// \brief Returns iterators to all TransactionLogNodes for the given target_transaction_log_id, which needs roll
    /// back.
    /// \param target_transaction_log_id transaction log id to be searched for
    /// \return vector of iterators
    std::vector<TransactionLogSet::TransactionLogCollection::iterator> FindTransactionLogNodesToBeRolledBack(
        const TransactionLogId& target_transaction_log_id);

    /// \brief Acquires next available/free transaction log from the proxy transaction logs.
    /// \param transaction_log_id
    /// \return if slot could be acquired, the returned optional contains a pair of an iterator to the allocated slot
    ///         (TransactionLogNode) and its index, otherwise an empty optional
    std::optional<
        std::pair<TransactionLogSet::TransactionLogCollection::iterator, TransactionLogSet::TransactionLogIndex>>
    AcquireNextAvailableSlot(TransactionLogId transaction_log_id);

    static bool IsSkeletonElementTransactionLogIndex(
        const TransactionLogSet::TransactionLogIndex transaction_log_index);

    TransactionLogCollection proxy_transaction_logs_;
    TransactionLogNode skeleton_tracing_transaction_log_;
    const memory::shared::MemoryResourceProxy* proxy_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_TRANSACTION_LOG_SET_H
