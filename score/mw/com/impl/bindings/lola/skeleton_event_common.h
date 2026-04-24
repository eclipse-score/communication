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

#include "score/mw/com/impl/bindings/lola/control_slot_types.h"
#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/event_data_control_composite.h"
#include "score/mw/com/impl/bindings/lola/i_runtime.h"
#include "score/mw/com/impl/bindings/lola/messaging/i_message_passing_service.h"
#include "score/mw/com/impl/bindings/lola/skeleton.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event_properties.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_registration_guard.h"
#include "score/mw/com/impl/bindings/lola/type_erased_sample_ptrs_guard.h"
#include "score/mw/com/impl/plumbing/sample_allocatee_ptr.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/impl/skeleton_event_binding.h"
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
/// GenericSkeletonEvent.
template <typename SampleType>
class SkeletonEventCommon
{
    // Grant friendship to allow access to private helpers
    // The "SkeletonEventAttorney" class is a helper, which sets the internal state of "SkeletonEventCommon" accessing
    // private members and used for testing purposes only.
    friend class SkeletonEventAttorney<SampleType>;

  public:
    SkeletonEventCommon(Skeleton& parent,
                        const std::string_view event_name,
                        const SkeletonEventProperties& event_properties,
                        const ElementFqId& element_fq_id,
                        impl::tracing::SkeletonEventTracingData tracing_data = {}) noexcept;

    SkeletonEventCommon(const SkeletonEventCommon&) = delete;
    SkeletonEventCommon(SkeletonEventCommon&&) noexcept = delete;
    SkeletonEventCommon& operator=(const SkeletonEventCommon&) & = delete;
    SkeletonEventCommon& operator=(SkeletonEventCommon&&) & noexcept = delete;

    ~SkeletonEventCommon() = default;

    void PrepareOfferCommon(const EventDataControlComposite<> event_data_control_composite) noexcept;
    void PrepareStopOfferCommon() noexcept;

    Result<SlotIndexType> AllocateSlot() noexcept;
    Result<void> Send(impl::SampleAllocateePtr<SampleType>& sample) noexcept;

    // Accessors for members used by PrepareOfferCommon/PrepareStopOfferCommon
    void SetSkeletonEventTracingData(impl::tracing::SkeletonEventTracingData tracing_data) noexcept
    {
        tracing_data_ = tracing_data;
    }

    const ElementFqId& GetElementFQId() const
    {
        return element_fq_id_;
    }

    Skeleton& GetParent()
    {
        return parent_;
    }

    [[nodiscard]] const SkeletonEventProperties& GetEventProperties() const
    {
        return event_properties_;
    }

    EventDataControlComposite<>& GetEventDataControlComposite()
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(event_data_control_composite_.has_value());
        return event_data_control_composite_.value();
    }

  private:
    Skeleton& parent_;
    std::string_view event_name_;
    SkeletonEventProperties event_properties_;
    ElementFqId element_fq_id_;
    std::optional<EventDataControlComposite<>> event_data_control_composite_;
    EventSlotStatus::EventTimeStamp current_timestamp_;
    impl::tracing::SkeletonEventTracingData tracing_data_;
    bool qm_disconnect_;

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
SkeletonEventCommon<SampleType>::SkeletonEventCommon(Skeleton& parent,
                                                     const std::string_view event_name,
                                                     const SkeletonEventProperties& event_properties,
                                                     const ElementFqId& element_fq_id,
                                                     impl::tracing::SkeletonEventTracingData tracing_data) noexcept
    : parent_{parent},
      event_name_{event_name},
      event_properties_{event_properties},
      element_fq_id_{element_fq_id},
      event_data_control_composite_{},
      current_timestamp_{1U},
      tracing_data_{tracing_data},
      qm_disconnect_{false}
{
}

template <typename SampleType>
void SkeletonEventCommon<SampleType>::PrepareOfferCommon(
    const EventDataControlComposite<> event_data_control_composite) noexcept
{
    event_data_control_composite_ = event_data_control_composite;

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
            QualityType::kASIL_QM, element_fq_id_, [this](const bool has_handlers) noexcept {
                SetQmNotificationsRegistered(has_handlers);
            });

    if (parent_.GetInstanceQualityType() == QualityType::kASIL_B)
    {
        GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa)
            .GetLolaMessaging()
            .RegisterEventNotificationExistenceChangedCallback(
                QualityType::kASIL_B, element_fq_id_, [this](const bool has_handlers) noexcept {
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
        .UnregisterEventNotificationExistenceChangedCallback(QualityType::kASIL_QM, element_fq_id_);

    if (parent_.GetInstanceQualityType() == QualityType::kASIL_B)
    {
        GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa)
            .GetLolaMessaging()
            .UnregisterEventNotificationExistenceChangedCallback(QualityType::kASIL_B, element_fq_id_);
    }

    // Reset the flags to indicate no handlers are registered
    SetQmNotificationsRegistered(false);
    SetAsilBNotificationsRegistered(false);

    ResetGuards();

    event_data_control_composite_.reset();
}

template <typename SampleType>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from '.value()' in case it doesn't have value but as we check
// before with 'has_value()' so no way for throwing std::bad_optional_access, which leads to std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
Result<SlotIndexType> SkeletonEventCommon<SampleType>::AllocateSlot() noexcept
{
    if (event_data_control_composite_.has_value() == false)
    {
        ::score::mw::log::LogError("lola") << "Tried to allocate event, but the EventDataControl does not exist!";
        return MakeUnexpected(ComErrc::kBindingFailure);
    }
    auto& event_data_control_composite = event_data_control_composite_.value();
    const auto allocated_slot_result = event_data_control_composite.AllocateNextSlot();

    // Suppress "AUTOSAR C++14 A5-2-6" rule finding. This rule states:"The operands of a logical && or \\ shall be
    // parenthesized if the operands contain binary operators".
    // This suppression is unnecessary as the operands do not contain binary operators.
    // A bug ticket has been created to track this: [Ticket-165315](broken_link_j/Ticket-165315)
    // coverity[autosar_cpp14_a5_2_6_violation : FALSE]
    if (!qm_disconnect_ && (event_data_control_composite.GetAsilBEventDataControlLocal() != nullptr) &&
        allocated_slot_result.qm_misbehaved)
    {
        qm_disconnect_ = true;
        score::mw::log::LogWarn("lola")
            << __func__ << __LINE__
            << "Disconnecting unsafe QM consumers as slot allocation failed on an ASIL-B enabled event: "
            << element_fq_id_;
        parent_.DisconnectQmConsumers();
    }

    if (!allocated_slot_result.allocated_slot_index.has_value())
    {
        // we didn't get a slot, which is a sign, that too few slots have been configured.
        if (!event_properties_.enforce_max_samples)
        {
            ::score::mw::log::LogError("lola")
                << "SkeletonEvent: Allocation of event slot failed. Hint: enforceMaxSamples was "
                   "disabled by config. Might be the root cause!";
        }
        return MakeUnexpected(ComErrc::kBindingFailure);
    }
    return allocated_slot_result.allocated_slot_index.value();
}

template <typename SampleType>
Result<void> SkeletonEventCommon<SampleType>::Send(impl::SampleAllocateePtr<SampleType>& sample) noexcept
{
    const impl::SampleAllocateePtrView<SampleType> view{sample};
    auto ptr = view.template As<lola::SampleAllocateePtr<SampleType>>();
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(nullptr != ptr);
    // Suppress The rule AUTOSAR C++14 A5-3-2: "Null pointers shall not be dereferenced.".
    // The "ptr" variable is checked before dereferencing.
    // coverity[autosar_cpp14_a5_3_2_violation]
    auto slot = ptr->GetReferencedSlot();
    // Suppress "AUTOSAR C++14 A4-7-1" rule finding. This rule states: "An integer expression shall
    // not lead to data loss.".
    // The current logic will not exceed the maximum value.
    // coverity[autosar_cpp14_a4_7_1_violation]
    ++current_timestamp_;
    event_data_control_composite_->EventReady(slot, current_timestamp_);

    // Only call NotifyEvent if there are any registered receive handlers for each quality level.
    // This avoids the expensive lock operation in the common case where no handlers are registered.
    // Using memory_order_relaxed is safe here as this is an optimisation, if we miss a very recent
    // handler registration, the next Send() will pick it up.
    if (qm_event_update_notifications_registered_.load() && !qm_disconnect_)
    {
        GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa)
            .GetLolaMessaging()
            .NotifyEvent(QualityType::kASIL_QM, element_fq_id_);
    }
    if (asil_b_event_update_notifications_registered_.load() &&
        parent_.GetInstanceQualityType() == QualityType::kASIL_B)
    {
        GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa)
            .GetLolaMessaging()
            .NotifyEvent(QualityType::kASIL_B, element_fq_id_);
    }
    return {};
}

template <typename SampleType>
void SkeletonEventCommon<SampleType>::EmplaceTransactionLogRegistrationGuard()
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(event_data_control_composite_.has_value(),
                                                "EventDataControlComposite must be initialized.");
    score::cpp::ignore = transaction_log_registration_guard_.emplace(
        TransactionLogRegistrationGuard::Create(event_data_control_composite_.value().GetQmEventDataControlLocal()));
}

template <typename SampleType>
void SkeletonEventCommon<SampleType>::EmplaceTypeErasedSamplePtrsGuard()
{
    score::cpp::ignore = type_erased_sample_ptrs_guard_.emplace(tracing_data_.service_element_tracing_data);
}

template <typename SampleType>
void SkeletonEventCommon<SampleType>::UpdateCurrentTimestamp()
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(event_data_control_composite_.has_value(),
                                                "EventDataControlComposite must be initialized.");
    current_timestamp_ = event_data_control_composite_.value().GetLatestTimestamp();
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
    if (event_data_control_composite_.has_value())
    {
        transaction_log_registration_guard_.reset();
    }
}

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_EVENT_COMMON_H_
