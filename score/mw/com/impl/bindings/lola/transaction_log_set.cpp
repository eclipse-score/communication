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
#include "score/mw/com/impl/bindings/lola/transaction_log_set.h"

#include "score/result/result.h"
#include "score/mw/com/impl/com_error.h"

#include <score/assert.hpp>

#include <limits>
#include <mutex>

namespace score::mw::com::impl::lola
{

bool TransactionLogSet::TransactionLogNode::TryAcquire(TransactionLogId transaction_log_id) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(transaction_log_id != kInvalidTransactionLogId,
                           "Called TransactionLogNode::TryAcquire with kInvalidTransactionLogId");
    TransactionLogId expected_transaction_log_id{kInvalidTransactionLogId};
    return transaction_log_id_.GetUnderlying().compare_exchange_strong(expected_transaction_log_id, transaction_log_id);
}

bool TransactionLogSet::TransactionLogNode::TryAcquireForRead(TransactionLogId transaction_log_id) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(transaction_log_id != kInvalidTransactionLogId,
                           "Called TransactionLogNode::TryAcquireForRead with kInvalidTransactionLogId");
    return (transaction_log_id_ == transaction_log_id);
}

void TransactionLogSet::TransactionLogNode::Reset() noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(!transaction_log_.ContainsTransactions(),
                           "Cannot Reset TransactionLog as it still contains some old transactions.");
    needs_rollback_ = false;
    Release();
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". The std::vector constructor we use is not marked as noexcept, in case TransactionLogNode is not
// CopyInsertable into std::vector<T>, the behavior is undefined. As TransactionLogNode is CopyInsertable so no way for
// throwing an exception which leds to calling std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
TransactionLogSet::TransactionLogSet(const TransactionLogIndex max_number_of_logs,
                                     const std::size_t number_of_slots,
                                     const memory::shared::MemoryResourceProxy* const proxy) noexcept
    : proxy_transaction_logs_(max_number_of_logs, TransactionLogNode{number_of_slots, proxy}, proxy),
      skeleton_tracing_transaction_log_{number_of_slots, proxy},
      proxy_{proxy}
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
        max_number_of_logs != kSkeletonIndexSentinel,
        "kSkeletonIndexSentinel is a reserved sentinel value so the max_number_of_logs must be reduced.");
}

void TransactionLogSet::MarkTransactionLogsNeedRollback(const TransactionLogId& transaction_log_id) noexcept
{
    for (auto& transaction_log_node : proxy_transaction_logs_)
    {
        const bool log_is_active = transaction_log_node.IsActive();
        // autosar_cpp14_a5_3_2_violation
        // the begin or end iterators of this range based for loop, can only be a nullpointer if proxy_transaction_logs_
        // is zero length DynamicArray. In which case BOTH begin and end will return a nullpointer and the loop
        // condition will fail before ever entering the loop. Thus derefferencing a nullpointer is impossible here.
        // coverity[autosar_cpp14_a5_3_2_violation : FALSE]
        const bool has_matching_id = (transaction_log_node.GetTransactionLogId() == transaction_log_id);
        if (log_is_active && has_matching_id)
        {
            transaction_log_node.MarkNeedsRollback(true);
        }
    }
}

ResultBlank TransactionLogSet::RollbackProxyTransactions(
    const TransactionLogId& transaction_log_id,
    const TransactionLog::DereferenceSlotCallback dereference_slot_callback,
    const TransactionLog::UnsubscribeCallback unsubscribe_callback) noexcept
{
    const auto transaction_log_node_iterators_to_be_rolled_back =
        FindTransactionLogNodesToBeRolledBack(transaction_log_id);

    // Keep trying to rollback a TransactionLog. If a rollback succeeds, return. If a rollback fails, try to rollback
    // the next TransactionLog. If there are only TransactionLogs remaining which cannot be rolled back, return an
    // error.
    ResultBlank rollback_result{};
    for (const auto transaction_log_node_it : transaction_log_node_iterators_to_be_rolled_back)
    {
        rollback_result = transaction_log_node_it->GetTransactionLog().RollbackProxyElementLog(
            dereference_slot_callback, unsubscribe_callback);
        if (rollback_result.has_value())
        {
            transaction_log_node_it->Reset();
            return {};
        }
    }
    return rollback_result;
}

ResultBlank TransactionLogSet::RollbackSkeletonTracingTransactions(
    const TransactionLog::DereferenceSlotCallback dereference_slot_callback) noexcept
{
    if (!skeleton_tracing_transaction_log_.IsActive())
    {
        return {};
    }
    const auto rollback_result =
        skeleton_tracing_transaction_log_.GetTransactionLog().RollbackSkeletonTracingElementLog(
            dereference_slot_callback);
    if (!rollback_result.has_value())
    {
        return rollback_result;
    }
    skeleton_tracing_transaction_log_.Reset();
    return {};
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from '.value()' in case it doesn't have a value but as we check
// before with 'has_value()' so no way for throwing std::bad_optional_access which leds to std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
score::Result<TransactionLogSet::TransactionLogIndex> TransactionLogSet::RegisterProxyElement(
    const TransactionLogId& transaction_log_id) noexcept
{
    const auto next_available_slot_result = AcquireNextAvailableSlot(transaction_log_id);
    if (!next_available_slot_result.has_value())
    {
        return MakeUnexpected(
            ComErrc::kMaxSubscribersExceeded,
            "Could not register with TransactionLogId as there are no available slots in the "
            "TransactionLogSet. This is likely because the number of subscribers has exceeded the configuration "
            "value of max_subscribers.");
    }
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(!next_available_slot_result.value().first->GetTransactionLog().ContainsTransactions(),
                           "Cannot reuse TransactionLog as it still contains some old transactions.");
    return next_available_slot_result.value().second;
}

TransactionLogSet::TransactionLogIndex TransactionLogSet::RegisterSkeletonTracingElement() noexcept
{
    // we only do have one skeleton instance accessing the skeleton transaction log, so a dummy value is good enough,
    // we don't need e.g. an uid here.
    constexpr TransactionLogId kDummyTransactionLogIdSkeleton{1};
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(!skeleton_tracing_transaction_log_.IsActive(),
                           "Can only register a single Skeleton Tracing element.");
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(skeleton_tracing_transaction_log_.TryAcquire(kDummyTransactionLogIdSkeleton),
                           "Unexpected failure to acquire TransactionLogNode for SkeletonEvent!");
    return kSkeletonIndexSentinel;
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from 'proxy_transaction_logs_.at()' which might throw
// std::out_of_range. As we already do an index check before accessing, so no way for throwing std::out_of_rang which
// leds to calling std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
void TransactionLogSet::Unregister(const TransactionLogIndex transaction_log_index) noexcept
{
    if (IsSkeletonElementTransactionLogIndex(transaction_log_index))
    {
        skeleton_tracing_transaction_log_.Reset();
    }
    else
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(static_cast<std::size_t>(transaction_log_index) < proxy_transaction_logs_.size());
        proxy_transaction_logs_.at(static_cast<std::size_t>(transaction_log_index)).Reset();
    }
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from 'proxy_transaction_logs_.at()' which might throw
// std::out_of_range As we already do an index check before accessing, so no way for throwing std::out_of_rang which
// leds to calling std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
TransactionLog& TransactionLogSet::GetTransactionLog(const TransactionLogIndex transaction_log_index) noexcept
{
    if (IsSkeletonElementTransactionLogIndex(transaction_log_index))
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(skeleton_tracing_transaction_log_.IsActive(),
                                     "Skeleton tracing transaction log must be registered before being retrieved.");
        return skeleton_tracing_transaction_log_.GetTransactionLog();
    }
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(static_cast<std::size_t>(transaction_log_index) < proxy_transaction_logs_.size());
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(proxy_transaction_logs_.at(static_cast<std::size_t>(transaction_log_index)).IsActive(),
                                 "Proxy tracing transaction log must be registered before being retrieved.");
    return proxy_transaction_logs_.at(static_cast<std::size_t>(transaction_log_index)).GetTransactionLog();
}

std::vector<TransactionLogSet::TransactionLogCollection::iterator>
TransactionLogSet::FindTransactionLogNodesToBeRolledBack(const TransactionLogId& target_transaction_log_id)
{
    std::vector<TransactionLogSet::TransactionLogCollection::iterator> found_node_iterators{};

    // autosar_cpp14_m5_0_15_violation
    // This rule has an explicit exception for using ++/-- operators on iterators, which is what is happening here.
    //
    // autosar_cpp14_a5_3_2_violation
    // `it` can only be a nullpointer if proxy_transaction_logs_ is zero length DynamicArray. In which case both begin
    // and end will return a nullpointer and the loop condition will fail before ever entering the loop. Thus
    // derefferencing a nullpointer is impossible here.
    //
    // coverity[autosar_cpp14_m5_0_15_violation]
    // coverity[autosar_cpp14_a5_3_2_violation : FALSE]
    for (auto it = proxy_transaction_logs_.begin(); it != proxy_transaction_logs_.end(); it++)
    {
        // coverity[autosar_cpp14_a5_3_2_violation : FALSE]
        const bool acquired = it->TryAcquireForRead(target_transaction_log_id);
        if (acquired)
        {
            if (it->NeedsRollback())
            {
                found_node_iterators.push_back(it);
            }
        }
    }
    return found_node_iterators;
}

std::optional<std::pair<TransactionLogSet::TransactionLogCollection::iterator, TransactionLogSet::TransactionLogIndex>>
TransactionLogSet::AcquireNextAvailableSlot(TransactionLogId transaction_log_id)
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(proxy_transaction_logs_.size() <= std::numeric_limits<std::uint8_t>::max(),
                           "proxy_transaction_logs_.size() does not fit uint8_t");
    //  The size of the transaction logs reflects the size of max subscribers and therefore the potential upper-bound
    //  of concurrent proxies accessing these transaction_logs, from which we deduce our max retry count!
    const std::uint8_t max_retry_count{static_cast<std::uint8_t>(proxy_transaction_logs_.size())};
    std::uint8_t retries{0};
    while (retries < max_retry_count)
    {
        // we iterate using iterators as it minimizes bounds-checking to start/end!
        TransactionLogSet::TransactionLogIndex index{0};

        // autosar_cpp14_m5_0_15_violation
        // This rule has an explicit exception for using ++/-- operators on iterators, which is what is happening here.
        //
        // autosar_cpp14_a5_3_2_violation
        // it can only be a nullpointer if proxy_transaction_logs_ is zero length DynamicArray. In which case both begin
        // and end will return a nullpointer and the loop condition will fail before ever entering the loop. Thus
        // derefferencing a nullpointer is impossible here.
        //
        // coverity[autosar_cpp14_m5_0_15_violation]
        // coverity[autosar_cpp14_a5_3_2_violation : FALSE]
        for (auto it = proxy_transaction_logs_.begin(); it != proxy_transaction_logs_.end(); it++)
        {
            auto& transaction_log_node = *it;
            const auto acquired = transaction_log_node.TryAcquire(transaction_log_id);
            if (acquired)
            {
                // coverity[autosar_cpp14_a5_3_2_violation]
                transaction_log_node.MarkNeedsRollback(false);
                return std::make_pair(it, index);
            }
            // Suppress "AUTOSAR C++14 A4-7-1" rule: "An integer expression shall not lead to data loss.".
            // The size check above guarantees that proxy_transaction_logs_ fits within the range of a uint8_t.
            // The index variable is incremented safely as its type matches the underlying type of the size check.
            // coverity[autosar_cpp14_a4_7_1_violation]
            index++;
        }
        retries++;
    }
    return {};
}

bool TransactionLogSet::IsSkeletonElementTransactionLogIndex(
    const TransactionLogSet::TransactionLogIndex transaction_log_index)
{
    return transaction_log_index == TransactionLogSet::kSkeletonIndexSentinel;
}

}  // namespace score::mw::com::impl::lola
