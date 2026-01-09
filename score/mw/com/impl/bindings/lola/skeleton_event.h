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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_EVENT_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_EVENT_H

#include "score/mw/com/impl/binding_type.h"
#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/event_data_control_composite.h"
#include "score/mw/com/impl/bindings/lola/event_data_storage.h"
#include "score/mw/com/impl/bindings/lola/i_runtime.h"
#include "score/mw/com/impl/bindings/lola/sample_allocatee_ptr.h"
#include "score/mw/com/impl/bindings/lola/skeleton.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event_properties.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_registration_guard.h"
#include "score/mw/com/impl/bindings/lola/type_erased_sample_ptrs_guard.h"
#include "score/mw/com/impl/plumbing/sample_allocatee_ptr.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/impl/skeleton_event_binding.h"
#include "score/mw/com/impl/tracing/skeleton_event_tracing.h"
#include "score/mw/com/impl/tracing/skeleton_event_tracing_data.h"

#include "score/mw/log/logging.h"

#include <score/assert.hpp>
#include <score/optional.hpp>
#include <score/utility.hpp>

#include <atomic>
#include <mutex>
#include <optional>
#include <string_view>
#include <tuple>
#include <utility>

namespace score::mw::com::impl::lola
{
namespace
{

template <typename EventType>
void PrepareOfferImpl(EventType& event)
{
    const bool tracing_globally_enabled = ((impl::Runtime::getInstance().GetTracingRuntime() != nullptr) &&
                                           (impl::Runtime::getInstance().GetTracingRuntime()->IsTracingEnabled()));
    if (!tracing_globally_enabled)
    {
        // in case tracing is globally disabled, this will never switch back to enable. Thus, we can directly disable
        // all trace points for this event. This avoids any further lookups to the tracing runtime during Send() calls.
        DisableAllTracePoints(event.GetTracingData());
    }

    const bool tracing_for_skeleton_event_enabled =
        event.GetTracingData().enable_send || event.GetTracingData().enable_send_with_allocate;
    // LCOV_EXCL_BR_START (Tool incorrectly marks the decision as "Decision couldn't be analyzed" despite all lines in
    // both branches (true / false) being covered. "Decision couldn't be analyzed" only appeared after changing the code
    // within the if statement (without changing the condition / tests). Suppression can be removed when bug is fixed in
    // Ticket-188259).
    if (tracing_for_skeleton_event_enabled)
    {
        // LCOV_EXCL_BR_STOP
        event.EmplaceTransactionLogRegistrationGuard();
        event.EmplaceTypeErasedSamplePtrsGuard();
    }

    event.UpdateCurrentTimestamp();

    // Register callbacks to be notified when event notification existence changes.
    // This allows us to optimise the Send() path by skipping NotifyEvent() when no handlers are registered.
    // Separate callbacks for QM and ASIL-B update their respective atomic flags for lock-free access.
    if (event.GetParent().GetInstanceQualityType() == QualityType::kASIL_QM)
    {
        GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa)
            .GetLolaMessaging()
            .RegisterEventNotificationExistenceChangedCallback(
                QualityType::kASIL_QM, event.GetElementFQId(), [&event](const bool has_handlers) noexcept {
                    event.SetQmNotificationsRegistered(has_handlers);
                });
    }
    if (event.GetParent().GetInstanceQualityType() == QualityType::kASIL_B)
    {
        GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa)
            .GetLolaMessaging()
            .RegisterEventNotificationExistenceChangedCallback(
                QualityType::kASIL_B, event.GetElementFQId(), [&event](const bool has_handlers) noexcept {
                    event.SetAsilBNotificationsRegistered(has_handlers);
                });
    }
}

template <typename EventType>
void PrepareStopOfferImpl(EventType& event) noexcept
{
    // Unregister event notification existence changed callbacks
    GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa)
        .GetLolaMessaging()
        .UnregisterEventNotificationExistenceChangedCallback(QualityType::kASIL_QM, event.GetElementFQId());

    if (event.GetParent().GetInstanceQualityType() == QualityType::kASIL_B)
    {
        GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa)
            .GetLolaMessaging()
            .UnregisterEventNotificationExistenceChangedCallback(QualityType::kASIL_B, event.GetElementFQId());
    }

    // Reset the flags to indicate no handlers are registered
    event.SetQmNotificationsRegistered(false);
    event.SetAsilBNotificationsRegistered(false);

    event.ResetGuards();
}

} // namespace

/// \brief Represents a binding specific instance (LoLa) of an event within a skeleton. It can be used to send events
/// via Shared Memory. It will be created via a Factory Method, that will instantiate this class based on deployment
/// values.
///
/// This class is _not_ user-facing.
///
/// All operations on this class are _not_ thread-safe, in a manner that they shall not be invoked in parallel by
/// different threads.
template <typename SampleType>
class SkeletonEvent final : public SkeletonEventBinding<SampleType>
{
    template <typename T>
    // Suppress "AUTOSAR C++14 A11-3-1", The rule declares: "Friend declarations shall not be used".
    // Design dessision: The "*Attorney" class is a helper, which sets the internal state of this class accessing
    // private members and used for testing purposes only.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class SkeletonEventAttorney;

  public:
    using typename SkeletonEventBinding<SampleType>::SendTraceCallback;
    using typename SkeletonEventBindingBase::SubscribeTraceCallback;
    using typename SkeletonEventBindingBase::UnsubscribeTraceCallback;

    SkeletonEvent(Skeleton& parent,
                  const ElementFqId event_fqn,
                  const std::string_view event_name,
                  const SkeletonEventProperties properties,
                  impl::tracing::SkeletonEventTracingData skeleton_event_tracing_data = {}) noexcept;

    SkeletonEvent(const SkeletonEvent&) = delete;
    SkeletonEvent(SkeletonEvent&&) noexcept = delete;
    SkeletonEvent& operator=(const SkeletonEvent&) & = delete;
    SkeletonEvent& operator=(SkeletonEvent&&) & noexcept = delete;

    ~SkeletonEvent() override = default;

    /// \brief Sends a value by _copy_ towards a consumer. It will allocate the necessary space and then copy the value
    /// into Shared Memory.
    ResultBlank Send(const SampleType& value, score::cpp::optional<SendTraceCallback> send_trace_callback) noexcept override;

    ResultBlank Send(impl::SampleAllocateePtr<SampleType> sample,
                     score::cpp::optional<SendTraceCallback> send_trace_callback) noexcept override;

    Result<impl::SampleAllocateePtr<SampleType>> Allocate() noexcept override;

    /// @requirement SWS_CM_00700
    ResultBlank PrepareOffer() noexcept override;

    void PrepareStopOffer() noexcept override;

    BindingType GetBindingType() const noexcept override
    {
        return BindingType::kLoLa;
    }

    void SetSkeletonEventTracingData(impl::tracing::SkeletonEventTracingData tracing_data) noexcept override
    {
        skeleton_event_tracing_data_ = tracing_data;
    }

    ElementFqId GetElementFQId() const noexcept
    {
        return event_fqn_;
    };

    // The following methods are public but intended for use by PrepareOfferImpl helper
    impl::tracing::SkeletonEventTracingData& GetTracingData()
    {
        return skeleton_event_tracing_data_;
    }

    void EmplaceTransactionLogRegistrationGuard()
    {
        score::cpp::ignore = transaction_log_registration_guard_.emplace(
            TransactionLogRegistrationGuard::Create(event_data_control_composite_->GetQmEventDataControl()));
    }

    void EmplaceTypeErasedSamplePtrsGuard()
    {
        score::cpp::ignore = type_erased_sample_ptrs_guard_.emplace(skeleton_event_tracing_data_.service_element_tracing_data);
    }

    void UpdateCurrentTimestamp()
    {
        current_timestamp_ = event_data_control_composite_.value().GetLatestTimestamp();
    }

    Skeleton& GetParent()
    {
        return parent_;
    }

    void SetQmNotificationsRegistered(bool value)
    {
        qm_event_update_notifications_registered_.store(value);
    }

    void SetAsilBNotificationsRegistered(bool value)
    {
        asil_b_event_update_notifications_registered_.store(value);
    }

    void ResetGuards() noexcept
    {
        type_erased_sample_ptrs_guard_.reset();
        if (event_data_control_composite_.has_value())
        {
            transaction_log_registration_guard_.reset();
        }
    }

  private:
    Skeleton& parent_;
    const ElementFqId event_fqn_;
    const std::string_view event_name_;
    const SkeletonEventProperties event_properties_;
    EventDataStorage<SampleType>* event_data_storage_;
    std::optional<EventDataControlComposite> event_data_control_composite_;
    EventSlotStatus::EventTimeStamp current_timestamp_;
    bool qm_disconnect_;
    impl::tracing::SkeletonEventTracingData skeleton_event_tracing_data_;

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
    /// type erased sample pointers which are created in PrepareOffer() and destroyed in PrepareStopoffer() - optional
    /// as only needed when tracing is enabled and when they haven't been cleaned up via a call to PrepareStopoffer().
    std::optional<TransactionLogRegistrationGuard> transaction_log_registration_guard_;
    std::optional<tracing::TypeErasedSamplePtrsGuard> type_erased_sample_ptrs_guard_;
};

template <typename SampleType>
SkeletonEvent<SampleType>::SkeletonEvent(Skeleton& parent,
                                         const ElementFqId event_fqn,
                                         const std::string_view event_name,
                                         const SkeletonEventProperties properties,
                                         impl::tracing::SkeletonEventTracingData skeleton_event_tracing_data) noexcept
    : SkeletonEventBinding<SampleType>{},
      parent_{parent},
      event_fqn_{event_fqn},
      event_name_{event_name},
      event_properties_{properties},
      event_data_storage_{nullptr},
      event_data_control_composite_{std::nullopt},
      current_timestamp_{1U},
      qm_disconnect_{false},
      skeleton_event_tracing_data_{skeleton_event_tracing_data},
      transaction_log_registration_guard_{},
      type_erased_sample_ptrs_guard_{}
{
}

template <typename SampleType>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from 'allocated_slot_result.value()' in case the
// 'allocated_slot_result' doesn't have value but as we check before with 'has_value()' so no way for throwing
// std::bad_optional_access which leds to std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
ResultBlank SkeletonEvent<SampleType>::Send(const SampleType& value,
                                            score::cpp::optional<SendTraceCallback> send_trace_callback) noexcept
{
    auto allocated_slot_result = Allocate();
    if (!(allocated_slot_result.has_value()))
    {
        return MakeUnexpected(ComErrc::kSampleAllocationFailure, "Could not allocate slot");
    }
    auto allocated_slot = std::move(allocated_slot_result).value();
    *allocated_slot = value;

    return Send(std::move(allocated_slot), std::move(send_trace_callback));
}

template <typename SampleType>
ResultBlank SkeletonEvent<SampleType>::Send(impl::SampleAllocateePtr<SampleType> sample,
                                            score::cpp::optional<SendTraceCallback> send_trace_callback) noexcept
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

    if (send_trace_callback.has_value())
    {
        (*send_trace_callback)(sample);
    }

    // Only call NotifyEvent if there are any registered receive handlers for each quality level.
    // This avoids the expensive lock operation in the common case where no handlers are registered.
    // Using memory_order_relaxed is safe here as this is an optimisation, if we miss a very recent
    // handler registration, the next Send() will pick it up.
    if (qm_event_update_notifications_registered_.load() && !qm_disconnect_)
    {
        GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa)
            .GetLolaMessaging()
            .NotifyEvent(QualityType::kASIL_QM, event_fqn_);
    }
    if (asil_b_event_update_notifications_registered_.load() &&
        parent_.GetInstanceQualityType() == QualityType::kASIL_B)
    {
        GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa)
            .GetLolaMessaging()
            .NotifyEvent(QualityType::kASIL_B, event_fqn_);
    }
    return {};
}

template <typename SampleType>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from '.value()' in case it doesn't have value but as we check
// before with 'has_value()' so no way for throwing std::bad_optional_access, which leads to std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
Result<impl::SampleAllocateePtr<SampleType>> SkeletonEvent<SampleType>::Allocate() noexcept
{
    if (event_data_control_composite_.has_value() == false)
    {
        ::score::mw::log::LogError("lola") << "Tried to allocate event, but the EventDataControl does not exist!";
        return MakeUnexpected(ComErrc::kBindingFailure);
    }
    const auto slot = event_data_control_composite_->AllocateNextSlot();

    // Suppress "AUTOSAR C++14 A5-2-6" rule finding. This rule states:"The operands of a logical && or \\ shall be
    // parenthesized if the operands contain binary operators".
    // This suppression is unnecessary as the operands do not contain binary operators.
    // A bug ticket has been created to track this: [Ticket-165315](broken_link_j/Ticket-165315)
    // coverity[autosar_cpp14_a5_2_6_violation : FALSE]
    if (!qm_disconnect_ && event_data_control_composite_->GetAsilBEventDataControl().has_value() && !slot.IsValidQM())
    {
        qm_disconnect_ = true;
        score::mw::log::LogWarn("lola")
            << __func__ << __LINE__
            << "Disconnecting unsafe QM consumers as slot allocation failed on an ASIL-B enabled event: " << event_fqn_;
        parent_.DisconnectQmConsumers();
    }

    if (slot.IsValidQM() || slot.IsValidAsilB())
    {
        return MakeSampleAllocateePtr(
            SampleAllocateePtr<SampleType>(&event_data_storage_->at(static_cast<std::uint64_t>(slot.GetIndex())),
                                           *event_data_control_composite_,
                                           slot));
    }
    else
    {
        // we didn't get a slot, which is a sign, that too few slots have been configured.
        if (event_properties_.enforce_max_samples == false)
        {
            ::score::mw::log::LogError("lola")
                << "SkeletonEvent: Allocation of event slot failed. Hint: enforceMaxSamples was "
                   "disabled by config. Might be the root cause!";
        }
        return MakeUnexpected(ComErrc::kBindingFailure);
    }
}

template <typename SampleType>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, all results which are accessed with '.value()' that could implicitly call
// 'std::terminate()' (in case it doesn't have value) has a check in advance using '.has_value()', so no way for
// throwing std::bad_optional_access which leds to std::terminate(). This suppression should be removed after fixing
// [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
ResultBlank SkeletonEvent<SampleType>::PrepareOffer() noexcept
{
    std::tie(event_data_storage_, event_data_control_composite_) =
        parent_.Register<SampleType>(event_fqn_, event_properties_);

    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(event_data_control_composite_.has_value(),
                           "Defensive programming as event_data_control_composite_ is set by Register above.");

    PrepareOfferImpl(*this);

    return {};
}

template <typename SampleType>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, all results which are accessed with '.value()' that could implicitly call
// 'std::terminate()' (in case it doesn't have value) has a check in advance using '.has_value()', so no way for
// throwing std::bad_optional_access which leds to std::terminate(). This suppression should be removed after fixing
// [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
void SkeletonEvent<SampleType>::PrepareStopOffer() noexcept
{
    PrepareStopOfferImpl(*this);
}

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_EVENT_H
