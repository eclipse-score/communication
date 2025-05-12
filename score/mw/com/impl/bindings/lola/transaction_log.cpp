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
#include "score/mw/com/impl/bindings/lola/transaction_log.h"

#include "score/mw/com/impl/com_error.h"

#include "score/mw/log/logging.h"

namespace score::mw::com::impl::lola
{

namespace
{

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from 'reference_count_slots.at()' which might throw
// std::out_of_range As we already do an index check before accessing, so no way for throwing std::out_of_rang which
// leds to calling std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
bool DoesLogContainIncrementOrDecrementTransactions(
    const TransactionLog::TransactionLogSlots& reference_count_slots) noexcept
{
    for (std::size_t slot_idx = 0U; slot_idx < reference_count_slots.size(); ++slot_idx)
    {
        const auto& slot = reference_count_slots.at(slot_idx);
        if (slot.GetTransactionBegin() || slot.GetTransactionEnd())
        {
            return true;
        }
    }
    return false;
}

}  // namespace

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". The std::vector constructor we use is not marked as noexcept, in case TransactionLogNode is not
// DefaultInsertable into std::vector<T>, the behavior is undefined. As TransactionLogNode is DefaultInsertable so no
// way for throwing an exception which leds to calling std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
TransactionLog::TransactionLog(std::size_t number_of_slots, const memory::shared::MemoryResourceProxy* proxy) noexcept
    : reference_count_slots_(number_of_slots, proxy), subscribe_transactions_{}, subscription_max_sample_count_{}
{
}

void TransactionLog::SubscribeTransactionBegin(const std::size_t subscription_max_sample_count) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(!subscribe_transactions_.GetTransactionBegin());
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(!subscribe_transactions_.GetTransactionEnd());
    subscribe_transactions_.SetTransactionBegin(true);
    subscription_max_sample_count_ = subscription_max_sample_count;
}

void TransactionLog::SubscribeTransactionCommit() noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(subscribe_transactions_.GetTransactionBegin());
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(!subscribe_transactions_.GetTransactionEnd());
    subscribe_transactions_.SetTransactionEnd(true);
}

void TransactionLog::SubscribeTransactionAbort() noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(subscribe_transactions_.GetTransactionBegin());
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(!subscribe_transactions_.GetTransactionEnd());
    subscribe_transactions_.SetTransactionBegin(false);
}

void TransactionLog::UnsubscribeTransactionBegin() noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(subscribe_transactions_.GetTransactionBegin());
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(subscribe_transactions_.GetTransactionEnd());
    subscribe_transactions_.SetTransactionBegin(false);
}

void TransactionLog::UnsubscribeTransactionCommit() noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(!subscribe_transactions_.GetTransactionBegin());
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(subscribe_transactions_.GetTransactionEnd());
    subscription_max_sample_count_.reset();
    subscribe_transactions_.SetTransactionEnd(false);
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from 'reference_count_slots_.at()' which might throw
// std::out_of_range As we already do an index check before accessing, so no way for throwing std::out_of_rang which
// leds to calling std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
void TransactionLog::ReferenceTransactionBegin(SlotIndexType slot_index) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(static_cast<std::size_t>(slot_index) < reference_count_slots_.size());
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(!reference_count_slots_.at(static_cast<std::size_t>(slot_index)).GetTransactionBegin());
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(!reference_count_slots_.at(static_cast<std::size_t>(slot_index)).GetTransactionEnd());
    reference_count_slots_.at(static_cast<std::size_t>(slot_index)).SetTransactionBegin(true);
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from 'reference_count_slots_.at()' which might throw
// std::out_of_range As we already do an index check before accessing, so no way for throwing std::out_of_rang which
// leds to calling std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
void TransactionLog::ReferenceTransactionCommit(SlotIndexType slot_index) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(static_cast<std::size_t>(slot_index) < reference_count_slots_.size());
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(reference_count_slots_.at(static_cast<std::size_t>(slot_index)).GetTransactionBegin());
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(!reference_count_slots_.at(static_cast<std::size_t>(slot_index)).GetTransactionEnd());
    reference_count_slots_.at(static_cast<std::size_t>(slot_index)).SetTransactionEnd(true);
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from 'reference_count_slots_.at()' which might throw
// std::out_of_range As we already do an index check before accessing, so no way for throwing std::out_of_rang which
// leds to calling std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
void TransactionLog::ReferenceTransactionAbort(SlotIndexType slot_index) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(static_cast<std::size_t>(slot_index) < reference_count_slots_.size());
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(reference_count_slots_.at(static_cast<std::size_t>(slot_index)).GetTransactionBegin());
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(!reference_count_slots_.at(static_cast<std::size_t>(slot_index)).GetTransactionEnd());
    reference_count_slots_.at(static_cast<std::size_t>(slot_index)).SetTransactionBegin(false);
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from 'reference_count_slots_.at()' which might throw
// std::out_of_range As we already do an index check before accessing, so no way for throwing std::out_of_rang which
// leds to calling std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
void TransactionLog::DereferenceTransactionBegin(SlotIndexType slot_index) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(static_cast<std::size_t>(slot_index) < reference_count_slots_.size());
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(reference_count_slots_.at(static_cast<std::size_t>(slot_index)).GetTransactionBegin());
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(reference_count_slots_.at(static_cast<std::size_t>(slot_index)).GetTransactionEnd());
    reference_count_slots_.at(static_cast<std::size_t>(slot_index)).SetTransactionBegin(false);
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from 'reference_count_slots_.at()' which might throw
// std::out_of_range We added an index check before accessing, so if the index is out of range the precondition will
// fail.
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
void TransactionLog::DereferenceTransactionCommit(SlotIndexType slot_index) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(static_cast<std::size_t>(slot_index) < reference_count_slots_.size());
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(!reference_count_slots_.at(static_cast<std::size_t>(slot_index)).GetTransactionBegin());
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(reference_count_slots_.at(static_cast<std::size_t>(slot_index)).GetTransactionEnd());
    reference_count_slots_.at(static_cast<std::size_t>(slot_index)).SetTransactionEnd(false);
}

ResultBlank TransactionLog::RollbackProxyElementLog(const DereferenceSlotCallback& dereference_slot_callback,
                                                    const UnsubscribeCallback& unsubscribe_callback) noexcept
{
    const bool was_no_subscribe_recorded{!subscribe_transactions_.GetTransactionBegin() &&
                                         !subscribe_transactions_.GetTransactionEnd()};
    if (was_no_subscribe_recorded)
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_MESSAGE(!DoesLogContainIncrementOrDecrementTransactions(reference_count_slots_),
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

ResultBlank TransactionLog::RollbackSkeletonTracingElementLog(
    const DereferenceSlotCallback& dereference_slot_callback) noexcept
{
    const auto rollback_increment_transactions_result = RollbackIncrementTransactions(dereference_slot_callback);
    return rollback_increment_transactions_result;
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from 'reference_count_slots_.at()' which might throw
// std::out_of_range As we already do an index check before accessing, so no way for throwing std::out_of_rang which
// leds to calling std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
ResultBlank TransactionLog::RollbackIncrementTransactions(
    const DereferenceSlotCallback& dereference_slot_callback) noexcept
{
    for (SlotIndexType slot_idx = 0U; slot_idx < reference_count_slots_.size(); ++slot_idx)
    {
        auto& slot = reference_count_slots_.at(static_cast<std::size_t>(slot_idx));

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

ResultBlank TransactionLog::RollbackSubscribeTransactions(const UnsubscribeCallback& unsubscribe_callback) noexcept
{
    const bool was_subscribe_succesfully_recorded{subscribe_transactions_.GetTransactionBegin() &&
                                                  subscribe_transactions_.GetTransactionEnd()};
    const bool did_program_crash_while_subscribing{subscribe_transactions_.GetTransactionBegin() &&
                                                   !subscribe_transactions_.GetTransactionEnd()};
    const bool did_program_crash_while_unsubscribing{!subscribe_transactions_.GetTransactionBegin() &&
                                                     subscribe_transactions_.GetTransactionEnd()};
    if (was_subscribe_succesfully_recorded)
    {
        UnsubscribeTransactionBegin();
        unsubscribe_callback(subscription_max_sample_count_.value());
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

bool TransactionLog::ContainsTransactions() const noexcept
{
    const bool contains_subscribe_transaction =
        subscribe_transactions_.GetTransactionBegin() || subscribe_transactions_.GetTransactionEnd();
    return contains_subscribe_transaction || DoesLogContainIncrementOrDecrementTransactions(reference_count_slots_);
}

}  // namespace score::mw::com::impl::lola
