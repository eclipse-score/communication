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
#include "score/mw/com/impl/bindings/lola/transaction_log_local_view.h"

#include <chrono>
#include <cstdint>
#include <thread>

#include "score/mw/com/impl/com_error.h"

#include "score/mw/log/logging.h"

namespace score::mw::com::impl::lola
{

namespace
{

// Handles race condition between concurrent SamplePtr destruction and creation for the same slot.
// Thread A may be suspended between decrementing refcount and clearing the transaction-END bit.
// This allows Thread A to complete its dereference transaction before proceeding.
void WaitForTransactionEndToBecomeFalse(TransactionLogSlot& slot) noexcept
{
    constexpr std::uint8_t kRetryCount = 10U;
    constexpr std::chrono::milliseconds kRetryInterval(10);
    for (std::uint8_t retry = 0U; retry < kRetryCount; ++retry)
    {
        if (!slot.GetTransactionEnd())
        {
            return;
        }
        std::this_thread::sleep_for(kRetryInterval);
    }
    score::mw::log::LogFatal("lola") << "ReferenceTransactionBegin: Transaction-END bit remains TRUE after "
                                     << kRetryCount * kRetryInterval.count() << "ms; terminating";
    std::terminate();
}

bool DoesLogContainIncrementOrDecrementTransactions(
    const TransactionLogLocalView::TransactionLogSlotsLocalView& reference_count_slots) noexcept
{
    for (std::size_t slot_idx = 0U; slot_idx < reference_count_slots.size(); ++slot_idx)
    {
        const auto& slot = reference_count_slots[slot_idx];
        if (slot.GetTransactionBegin() || slot.GetTransactionEnd())
        {
            return true;
        }
    }
    return false;
}

}  // namespace

TransactionLogLocalView::TransactionLogLocalView(TransactionLog& transaction_log) noexcept
    : reference_count_slots_local_{transaction_log.reference_count_slots_.data(),
                                   transaction_log.reference_count_slots_.size()},
      subscribe_transactions_{transaction_log.subscribe_transactions_},
      subscription_max_sample_count_{transaction_log.subscription_max_sample_count_}
{
}

void TransactionLogLocalView::SubscribeTransactionBegin(const std::size_t subscription_max_sample_count) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(!subscribe_transactions_.get().GetTransactionBegin());
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(!subscribe_transactions_.get().GetTransactionEnd());
    subscribe_transactions_.get().SetTransactionBegin(true);
    subscription_max_sample_count_.get() = subscription_max_sample_count;
}

void TransactionLogLocalView::SubscribeTransactionCommit() noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(subscribe_transactions_.get().GetTransactionBegin());
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(!subscribe_transactions_.get().GetTransactionEnd());
    subscribe_transactions_.get().SetTransactionEnd(true);
}

void TransactionLogLocalView::SubscribeTransactionAbort() noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(subscribe_transactions_.get().GetTransactionBegin());
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(!subscribe_transactions_.get().GetTransactionEnd());
    subscribe_transactions_.get().SetTransactionBegin(false);
}

void TransactionLogLocalView::UnsubscribeTransactionBegin() noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(subscribe_transactions_.get().GetTransactionBegin());
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(subscribe_transactions_.get().GetTransactionEnd());
    subscribe_transactions_.get().SetTransactionBegin(false);
}

void TransactionLogLocalView::UnsubscribeTransactionCommit() noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(!subscribe_transactions_.get().GetTransactionBegin());
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(subscribe_transactions_.get().GetTransactionEnd());
    subscription_max_sample_count_.get().reset();
    subscribe_transactions_.get().SetTransactionEnd(false);
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from 'reference_count_slots_local_.at()' which might throw
// std::out_of_range As we already do an index check before accessing, so no way for throwing std::out_of_rang which
// leds to calling std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
void TransactionLogLocalView::ReferenceTransactionBegin(SlotIndexType slot_index) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(slot_index < reference_count_slots_local_.size());
    TransactionLogSlot& slot = reference_count_slots_local_[static_cast<std::size_t>(slot_index)];
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(!slot.GetTransactionBegin());
    WaitForTransactionEndToBecomeFalse(slot);
    slot.SetTransactionBegin(true);
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from 'reference_count_slots_local_.at()' which might throw
// std::out_of_range As we already do an index check before accessing, so no way for throwing std::out_of_rang which
// leds to calling std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
void TransactionLogLocalView::ReferenceTransactionCommit(SlotIndexType slot_index) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(slot_index < reference_count_slots_local_.size());
    TransactionLogSlot& slot = reference_count_slots_local_[static_cast<std::size_t>(slot_index)];
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(slot.GetTransactionBegin());
    WaitForTransactionEndToBecomeFalse(slot);
    slot.SetTransactionEnd(true);
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from 'reference_count_slots_local_.at()' which might throw
// std::out_of_range As we already do an index check before accessing, so no way for throwing std::out_of_rang which
// leds to calling std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
void TransactionLogLocalView::ReferenceTransactionAbort(SlotIndexType slot_index) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(slot_index < reference_count_slots_local_.size());
    TransactionLogSlot& slot = reference_count_slots_local_[static_cast<std::size_t>(slot_index)];
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(slot.GetTransactionBegin());
    WaitForTransactionEndToBecomeFalse(slot);
    slot.SetTransactionBegin(false);
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from 'reference_count_slots_local_.at()' which might throw
// std::out_of_range As we already do an index check before accessing, so no way for throwing std::out_of_rang which
// leds to calling std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
void TransactionLogLocalView::DereferenceTransactionBegin(SlotIndexType slot_index) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(slot_index < reference_count_slots_local_.size());
    TransactionLogSlot& slot = reference_count_slots_local_[static_cast<std::size_t>(slot_index)];
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(slot.GetTransactionBegin());
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(slot.GetTransactionEnd());
    slot.SetTransactionBegin(false);
}

void TransactionLogLocalView::DereferenceTransactionCommit(SlotIndexType slot_index) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(slot_index < reference_count_slots_local_.size());
    TransactionLogSlot& slot = reference_count_slots_local_[static_cast<std::size_t>(slot_index)];
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(!slot.GetTransactionBegin());
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(slot.GetTransactionEnd());
    slot.SetTransactionEnd(false);
}

Result<void> TransactionLogLocalView::RollbackProxyElementLog(const DereferenceSlotCallback& dereference_slot_callback,
                                                              const UnsubscribeCallback& unsubscribe_callback) noexcept
{
    const bool was_no_subscribe_recorded{!subscribe_transactions_.get().GetTransactionBegin() &&
                                         !subscribe_transactions_.get().GetTransactionEnd()};
    if (was_no_subscribe_recorded)
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_MESSAGE(
            !DoesLogContainIncrementOrDecrementTransactions(reference_count_slots_local_),
            "All slot increment transactions should be reversed before calling unsubscribe");
    }

    const auto rollback_increment_transactions_result = RollbackIncrementTransactions(dereference_slot_callback);
    if (!rollback_increment_transactions_result.has_value())
    {
        return rollback_increment_transactions_result;
    }

    const auto rollback_subscribe_transactions_result = RollbackSubscribeTransactions(unsubscribe_callback);
    return rollback_subscribe_transactions_result;
}

Result<void> TransactionLogLocalView::RollbackSkeletonTracingElementLog(
    const DereferenceSlotCallback& dereference_slot_callback) noexcept
{
    const auto rollback_increment_transactions_result = RollbackIncrementTransactions(dereference_slot_callback);
    return rollback_increment_transactions_result;
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from 'reference_count_slots_local_.at()' which might throw
// std::out_of_range As we already do an index check before accessing, so no way for throwing std::out_of_rang which
// leds to calling std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
Result<void> TransactionLogLocalView::RollbackIncrementTransactions(
    const DereferenceSlotCallback& dereference_slot_callback) noexcept
{
    for (SlotIndexType slot_idx = 0U; slot_idx < reference_count_slots_local_.size(); ++slot_idx)
    {
        TransactionLogSlot& slot = reference_count_slots_local_[static_cast<std::size_t>(slot_idx)];

        const bool was_slot_succesfully_incremented{slot.GetTransactionBegin() && slot.GetTransactionEnd()};
        const bool did_program_crash_while_incrementing_slot{slot.GetTransactionBegin() && !slot.GetTransactionEnd()};
        const bool did_program_crash_while_decrementing_slot{!slot.GetTransactionBegin() && slot.GetTransactionEnd()};
        if (was_slot_succesfully_incremented)
        {
            DereferenceTransactionBegin(slot_idx);
            dereference_slot_callback(slot_idx);
            DereferenceTransactionCommit(slot_idx);
        }
        else if (did_program_crash_while_incrementing_slot)
        {
            score::mw::log::LogError("lola")
                << "Could not rollback transaction log as previous service element crashed while "
                   "incrementing a control slot.";
            return MakeUnexpected(ComErrc::kCouldNotRestartProxy);
        }
        else if (did_program_crash_while_decrementing_slot)
        {
            score::mw::log::LogError("lola")
                << "Could not rollback transaction log as previous service element crashed while "
                   "decrementing a control slot.";
            return MakeUnexpected(ComErrc::kCouldNotRestartProxy);
        }
    }
    return {};
}

Result<void> TransactionLogLocalView::RollbackSubscribeTransactions(
    const UnsubscribeCallback& unsubscribe_callback) noexcept
{
    const bool was_subscribe_succesfully_recorded{subscribe_transactions_.get().GetTransactionBegin() &&
                                                  subscribe_transactions_.get().GetTransactionEnd()};
    const bool did_program_crash_while_subscribing{subscribe_transactions_.get().GetTransactionBegin() &&
                                                   !subscribe_transactions_.get().GetTransactionEnd()};
    const bool did_program_crash_while_unsubscribing{!subscribe_transactions_.get().GetTransactionBegin() &&
                                                     subscribe_transactions_.get().GetTransactionEnd()};
    if (was_subscribe_succesfully_recorded)
    {
        UnsubscribeTransactionBegin();
        unsubscribe_callback(subscription_max_sample_count_.get().value());
        UnsubscribeTransactionCommit();
    }
    else if (did_program_crash_while_subscribing)
    {
        score::mw::log::LogError("lola")
            << "Could not rollback transaction log as previous service element crashed while calling Subscribe.";
        return MakeUnexpected(ComErrc::kCouldNotRestartProxy);
    }
    else if (did_program_crash_while_unsubscribing)
    {
        score::mw::log::LogError("lola")
            << "Could not rollback transaction log as previous service element crashed while calling Unsubscribe.";
        return MakeUnexpected(ComErrc::kCouldNotRestartProxy);
    }
    return {};
}

bool TransactionLogLocalView::ContainsTransactions() const noexcept
{
    const bool contains_subscribe_transaction =
        subscribe_transactions_.get().GetTransactionBegin() || subscribe_transactions_.get().GetTransactionEnd();
    return contains_subscribe_transaction ||
           DoesLogContainIncrementOrDecrementTransactions(reference_count_slots_local_);
}

}  // namespace score::mw::com::impl::lola
