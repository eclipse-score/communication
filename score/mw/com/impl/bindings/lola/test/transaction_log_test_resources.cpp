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
#include "score/mw/com/impl/bindings/lola/test/transaction_log_test_resources.h"

namespace score::mw::com::impl::lola
{

std::uint32_t CreateEventSubscriptionControlState(EventSubscriptionControl::SubscriberCountType subscriber_count,
                                                  EventSubscriptionControl::SlotNumberType subscribed_slots)
{
    std::uint32_t result{subscriber_count};
    result = result << 16U;
    result += subscribed_slots;
    return result;
}

void AddSubscriptionToEventSubscriptionControl(ProxyEventControlLocalView& proxy_event_control_local,
                                               const EventSubscriptionControl::SubscriberCountType subscriber_count,
                                               const TransactionLog::MaxSampleCountType max_sample_count) noexcept
{
    const std::uint32_t current_subscription_state{
        CreateEventSubscriptionControlState(subscriber_count, max_sample_count)};
    EventSubscriptionControlAttorney<EventSubscriptionControl>{proxy_event_control_local.subscription_control}
        .SetCurrentState(current_subscription_state);
}

void InsertProxyTransactionLogWithValidTransactions(
    ProxyEventControlLocalView& proxy_event_control_local,
    const TransactionLog::MaxSampleCountType subscription_max_sample_count,
    const TransactionLogId transaction_log_id) noexcept
{
    // Modify the SubscriptionControl so that it currently has a record of a single subscriber which subscribed with
    // a sample count of subscription_max_sample_count_
    const EventSubscriptionControl::SubscriberCountType subscriber_count{1U};
    AddSubscriptionToEventSubscriptionControl(
        proxy_event_control_local, subscriber_count, subscription_max_sample_count);

    auto& transaction_log_set = proxy_event_control_local.data_control.GetTransactionLogSet();
    const auto transaction_log_index = transaction_log_set.RegisterProxyElement(transaction_log_id).value();

    auto& transaction_log = transaction_log_set.GetTransactionLog(transaction_log_index);
    transaction_log.SubscribeTransactionBegin(subscription_max_sample_count);
    transaction_log.SubscribeTransactionCommit();

    constexpr std::size_t slot_index{0U};
    transaction_log.ReferenceTransactionBegin(slot_index);
    transaction_log.ReferenceTransactionCommit(slot_index);
}

void InsertSkeletonTransactionLogWithValidTransactions(
    SkeletonEventDataControlLocalView<>& skeleton_event_data_control_local) noexcept
{
    auto& transaction_log_set = skeleton_event_data_control_local.GetTransactionLogSet();
    const auto transaction_log_index = transaction_log_set.RegisterSkeletonTracingElement();

    auto& transaction_log = transaction_log_set.GetTransactionLog(transaction_log_index);

    constexpr std::size_t slot_index{0U};
    transaction_log.ReferenceTransactionBegin(slot_index);
    transaction_log.ReferenceTransactionCommit(slot_index);
}

void InsertProxyTransactionLogWithInvalidTransactions(
    ProxyEventControlLocalView& proxy_event_control_local,
    const TransactionLog::MaxSampleCountType subscription_max_sample_count,
    const TransactionLogId transaction_log_id) noexcept
{
    // Modify the SubscriptionControl so that it currently has a record of a single subscriber which subscribed with
    // a sample count of subscription_max_sample_count_
    const EventSubscriptionControl::SubscriberCountType subscriber_count{1U};
    AddSubscriptionToEventSubscriptionControl(
        proxy_event_control_local, subscriber_count, subscription_max_sample_count);

    auto& transaction_log_set = proxy_event_control_local.data_control.GetTransactionLogSet();
    const auto transaction_log_index = transaction_log_set.RegisterProxyElement(transaction_log_id).value();

    auto& transaction_log = transaction_log_set.GetTransactionLog(transaction_log_index);
    transaction_log.SubscribeTransactionBegin(subscription_max_sample_count);
    transaction_log.SubscribeTransactionCommit();

    constexpr std::size_t slot_index{0U};
    transaction_log.ReferenceTransactionBegin(slot_index);
}

void InsertSkeletonTransactionLogWithInvalidTransactions(
    SkeletonEventDataControlLocalView<>& skeleton_event_data_control_local) noexcept
{
    auto& transaction_log_set = skeleton_event_data_control_local.GetTransactionLogSet();
    const auto transaction_log_index = transaction_log_set.RegisterSkeletonTracingElement();

    auto& transaction_log = transaction_log_set.GetTransactionLog(transaction_log_index);

    constexpr std::size_t slot_index{0U};
    transaction_log.ReferenceTransactionBegin(slot_index);
}

bool IsProxyTransactionLogIdRegistered(ProxyEventControlLocalView& proxy_event_control_local,
                                       const TransactionLogId& transaction_log_id) noexcept
{
    auto& transaction_log_set = proxy_event_control_local.data_control.GetTransactionLogSet();
    auto& transaction_logs = TransactionLogSetAttorney{transaction_log_set}.GetProxyTransactionLogs();
    const auto result =
        std::find_if(transaction_logs.begin(), transaction_logs.end(), [&transaction_log_id](const auto& element) {
            return (element.IsActive() && (element.GetTransactionLogId() == transaction_log_id));
        });
    return result != transaction_logs.cend();
}

bool IsSkeletonTransactionLogRegistered(SkeletonEventDataControlLocalView<>& skeleton_event_data_control_local) noexcept
{
    auto& transaction_log_set = skeleton_event_data_control_local.GetTransactionLogSet();
    auto& transaction_log = TransactionLogSetAttorney{transaction_log_set}.GetSkeletonTransactionLog();
    return transaction_log.has_value();
}

}  // namespace score::mw::com::impl::lola
