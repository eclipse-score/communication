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
#include "score/mw/com/impl/bindings/lola/transaction_log_rollback_executor.h"

#include "score/mw/com/impl/bindings/lola/transaction_log_set.h"
#include "score/mw/com/impl/runtime.h"

#include "score/mw/log/logging.h"

#include <score/assert.hpp>
#include <exception>

namespace score::mw::com::impl::lola
{

namespace
{

// Suppress "AUTOSAR C++14 A15-5-3" rule findings.
// This rule states: "The std::terminate() function shall not be called implicitly".
// The coverity tool reports: "fun_call_w_exception: Called function throws an exception of type
// std::bad_optional_access." and points on statement `for (auto& element : service_data_control.event_controls_)`.
// This is a false positive; no optional is involved in this statement.
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
void MarkTransactionLogsNeedRollback(ServiceDataControl& service_data_control,
                                     const TransactionLogId transaction_log_id) noexcept
{
    for (auto& element : service_data_control.event_controls_)
    {
        auto& event_data_control = element.second.data_control;
        auto& transaction_log_set = event_data_control.GetTransactionLogSet();
        transaction_log_set.MarkTransactionLogsNeedRollback(transaction_log_id);
    }
}

}  // namespace

TransactionLogRollbackExecutor::TransactionLogRollbackExecutor(ServiceDataControl& service_data_control,
                                                               const QualityType asil_level,
                                                               pid_t provider_pid,
                                                               const TransactionLogId transaction_log_id) noexcept
    : service_data_control_{service_data_control},
      asil_level_{asil_level},
      provider_pid_{provider_pid},
      transaction_log_id_{transaction_log_id}
{
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, all results which are accessed with '.value()' that could implicitly call
// 'std::terminate()' (in case it doesn't have value) has a check in advance using '.has_value()', so no way for
// throwing std::bad_optional_access which leds to std::terminate(). This suppression should be removed after fixing
// [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
void TransactionLogRollbackExecutor::PrepareRollback(lola::IRuntime& lola_runtime) noexcept
{
    // Register the application's unique identifier (which is the transaction_log_id_ for this context) and
    // current pid in the shared mapping.
    const auto current_pid = lola_runtime.GetPid();
    const auto previous_pid = service_data_control_.application_id_pid_mapping_.RegisterPid(
        static_cast<std::uint32_t>(transaction_log_id_), current_pid);
    if (!(previous_pid.has_value()))
    {
        score::mw::log::LogFatal("lola")
            << "Couldn't Register current PID for UID within shared memory. This can occurr "
               "if there is too high contention accessing the registry. Terminating.";
        std::terminate();
    }

    if (previous_pid.value() != current_pid)
    {
        // we found an old/outdated PID for our UID in the shared-memory of the service-instance. Notify provider, that
        // this pid is outdated.
        lola_runtime.GetLolaMessaging().NotifyOutdatedNodeId(asil_level_, previous_pid.value(), provider_pid_);
    }

    // Mark all TransactionLogs for each event that correspond to transaction_log_id as needing to be rolled back.
    MarkTransactionLogsNeedRollback(service_data_control_, transaction_log_id_);
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, no way for implicit calling std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
ResultBlank TransactionLogRollbackExecutor::RollbackTransactionLogs() noexcept
{
    auto* const lola_runtime =
        dynamic_cast<lola::IRuntime*>(mw::com::impl::Runtime::getInstance().GetBindingRuntime(BindingType::kLoLa));
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(lola_runtime != nullptr, "Lola runtime does not exist!");
    auto& rollback_synchronization = lola_runtime->GetRollbackSynchronization();

    auto [lock, mutex_existed] = rollback_synchronization.GetMutex(&service_data_control_);
    // Suppress Autosar C++14 A8-5-3 states that auto variables shall not be initialized using braced initialization.
    // This is a false positive, we don't use auto here
    // coverity[autosar_cpp14_a8_5_3_violation : FALSE]
    std::lock_guard map_lock{lock};

    // If another Proxy instance has already prepared the rollback for the given service_data_control_ (the special case
    // where we have more than one proxy instance in the same process, which uses the same service instance ->
    // service_data_control_), then we must not prepare rollback a 2nd time! If the mutex was not created (but an
    // existing one returned), this is the sign, that another proxy already did the PrepareRollback for the same
    // service_data_control_
    if (!mutex_existed)
    {
        PrepareRollback(*lola_runtime);
    }

    for (auto& element : service_data_control_.event_controls_)
    {
        auto& event_control = element.second;
        auto& transaction_log_set = event_control.data_control.GetTransactionLogSet();
        const auto rollback_result = transaction_log_set.RollbackProxyTransactions(
            transaction_log_id_,
            [&event_control](const TransactionLog::SlotIndexType slot_index) noexcept {
                event_control.data_control.DereferenceEventWithoutTransactionLogging(slot_index);
            },
            [&event_control](const TransactionLog::MaxSampleCountType subscription_max_sample_count) noexcept {
                event_control.subscription_control.Unsubscribe(subscription_max_sample_count);
            });
        if (!rollback_result.has_value())
        {
            return rollback_result;
        }
    }
    return {};
}

}  // namespace score::mw::com::impl::lola
