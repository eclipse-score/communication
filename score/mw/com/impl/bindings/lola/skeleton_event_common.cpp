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
#include "score/mw/com/impl/bindings/lola/i_runtime.h" // For GetBindingRuntime
#include "score/mw/com/impl/bindings/lola/messaging/i_message_passing_service.h" // For RegisterEventNotificationExistenceChangedCallback
#include "score/mw/com/impl/bindings/lola/skeleton.h"

namespace score::mw::com::impl::lola
{

SkeletonEventCommon::SkeletonEventCommon(Skeleton& parent,
                                         const ElementFqId& event_fqn,
                                         score::cpp::optional<EventDataControlComposite>& event_data_control_composite_ref,
                                         EventSlotStatus::EventTimeStamp& current_timestamp_ref,
                                         impl::tracing::SkeletonEventTracingData tracing_data) noexcept
    : parent_{parent},
      event_fqn_{event_fqn},
      event_data_control_composite_ref_{event_data_control_composite_ref},
      current_timestamp_ref_{current_timestamp_ref},
      tracing_data_{tracing_data}
{
}

void SkeletonEventCommon::PrepareOfferCommon() noexcept
{
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
        EmplaceTransactionLogRegistrationGuard();
        EmplaceTypeErasedSamplePtrsGuard();
    }

    UpdateCurrentTimestamp();

    // Register callbacks to be notified when event notification existence changes.
    // This allows us to optimise the Send() path by skipping NotifyEvent() when no handlers are registered.
    // Separate callbacks for QM and ASIL-B update their respective atomic flags for lock-free access.
    if (parent_.GetInstanceQualityType() == QualityType::kASIL_QM)
    {
        GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa)
            .GetLolaMessaging()
            .RegisterEventNotificationExistenceChangedCallback(
                QualityType::kASIL_QM, event_fqn_, [this](const bool has_handlers) noexcept {
                    SetQmNotificationsRegistered(has_handlers);
                });
    }
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
}

void SkeletonEventCommon::EmplaceTransactionLogRegistrationGuard()
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(event_data_control_composite_ref_.has_value(), "EventDataControlComposite must be initialized.");
    score::cpp::ignore = transaction_log_registration_guard_.emplace(
        TransactionLogRegistrationGuard::Create(event_data_control_composite_ref_.value().GetQmEventDataControl()));
}

void SkeletonEventCommon::EmplaceTypeErasedSamplePtrsGuard()
{
    score::cpp::ignore = type_erased_sample_ptrs_guard_.emplace(tracing_data_.service_element_tracing_data);
}

void SkeletonEventCommon::UpdateCurrentTimestamp()
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(event_data_control_composite_ref_.has_value(), "EventDataControlComposite must be initialized.");
    current_timestamp_ref_ = event_data_control_composite_ref_.value().GetLatestTimestamp();
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
    if (event_data_control_composite_ref_.has_value())
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

} // namespace score::mw::com::impl::lola
