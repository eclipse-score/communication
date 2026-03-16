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
#include "score/mw/com/impl/bindings/lola/skeleton_event_common.h"
#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/event_data_control_composite.h"
#include "score/mw/com/impl/bindings/lola/i_runtime.h"
#include "score/mw/com/impl/bindings/lola/messaging/i_message_passing_service.h"
#include "score/mw/com/impl/bindings/lola/skeleton.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_set.h"
#include "score/mw/com/impl/tracing/skeleton_event_tracing_data.h"

namespace score::mw::com::impl::lola
{

SkeletonEventCommon::SkeletonEventCommon(Skeleton& parent,
                                         const ElementFqId& event_fqn,
                                         impl::tracing::SkeletonEventTracingData tracing_data,
                                         SkeletonEventProperties event_properties) noexcept
    : parent_{parent},
      event_fqn_{event_fqn},
      qm_disconnect_{false},
      tracing_data_{tracing_data},
      event_properties_{event_properties},
      transaction_log_registration_guard_{}
{
}

void SkeletonEventCommon::PrepareOfferCommon(TransactionLogSet& transaction_log_set,
                                             EventDataControlComposite<>& event_data_control_composite_ref) noexcept
{
    event_data_control_composite_ = event_data_control_composite_ref;
    const bool tracing_globally_enabled = ((impl::Runtime::getInstance().GetTracingRuntime() != nullptr) &&
                                           (impl::Runtime::getInstance().GetTracingRuntime()->IsTracingEnabled()));
    if (!tracing_globally_enabled)
    {
        DisableAllTracePoints(tracing_data_);
    }

    const bool tracing_for_skeleton_event_enabled =
        tracing_data_.enable_send || tracing_data_.enable_send_with_allocate;
    // LCOV_EXCL_BR_START (Tool incorrectly marks the decision as "Decision couldn't be analyzed" despite all lines in
    // both branches (true / false) being covered. "Decision couldn't be analyzed" only appeared after changing the code
    // within the if statement (without changing the condition / tests). Suppression can be removed when bug is fixed in
    // Ticket-188259).
    if (tracing_for_skeleton_event_enabled)
    {
        EmplaceTransactionLogRegistrationGuard(transaction_log_set);
        EmplaceTypeErasedSamplePtrsGuard();
    }

    // Register callbacks to be notified when event notification existence changes.
    // This allows us to optimise the Send() path by skipping NotifyEvent() when no handlers are registered.
    // Separate callbacks for QM and ASIL-B update their respective atomic flags for lock-free access.
    GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa)
        .GetLolaMessaging()
        .RegisterEventNotificationExistenceChangedCallback(
            QualityType::kASIL_QM, event_fqn_, [this](const bool has_handlers) noexcept {
                SetQmNotificationsRegistered(has_handlers);
            });

    if (parent_.GetInstanceQualityType() == QualityType::kASIL_B)
    {
        GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa)
            .GetLolaMessaging()
            .RegisterEventNotificationExistenceChangedCallback(
                QualityType::kASIL_B, event_fqn_, [this](const bool has_handlers) noexcept {
                    SetAsilBNotificationsRegistered(has_handlers);
                });
    }
}

void SkeletonEventCommon::PrepareStopOfferCommon() noexcept
{
    // Unregister event notification existence changed callbacks
    GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa)
        .GetLolaMessaging()
        .UnregisterEventNotificationExistenceChangedCallback(QualityType::kASIL_QM, event_fqn_);

    if (parent_.GetInstanceQualityType() == QualityType::kASIL_B)
    {
        GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa)
            .GetLolaMessaging()
            .UnregisterEventNotificationExistenceChangedCallback(QualityType::kASIL_B, event_fqn_);
    }

    // Reset the flags to indicate no handlers are registered
    SetQmNotificationsRegistered(false);
    SetAsilBNotificationsRegistered(false);

    ResetGuards();
    event_data_control_composite_.reset();
}

void SkeletonEventCommon::EmplaceTransactionLogRegistrationGuard(TransactionLogSet& transaction_log_set)
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(event_data_control_composite_.has_value(),
                                                "EventDataControlComposite must be initialized.");
    score::cpp::ignore = transaction_log_registration_guard_.emplace(transaction_log_set.RegisterSkeletonTracingElement(
        event_data_control_composite_->GetConsumerEventDataControlLocalView()));
}

void SkeletonEventCommon::EmplaceTypeErasedSamplePtrsGuard()
{
    score::cpp::ignore = type_erased_sample_ptrs_guard_.emplace(tracing_data_.service_element_tracing_data);
}

void SkeletonEventCommon::SetQmNotificationsRegistered(bool value)
{
    qm_event_update_notifications_registered_.store(value);
}

void SkeletonEventCommon::SetAsilBNotificationsRegistered(bool value)
{
    asil_b_event_update_notifications_registered_.store(value);
}

void SkeletonEventCommon::ResetGuards() noexcept
{
    type_erased_sample_ptrs_guard_.reset();
    if (event_data_control_composite_.has_value())
    {
        transaction_log_registration_guard_.reset();
    }
}

bool SkeletonEventCommon::IsQmNotificationsRegistered() const noexcept
{

    return qm_event_update_notifications_registered_.load();
}

bool SkeletonEventCommon::IsAsilBNotificationsRegistered() const noexcept
{

    return asil_b_event_update_notifications_registered_.load();
}

}  // namespace score::mw::com::impl::lola
