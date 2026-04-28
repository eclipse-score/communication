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
#include "score/mw/com/impl/bindings/lola/event_subscription_control.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_local_view.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_set.h"

namespace score::mw::com::impl::lola
{

std::uint32_t CreateEventSubscriptionControlState(EventSubscriptionControl<>::SubscriberCountType subscriber_count,
                                                  EventSubscriptionControl<>::SlotNumberType subscribed_slots)
{
    std::uint32_t result{subscriber_count};
    result = result << 16U;
    result += subscribed_slots;
    return result;
}

void AddSubscriptionToEventSubscriptionControl(EventSubscriptionControl<>& subscription_control,
                                               const EventSubscriptionControl<>::SubscriberCountType subscriber_count,
                                               const TransactionLog::MaxSampleCountType max_sample_count) noexcept
{
    const std::uint32_t current_subscription_state{
        CreateEventSubscriptionControlState(subscriber_count, max_sample_count)};
    EventSubscriptionControlAttorney<EventSubscriptionControl<>>{subscription_control}.SetCurrentState(
        current_subscription_state);
}

void InsertProxyTransactionLogWithValidTransactions(
    ConsumerEventDataControlLocalView<>& consumer_event_data_control_local,
    EventSubscriptionControl<>& subscription_control,
    TransactionLogSet& transaction_log_set,
    const TransactionLog::MaxSampleCountType subscription_max_sample_count,
    const TransactionLogId transaction_log_id) noexcept
{
    // We want to simulate that a previous process registered a TransactionLog, recorded some transactions and then
    // crashed. In this scenario, the destructor of the TransactionLogRegistrationGuard would never have been called
    // (which would have unregistered the TransactionLog). Therefore, we disable the destruction operation of the
    // TransactionLogRegistrationGuard in this function and destroy it immediately after inserting transactions into the
    // TransactionLog.
    TransactionLogRegistrationGuardDeactiveDestructionOperationGuard guard{};

    // Modify the SubscriptionControl so that it currently has a record of a single subscriber which subscribed with
    // a sample count of subscription_max_sample_count_
    const EventSubscriptionControl<>::SubscriberCountType subscriber_count{1U};
    AddSubscriptionToEventSubscriptionControl(subscription_control, subscriber_count, subscription_max_sample_count);

    auto transaction_registration_guard =
        transaction_log_set.RegisterProxyElement(transaction_log_id, consumer_event_data_control_local).value();
    const auto transaction_log_index = transaction_registration_guard.GetTransactionLogIndex();

    TransactionLogLocalView transaction_log_local_view = transaction_log_set.GetTransactionLog(transaction_log_index);
    transaction_log_local_view.SubscribeTransactionBegin(subscription_max_sample_count);
    transaction_log_local_view.SubscribeTransactionCommit();

    constexpr std::size_t slot_index{0U};
    transaction_log_local_view.ReferenceTransactionBegin(slot_index);
    transaction_log_local_view.ReferenceTransactionCommit(slot_index);
}

void InsertSkeletonTransactionLogWithValidTransactions(
    ConsumerEventDataControlLocalView<>& consumer_event_data_control_local,
    TransactionLogSet& transaction_log_set) noexcept
{
    // We want to simulate that a previous process registered a TransactionLog, recorded some transactions and then
    // crashed. In this scenario, the destructor of the TransactionLogRegistrationGuard would never have been called
    // (which would have unregistered the TransactionLog). Therefore, we disable the destruction operation of the
    // TransactionLogRegistrationGuard in this function and destroy it immediately after inserting transactions into the
    // TransactionLog.
    TransactionLogRegistrationGuardDeactiveDestructionOperationGuard guard{};

    auto transaction_registration_guard =
        transaction_log_set.RegisterSkeletonTracingElement(consumer_event_data_control_local);
    const auto transaction_log_index = transaction_registration_guard.GetTransactionLogIndex();

    TransactionLogLocalView transaction_log_local_view = transaction_log_set.GetTransactionLog(transaction_log_index);

    constexpr std::size_t slot_index{0U};
    transaction_log_local_view.ReferenceTransactionBegin(slot_index);
    transaction_log_local_view.ReferenceTransactionCommit(slot_index);
}

void InsertProxyTransactionLogWithInvalidTransactions(
    ConsumerEventDataControlLocalView<>& consumer_event_data_control_local,
    EventSubscriptionControl<>& subscription_control,
    TransactionLogSet& transaction_log_set,
    const TransactionLog::MaxSampleCountType subscription_max_sample_count,
    const TransactionLogId transaction_log_id) noexcept
{
    // We want to simulate that a previous process registered a TransactionLog, recorded some transactions and then
    // crashed. In this scenario, the destructor of the TransactionLogRegistrationGuard would never have been called
    // (which would have unregistered the TransactionLog). Therefore, we disable the destruction operation of the
    // TransactionLogRegistrationGuard in this function and destroy it immediately after inserting transactions into the
    // TransactionLog.
    TransactionLogRegistrationGuardDeactiveDestructionOperationGuard guard{};

    // Modify the SubscriptionControl so that it currently has a record of a single subscriber which subscribed with
    // a sample count of subscription_max_sample_count_
    const EventSubscriptionControl<>::SubscriberCountType subscriber_count{1U};
    AddSubscriptionToEventSubscriptionControl(subscription_control, subscriber_count, subscription_max_sample_count);

    auto transaction_registration_guard =
        transaction_log_set.RegisterProxyElement(transaction_log_id, consumer_event_data_control_local).value();
    const auto transaction_log_index = transaction_registration_guard.GetTransactionLogIndex();

    TransactionLogLocalView transaction_log_local_view = transaction_log_set.GetTransactionLog(transaction_log_index);
    transaction_log_local_view.SubscribeTransactionBegin(subscription_max_sample_count);
    transaction_log_local_view.SubscribeTransactionCommit();

    constexpr std::size_t slot_index{0U};
    transaction_log_local_view.ReferenceTransactionBegin(slot_index);
}

void InsertSkeletonTransactionLogWithInvalidTransactions(
    ConsumerEventDataControlLocalView<>& consumer_event_data_control_local,
    TransactionLogSet& transaction_log_set) noexcept
{
    // We want to simulate that a previous process registered a TransactionLog, recorded some transactions and then
    // crashed. In this scenario, the destructor of the TransactionLogRegistrationGuard would never have been called
    // (which would have unregistered the TransactionLog). Therefore, we disable the destruction operation of the
    // TransactionLogRegistrationGuard in this function and destroy it immediately after inserting transactions into the
    // TransactionLog.
    TransactionLogRegistrationGuardDeactiveDestructionOperationGuard guard{};

    auto transaction_registration_guard =
        transaction_log_set.RegisterSkeletonTracingElement(consumer_event_data_control_local);
    const auto transaction_log_index = transaction_registration_guard.GetTransactionLogIndex();

    TransactionLogLocalView transaction_log_local_view = transaction_log_set.GetTransactionLog(transaction_log_index);

    constexpr std::size_t slot_index{0U};
    transaction_log_local_view.ReferenceTransactionBegin(slot_index);
}

bool IsProxyTransactionLogIdRegistered(TransactionLogSet& transaction_log_set,
                                       const TransactionLogId& transaction_log_id) noexcept
{
    auto& transaction_logs = TransactionLogSetAttorney{transaction_log_set}.GetProxyTransactionLogs();
    const auto result =
        std::find_if(transaction_logs.begin(), transaction_logs.end(), [&transaction_log_id](const auto& element) {
            return (element.IsActive() && (element.GetTransactionLogId() == transaction_log_id));
        });
    return result != transaction_logs.cend();
}

bool IsSkeletonTransactionLogRegistered(TransactionLogSet& transaction_log_set) noexcept
{
    auto& transaction_log = TransactionLogSetAttorney{transaction_log_set}.GetSkeletonTransactionLog();
    return transaction_log.has_value();
}

}  // namespace score::mw::com::impl::lola
