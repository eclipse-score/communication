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

#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_EVENT_COMMON_H_
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_EVENT_COMMON_H_

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/i_runtime.h"
#include "score/mw/com/impl/bindings/lola/messaging/i_message_passing_service.h"
#include "score/mw/com/impl/bindings/lola/skeleton.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event_properties.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_registration_guard.h"
#include "score/mw/com/impl/bindings/lola/type_erased_sample_ptrs_guard.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/impl/tracing/skeleton_event_tracing_data.h"

#include <score/assert.hpp>
#include <score/optional.hpp>
#include <score/utility.hpp>

#include <atomic>
#include <optional>

namespace score::mw::com::impl::lola
{

template <typename SampleType>
class SkeletonEventAttorney;

class Skeleton;

/// @brief Common implementation for LoLa skeleton events, shared between SkeletonEvent and GenericSkeletonEvent.
///
/// \tparam SampleType Will be the SampleType of the event for a regular SkeletonEvent and void for a
/// GenericSkeletonEventr.
template <typename SampleType>
class SkeletonEventCommon
{
    // Grant friendship to allow access to private helpers
    // The "SkeletonEventAttorney" class is a helper, which sets the internal state of "SkeletonEventCommon" accessing
    // private members and used for testing purposes only.
    friend class SkeletonEventAttorney<SampleType>;

  public:
    SkeletonEventCommon(Skeleton& parent,
                        const SkeletonEventProperties& event_properties,
                        const ElementFqId& event_fqn,
                        std::optional<EventDataControlComposite<>>& event_data_control_composite_ref,
                        EventSlotStatus::EventTimeStamp& current_timestamp_ref,
                        impl::tracing::SkeletonEventTracingData tracing_data = {}) noexcept;

    SkeletonEventCommon(const SkeletonEventCommon&) = delete;
    SkeletonEventCommon(SkeletonEventCommon&&) noexcept = delete;
    SkeletonEventCommon& operator=(const SkeletonEventCommon&) & = delete;
    SkeletonEventCommon& operator=(SkeletonEventCommon&&) & noexcept = delete;

    ~SkeletonEventCommon() = default;

    void PrepareOfferCommon() noexcept;
    void PrepareStopOfferCommon() noexcept;

    // Accessors for members used by PrepareOfferCommon/PrepareStopOfferCommon
    impl::tracing::SkeletonEventTracingData& GetTracingData()
    {
        return tracing_data_;
    }
    const ElementFqId& GetElementFQId() const
    {
        return event_fqn_;
    }
    Skeleton& GetParent()
    {
        return parent_;
    }

    [[nodiscard]] const SkeletonEventProperties& GetEventProperties() const
    {
        return event_properties_;
    }

    // Accessors for atomic flags for derived classes' Send() method
    bool IsQmNotificationsRegistered() const noexcept;
    bool IsAsilBNotificationsRegistered() const noexcept;

  private:
    Skeleton& parent_;
    SkeletonEventProperties event_properties_;
    ElementFqId event_fqn_;
    std::optional<EventDataControlComposite<>>&
        event_data_control_composite_ref_;                    // Reference to the optional in derived class
    EventSlotStatus::EventTimeStamp& current_timestamp_ref_;  // Reference to the timestamp in derived class
    impl::tracing::SkeletonEventTracingData tracing_data_;

    /// \brief Atomic flags indicating whether any receive handlers are currently registered for this event
    ///        at each quality level (QM and ASIL-B).
    /// \details These flags are updated via callbacks from MessagePassingServiceInstance when handler
    ///          registration status changes. They allow Send() to skip the NotifyEvent() call when no
    ///          handlers are registered for a specific quality level, avoiding unnecessary lock overhead
    ///          in the main path. Uses memory_order_relaxed as the flags are optimisation hints - false
    ///          positives (thinking handlers exist when they don't) are harmless, and false negatives
    ///          (missing handlers) are prevented by the callback mechanism.
    std::atomic<bool> qm_event_update_notifications_registered_{false};
    std::atomic<bool> asil_b_event_update_notifications_registered_{false};

    /// \brief optional RAII guards for tracing transaction log registration/un-registration and cleanup of "pending"
    /// type erased sample pointers which are created in PrepareOfferCommon() and destroyed in PrepareStopOfferCommon()
    /// - optional as only needed when tracing is enabled and when they haven't been cleaned up via a call to
    /// PrepareStopOfferCommon().
    std::optional<TransactionLogRegistrationGuard> transaction_log_registration_guard_{};
    std::optional<tracing::TypeErasedSamplePtrsGuard> type_erased_sample_ptrs_guard_{};

    void EmplaceTransactionLogRegistrationGuard();
    void EmplaceTypeErasedSamplePtrsGuard();
    void UpdateCurrentTimestamp();
    void SetQmNotificationsRegistered(bool value);
    void SetAsilBNotificationsRegistered(bool value);
    void ResetGuards() noexcept;
};

template <typename SampleType>
SkeletonEventCommon<SampleType>::SkeletonEventCommon(
    Skeleton& parent,
    const SkeletonEventProperties& event_properties,
    const ElementFqId& event_fqn,
    std::optional<EventDataControlComposite<>>& event_data_control_composite_ref,
    EventSlotStatus::EventTimeStamp& current_timestamp_ref,
    impl::tracing::SkeletonEventTracingData tracing_data) noexcept
    : parent_{parent},
      event_properties_{event_properties},
      event_fqn_{event_fqn},
      event_data_control_composite_ref_{event_data_control_composite_ref},
      current_timestamp_ref_{current_timestamp_ref},
      tracing_data_{tracing_data}
{
}

template <typename SampleType>
void SkeletonEventCommon<SampleType>::PrepareOfferCommon() noexcept
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

template <typename SampleType>
void SkeletonEventCommon<SampleType>::PrepareStopOfferCommon() noexcept
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

template <typename SampleType>
void SkeletonEventCommon<SampleType>::EmplaceTransactionLogRegistrationGuard()
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(event_data_control_composite_ref_.has_value(),
                                                "EventDataControlComposite must be initialized.");
    score::cpp::ignore = transaction_log_registration_guard_.emplace(TransactionLogRegistrationGuard::Create(
        event_data_control_composite_ref_.value().GetQmEventDataControlLocal()));
}

template <typename SampleType>
void SkeletonEventCommon<SampleType>::EmplaceTypeErasedSamplePtrsGuard()
{
    score::cpp::ignore = type_erased_sample_ptrs_guard_.emplace(tracing_data_.service_element_tracing_data);
}

template <typename SampleType>
void SkeletonEventCommon<SampleType>::UpdateCurrentTimestamp()
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(event_data_control_composite_ref_.has_value(),
                                                "EventDataControlComposite must be initialized.");
    current_timestamp_ref_ = event_data_control_composite_ref_.value().GetLatestTimestamp();
}

template <typename SampleType>
void SkeletonEventCommon<SampleType>::SetQmNotificationsRegistered(bool value)
{
    qm_event_update_notifications_registered_.store(value);
}

template <typename SampleType>
void SkeletonEventCommon<SampleType>::SetAsilBNotificationsRegistered(bool value)
{
    asil_b_event_update_notifications_registered_.store(value);
}

template <typename SampleType>
void SkeletonEventCommon<SampleType>::ResetGuards() noexcept
{
    type_erased_sample_ptrs_guard_.reset();
    if (event_data_control_composite_ref_.has_value())
    {
        transaction_log_registration_guard_.reset();
    }
}

template <typename SampleType>
bool SkeletonEventCommon<SampleType>::IsQmNotificationsRegistered() const noexcept
{

    return qm_event_update_notifications_registered_.load();
}

template <typename SampleType>
bool SkeletonEventCommon<SampleType>::IsAsilBNotificationsRegistered() const noexcept
{

    return asil_b_event_update_notifications_registered_.load();
}

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_EVENT_COMMON_H_
