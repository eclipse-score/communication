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
#include "score/mw/com/impl/bindings/lola/skeleton_event.h"

#include "score/mw/com/impl/bindings/lola/sample_ptr.h"

namespace score::mw::com::impl::lola
{

SkeletonEvent::SkeletonEvent(Skeleton& parent,
                             const ElementFqId element_fq_id,
                             const std::string_view event_name,
                             const memory::DataTypeSizeInfo size_info,
                             const SkeletonEventProperties properties,
                             impl::tracing::SkeletonEventTracingData skeleton_event_tracing_data) noexcept
    : parent_{parent},
      event_name_{event_name},
      element_fq_id_(element_fq_id),
      event_data_storage_local_view_{std::nullopt},
      event_sample_size_info_(size_info),
      event_properties_{properties},
      current_timestamp_{EventSlotStatus::INVALID_TIMESTAMP},
      tracing_data_{skeleton_event_tracing_data},
      qm_disconnect_{false},
      field_getter_enabled_{event_properties_.GetNumberOfFieldGetterSlots() > 0U},
      getter_sample_tracker_{kMaxConcurrentFieldGetterSamplePtrs},
      qm_event_update_notifications_registered_{false},
      asil_b_event_update_notifications_registered_{false}
{
}

Result<void> SkeletonEvent::Send(impl::SampleAllocateePtr<void> sample,
                                 const std::optional<SendTraceCallback>& send_trace_callback)
{
    const impl::SampleAllocateePtrView<void> view{sample};
    const auto* ptr = view.template As<lola::SampleAllocateePtr>();
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(nullptr != ptr);
    // Suppress The rule AUTOSAR C++14 A5-3-2: "Null pointers shall not be dereferenced".
    // The "ptr" variable is checked before dereferencing.
    // coverity[autosar_cpp14_a5_3_2_violation]
    auto slot = ptr->GetReferencedSlot();
    // Suppress "AUTOSAR C++14 A4-7-1" rule finding. This rule states: "An integer expression shall
    // not lead to data loss.".
    // The current logic will not exceed the maximum value.
    // coverity[autosar_cpp14_a4_7_1_violation]
    ++current_timestamp_;
    if (event_properties_.coherency == CoherencyType::kCoherent)
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(event_data_control_coherent_composite_.has_value());
        event_data_control_coherent_composite_.value().EventReady(slot, current_timestamp_);
    }
    else
    {
        event_data_control_composite_->EventReady(slot, current_timestamp_);
    }
    std::ignore = NotifyConsumersIfHandlersRegistered();
    if (send_trace_callback.has_value())
    {
        (*send_trace_callback)(sample);
    }

    return {};
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from '.value()' in case it doesn't have value but as we check
// before with 'has_value()' so no way for throwing std::bad_optional_access, which leads to std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
Result<impl::SampleAllocateePtr<void>> SkeletonEvent::Allocate(SampleAllocateeGuard guard)
{
    if (event_properties_.coherency == CoherencyType::kCoherent)
    {
        if (!event_data_control_coherent_composite_.has_value())
        {
            ::score::mw::log::LogError("lola") << "Tried to allocate event, but the EventDataControlCoherent does "
                                                  "not exist!";
            return MakeUnexpected(ComErrc::kBindingFailure);
        }
        // Unlike the kNonCoherent path below, slot allocation always succeeds (no "too few slots configured" case
        // exists for a kCoherent event, see ProviderEventDataControlCoherentLocalView). The only possible reaction
        // to a misbehaving QM consumer is detecting a QM/ASIL-B control section divergence and disconnecting QM
        // consumers from that point onwards (see EventDataControlCoherentComposite::IsQmControlDisconnected()).
        auto& event_data_control_coherent_composite = event_data_control_coherent_composite_.value();
        const auto slot_index = event_data_control_coherent_composite.AllocateNextSlot();

        if (!qm_disconnect_ && event_data_control_coherent_composite.IsQmControlDisconnected())
        {
            qm_disconnect_ = true;
            score::mw::log::LogWarn("lola")
                << __func__ << __LINE__
                << "Disconnecting unsafe QM consumers as QM/ASIL-B control sections diverged on a kCoherent "
                   "ASIL-B enabled event: "
                << element_fq_id_;
            parent_.DisconnectQmConsumers();
        }

        return MakeSampleAllocateePtr(
            SampleAllocateePtr(event_data_storage_local_view_->GetTypeErasedDataSlot(slot_index),
                               event_data_control_coherent_composite,
                               slot_index),
            std::move(guard));
    }

    if (!event_data_control_composite_.has_value())
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

    const auto slot_index = allocated_slot_result.allocated_slot_index.value();

    // The ConsumerEventDataControlLocalView stored inside SampleAllocateePtr is only used by the send-tracing path.
    // Tracing is a diagnostic/monitoring feature with no safety requirement, so QM is sufficient.
    // GetConsumerEventDataControlLocalView(kASIL_B) is a separate code path introduced specifically for
    // GetLatestSample().
    return MakeSampleAllocateePtr(SampleAllocateePtr(event_data_storage_local_view_->GetTypeErasedDataSlot(slot_index),
                                                     GetEventDataControlComposite(),
                                                     GetConsumerEventDataControlLocalView(QualityType::kASIL_QM),
                                                     slot_index),
                                  std::move(guard));
}

Result<impl::SamplePtr<void>> SkeletonEvent::GetLatestSample(QualityType quality_type)
{
    // CoherencyType::kCoherent is restricted (at configuration-validation time, see
    // CreateSkeletonEventProperties()) to events/fields with no Getter enabled, so this should never be invoked for
    // a kCoherent event.
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
        event_properties_.coherency != CoherencyType::kCoherent,
        "GetLatestSample() is not supported for a CoherencyType::kCoherent event.");

    const QualityType event_quality_type = parent_.GetInstanceQualityType();
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
        (event_quality_type != QualityType::kASIL_QM) || (quality_type != QualityType::kASIL_B),
        "ASIL-B event support ASIL-QM and ASIL-B quality types, but ASIL-QM event support only ASIL-QM quality type.");

    auto guard = getter_sample_tracker_.Allocate(1U).TakeGuard();
    if (!guard.has_value())
    {
        ::score::mw::log::LogError("lola")
            << "GetLatestSample called while a SamplePtr from a previous call is still alive";
        return MakeUnexpected(ComErrc::kMaxSamplesReached);
    }

    auto& consumer_event_data_control_local = GetConsumerEventDataControlLocalView(quality_type);

    // ReferenceNextEvent returns the slot with the highest timestamp in the exclusive range (min, max).
    // We pass 0 and TIMESTAMP_MAX to span the entire valid timestamp range, so it always returns the
    // most recently written sample regardless of its timestamp.
    const auto slot_result = consumer_event_data_control_local.ReferenceNextEvent(0U, EventSlotStatus::TIMESTAMP_MAX);
    if (!slot_result.has_value())
    {
        ::score::mw::log::LogError("lola") << "ReferenceNextEvent did not return a slot index";
        return MakeUnexpected(ComErrc::kBindingFailure);
    }

    return impl::SamplePtr<void>{lola::SamplePtr{event_data_storage_local_view_->GetTypeErasedDataSlot(*slot_result),
                                                 consumer_event_data_control_local,
                                                 slot_result.value()},
                                 std::move(*guard)};
}

Result<void> SkeletonEvent::PrepareOffer(const std::optional<InitializeSampleCallback>& initialize_sample_callback)
{
    // Invariant: after a successful PrepareOffer(), event_data_storage_local_view_ is guaranteed to be non-null.
    // All methods that require event_data_storage_local_view_ (e.g. GetLatestSample) rely on this invariant.
    const auto registration_result =
        parent_.Register(element_fq_id_, event_properties_, event_sample_size_info_, initialize_sample_callback);
    event_data_storage_local_view_.emplace(registration_result.event_data_storage, event_sample_size_info_);

    if (event_properties_.coherency == CoherencyType::kCoherent)
    {
        auto* const event_control_coherent_pair =
            std::get_if<SkeletonMemoryManager::EventControlCoherentPair>(&registration_result.event_controls);
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
            event_control_coherent_pair != nullptr,
            "EventControlCoherent must be available for a CoherencyType::kCoherent event.");
        auto& event_control_coherent_qm = event_control_coherent_pair->qm.get();

        auto& provider_control_local_view_coherent_qm =
            provider_control_local_view_coherent_qm_.emplace(event_control_coherent_qm.data_control);

        ProviderEventDataControlCoherentLocalView* provider_control_local_view_coherent_asil_b_ptr{nullptr};
        if (event_control_coherent_pair->asil_b != nullptr)
        {
            provider_control_local_view_coherent_asil_b_ptr = &provider_control_local_view_coherent_asil_b_.emplace(
                event_control_coherent_pair->asil_b->data_control);
        }
        score::cpp::ignore = event_data_control_coherent_composite_.emplace(
            provider_control_local_view_coherent_qm, provider_control_local_view_coherent_asil_b_ptr);

        // CoherencyType::kCoherent events do not support IPC-Tracing or field Getter/Setter semantics (enforced at
        // configuration-validation time, see CreateSkeletonEventProperties()), so unlike the kNonCoherent path
        // below, there is no transaction-log registration / type-erased-sample-ptrs-guard setup needed here. No
        // consumer-side local view is needed either, see the provider_control_local_view_coherent_qm_/_asil_b_
        // member doc.
    }
    else
    {
        auto* const non_coherent_pair =
            std::get_if<SkeletonMemoryManager::EventControlNonCoherentPair>(&registration_result.event_controls);
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
            non_coherent_pair != nullptr, "EventControl must be available for a CoherencyType::kNonCoherent event.");
        auto& event_control_qm = non_coherent_pair->qm.get();

        auto& provider_control_local_view_qm = provider_control_local_view_qm_.emplace(event_control_qm.data_control);
        score::cpp::ignore = consumer_control_local_view_qm_.emplace(event_control_qm.data_control);

        const bool is_skeleton_event_asil_b = non_coherent_pair->asil_b != nullptr;
        ProviderEventDataControlLocalView<>* provider_control_local_view_asil_b_ptr{nullptr};
        if (is_skeleton_event_asil_b)
        {
            auto& provider_control_local_view_asil_b =
                provider_control_local_view_asil_b_.emplace(non_coherent_pair->asil_b->data_control);
            score::cpp::ignore = consumer_control_local_view_asil_b_.emplace(non_coherent_pair->asil_b->data_control);
            provider_control_local_view_asil_b_ptr = &provider_control_local_view_asil_b;
        }
        score::cpp::ignore = event_data_control_composite_.emplace(provider_control_local_view_qm,
                                                                   provider_control_local_view_asil_b_ptr);

        const bool tracing_globally_enabled = ((impl::Runtime::getInstance().GetTracingRuntime() != nullptr) &&
                                               (impl::Runtime::getInstance().GetTracingRuntime()->IsTracingEnabled()));
        if (!tracing_globally_enabled)
        {
            DisableAllTracePoints(tracing_data_);
        }

        const bool tracing_for_skeleton_event_enabled =
            tracing_data_.enable_send || tracing_data_.enable_send_with_allocate;

        // QM TransactionLog: register if tracing is enabled OR getter is enabled
        if (tracing_for_skeleton_event_enabled || field_getter_enabled_)
        {
            score::cpp::ignore = transaction_log_registration_guard_qm_.emplace(
                event_control_qm.transaction_log_set_.RegisterSkeletonTransactionLog(
                    consumer_control_local_view_qm_.value()));
        }

        // ASIL-B TransactionLog: register ASIL-B TransactionLog if getter is enabled AND SkeletonEvent is ASIL-B.
        if (field_getter_enabled_ && is_skeleton_event_asil_b)
        {
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(consumer_control_local_view_asil_b_.has_value());
            score::cpp::ignore = transaction_log_registration_guard_asil_b_.emplace(
                non_coherent_pair->asil_b->transaction_log_set_.RegisterSkeletonTransactionLog(
                    consumer_control_local_view_asil_b_.value()));
        }

        // LCOV_EXCL_BR_START (Tool incorrectly marks the decision as "Decision couldn't be analyzed" despite all
        // lines in both branches (true / false) being covered. "Decision couldn't be analyzed" only appeared after
        // changing the code within the if statement (without changing the condition / tests). Suppression can be
        // removed when bug is fixed in Ticket-188259).
        if (tracing_for_skeleton_event_enabled)
        {
            score::cpp::ignore = type_erased_sample_ptrs_guard_.emplace(tracing_data_.service_element_tracing_data);
        }
        // LCOV_EXCL_BR_STOP
    }

    UpdateCurrentTimestamp();

    // Register callbacks to be notified when event notification existence changes.
    // This allows us to optimise the Send() path by skipping NotifyEvent() when no handlers are registered.
    // Separate callbacks for QM and ASIL-B update their respective atomic flags for lock-free access.
    // If a callback for receive handler registration changes has been set, like it is done for the gateway
    // use case, it will also be called.
    GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa)
        .GetLolaMessaging()
        .RegisterEventNotificationExistenceChangedCallback(
            QualityType::kASIL_QM, element_fq_id_, [this](const bool has_handlers) noexcept {
                SetQmNotificationsRegistered(has_handlers);
                if (receive_handler_registration_changed_callback_.has_value())
                {
                    const bool qm_registered = qm_event_update_notifications_registered_.load();
                    (*receive_handler_registration_changed_callback_)(qm_registered);
                }
            });

    if (parent_.GetInstanceQualityType() == QualityType::kASIL_B)
    {
        GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa)
            .GetLolaMessaging()
            .RegisterEventNotificationExistenceChangedCallback(
                QualityType::kASIL_B, element_fq_id_, [this](const bool has_handlers) noexcept {
                    SetAsilBNotificationsRegistered(has_handlers);
                    if (receive_handler_registration_changed_callback_.has_value())
                    {
                        const bool asil_b_registered = asil_b_event_update_notifications_registered_.load();
                        (*receive_handler_registration_changed_callback_)(asil_b_registered);
                    }
                });
    }

    return {};
}

void SkeletonEvent::PrepareStopOffer()
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
    provider_control_local_view_qm_.reset();
    provider_control_local_view_asil_b_.reset();
    consumer_control_local_view_qm_.reset();
    consumer_control_local_view_asil_b_.reset();
    event_data_control_coherent_composite_.reset();
    provider_control_local_view_coherent_qm_.reset();
    provider_control_local_view_coherent_asil_b_.reset();
}

void SkeletonEvent::SetSkeletonEventTracingData(impl::tracing::SkeletonEventTracingData tracing_data) noexcept
{
    tracing_data_ = tracing_data;
}

void SkeletonEvent::UpdateCurrentTimestamp()
{
    if (event_properties_.coherency == CoherencyType::kCoherent)
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(event_data_control_coherent_composite_.has_value(),
                                                    "EventDataControlCoherentComposite must be initialized.");
        current_timestamp_ = event_data_control_coherent_composite_.value().GetLatestTimestamp();
        return;
    }

    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(event_data_control_composite_.has_value(),
                                                "EventDataControlComposite must be initialized.");
    current_timestamp_ = event_data_control_composite_.value().GetLatestTimestamp();
}

void SkeletonEvent::SetQmNotificationsRegistered(bool value)
{
    qm_event_update_notifications_registered_.store(value);
}

void SkeletonEvent::SetAsilBNotificationsRegistered(bool value)
{
    asil_b_event_update_notifications_registered_.store(value);
}

void SkeletonEvent::ResetGuards() noexcept
{
    type_erased_sample_ptrs_guard_.reset();
    if (event_data_control_composite_.has_value())
    {
        transaction_log_registration_guard_asil_b_.reset();
        transaction_log_registration_guard_qm_.reset();
    }
}

Result<void> SkeletonEvent::NotifyConsumersIfHandlersRegistered()
{
    // Only call NotifyEvent if there are any registered receive handlers for each quality level.
    // This avoids the expensive lock operation in the common case where no handlers are registered.
    if (qm_event_update_notifications_registered_.load() && !qm_disconnect_)
    {
        GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa)
            .GetLolaMessaging()
            .NotifyEvent(QualityType::kASIL_QM, element_fq_id_);
    }
    if ((asil_b_event_update_notifications_registered_.load()) &&
        (parent_.GetInstanceQualityType() == QualityType::kASIL_B))
    {
        GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa)
            .GetLolaMessaging()
            .NotifyEvent(QualityType::kASIL_B, element_fq_id_);
    }
    return {};
}

Result<void> SkeletonEvent::Notify()
{
    return NotifyConsumersIfHandlersRegistered();
}

Result<void> SkeletonEvent::SetReceiveHandlerRegistrationChangedHandler(
    ReceiveHandlerRegistrationChangedCallback callback) noexcept
{
    receive_handler_registration_changed_callback_ = std::move(callback);
    return {};
}

Result<void> SkeletonEvent::UnsetReceiveHandlerRegistrationChangedHandler() noexcept
{
    receive_handler_registration_changed_callback_.reset();
    return {};
}

}  // namespace score::mw::com::impl::lola
