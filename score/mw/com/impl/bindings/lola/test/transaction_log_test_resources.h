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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_TEST_TRANSACTION_LOG_TEST_RESOURCES_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_TEST_TRANSACTION_LOG_TEST_RESOURCES_H

#include "score/mw/com/impl/bindings/lola/test/proxy_event_test_resources.h"
#include "score/mw/com/impl/bindings/lola/transaction_log.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_id.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_rollback_executor.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_set.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <functional>

namespace score::mw::com::impl::lola
{

class TransactionLogSetAttorney
{
  public:
    TransactionLogSetAttorney(TransactionLogSet& transaction_log_set) : transaction_log_set_{transaction_log_set} {}

    TransactionLogSet::TransactionLogCollection& GetProxyTransactionLogs() noexcept
    {
        return transaction_log_set_.proxy_transaction_logs_;
    }

    const TransactionLogSet::TransactionLogCollection& GetProxyTransactionLogs() const noexcept
    {
        return transaction_log_set_.proxy_transaction_logs_;
    }

    const score::cpp::optional<std::reference_wrapper<TransactionLog>> GetSkeletonTransactionLog() noexcept
    {
        auto& skeleton_tracing_transaction_log = transaction_log_set_.skeleton_tracing_transaction_log_;
        if (!skeleton_tracing_transaction_log.IsActive())
        {
            return {};
        }
        return {skeleton_tracing_transaction_log.GetTransactionLog()};
    }

  private:
    TransactionLogSet& transaction_log_set_;
};

class TransactionLogAttorney
{
  public:
    TransactionLogAttorney(TransactionLog& transaction_log) noexcept : transaction_log_{transaction_log} {}

    TransactionLogSlot& GetReferenceCountSlot(const TransactionLog::SlotIndexType slot_index) noexcept
    {
        return transaction_log_.reference_count_slots_.at(static_cast<std::size_t>(slot_index));
    }

    bool IsSubscribeTransactionSuccesfullyRecorded() noexcept
    {
        return (transaction_log_.subscribe_transactions_.GetTransactionBegin() &&
                transaction_log_.subscribe_transactions_.GetTransactionEnd());
    }

  private:
    TransactionLog& transaction_log_;
};

class TransactionLogSetHelperFixture : public ::testing::Test
{
  protected:
    void ExpectTransactionLogSetEmpty(TransactionLogSet& transaction_log_set) noexcept
    {
        const auto& transaction_logs = TransactionLogSetAttorney{transaction_log_set}.GetProxyTransactionLogs();
        for (std::size_t i = 0; i < transaction_logs.size(); ++i)
        {
            const auto& transaction_log_element = transaction_logs[i];
            EXPECT_FALSE(transaction_log_element.IsActive());
        }
    }

    void ExpectProxyTransactionLogExistsAtIndex(TransactionLogSet& transaction_log_set,
                                                const TransactionLogId& transaction_log_id,
                                                const TransactionLogSet::TransactionLogIndex transaction_log_index,
                                                const bool expect_needs_rollback,
                                                const bool expect_other_slots_empty = true) noexcept
    {
        const auto& transaction_logs = TransactionLogSetAttorney{transaction_log_set}.GetProxyTransactionLogs();
        for (std::size_t i = 0; i < transaction_logs.size(); ++i)
        {
            const auto& transaction_log_element = transaction_logs[i];
            if (i == transaction_log_index)
            {
                ASSERT_TRUE(transaction_log_element.IsActive());
                ASSERT_TRUE(transaction_log_element.NeedsRollback() == expect_needs_rollback);
                EXPECT_EQ(transaction_log_id, transaction_log_element.GetTransactionLogId());
            }
            else if (expect_other_slots_empty)
            {
                EXPECT_FALSE(transaction_log_element.IsActive());
            }
        }
    }
};

std::uint32_t CreateEventSubscriptionControlState(EventSubscriptionControl::SubscriberCountType subscriber_count,
                                                  EventSubscriptionControl::SlotNumberType subscribed_slots);
void AddSubscriptionToEventSubscriptionControl(EventControl& event_control,
                                               const EventSubscriptionControl::SubscriberCountType subscriber_count,
                                               const TransactionLog::MaxSampleCountType max_sample_count) noexcept;
void InsertProxyTransactionLogWithValidTransactions(
    EventControl& event_control,
    const TransactionLog::MaxSampleCountType subscription_max_sample_count,
    const TransactionLogId transaction_log_id) noexcept;
void InsertSkeletonTransactionLogWithValidTransactions(EventDataControl& event_data_control) noexcept;

void InsertProxyTransactionLogWithInvalidTransactions(
    EventControl& event_control,
    const TransactionLog::MaxSampleCountType subscription_max_sample_count,
    const TransactionLogId transaction_log_id) noexcept;
void InsertSkeletonTransactionLogWithInvalidTransactions(EventDataControl& event_data_control) noexcept;

bool IsProxyTransactionLogIdRegistered(EventControl& event_control,
                                       const TransactionLogId& transaction_log_id) noexcept;
bool IsSkeletonTransactionLogRegistered(EventDataControl& event_data_control) noexcept;
bool DoesSkeletonTransactionLogContainTransactions(EventDataControl& event_data_control) noexcept;

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_TEST_TRANSACTION_LOG_TEST_RESOURCES_H
