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
#include "score/mw/com/impl/bindings/lola/sample_allocatee_ptr.h"
#include "score/mw/com/impl/bindings/lola/sample_ptr.h"
#include "score/mw/com/impl/bindings/lola/skeleton.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event_properties.h"
#include "score/mw/com/impl/bindings/lola/type_erased_sample_ptrs_guard.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/plumbing/sample_allocatee_ptr.h"
#include "score/mw/com/impl/plumbing/sample_ptr.h"
#include "score/mw/com/impl/skeleton_event_binding.h"
#include "score/mw/com/impl/tracing/skeleton_event_tracing_data.h"

#include "score/memory/data_type_size_info.h"
#include "score/mw/log/logging.h"
#include "score/result/result.h"

#include <score/assert.hpp>
#include <score/utility.hpp>

#include <atomic>
#include <optional>
#include <string_view>
#include <utility>

namespace score::mw::com::impl::lola
{

/// \brief Maximum number of concurrent SamplePtrs that can be held from GetLatestSample() at any time.
static constexpr std::uint8_t kMaxConcurrentFieldGetterSamplePtrs{1U};

/// \brief Represents a binding specific instance (LoLa) of an event within a skeleton. It can be used to send events
/// via Shared Memory. It will be created via a Factory Method, that will instantiate this class based on deployment
/// values.
///
/// This class is _not_ user-facing.
///
/// All operations on this class are _not_ thread-safe, in a manner that they shall not be invoked in parallel by
/// different threads.
class SkeletonEvent final : public SkeletonEventBinding
{
    // Suppress "AUTOSAR C++14 A11-3-1", The rule declares: "Friend declarations shall not be used".
    // Design decision: The "*Attorney" class is a helper, which sets the internal state of this class accessing
    // private members and used for testing purposes only.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class SkeletonEventAttorney;

    using ReceiveHandlerRegistrationChangedCallback = lola::IMessagePassingService::HandlerStatusChangeCallback;

  public:
    using SkeletonEventBinding::SendTraceCallback;
    using SkeletonEventBinding::SubscribeTraceCallback;
    using SkeletonEventBinding::UnsubscribeTraceCallback;

    SkeletonEvent(Skeleton& parent,
                  const ElementFqId element_fq_id,
                  const std::string_view event_name,
                  const memory::DataTypeSizeInfo size_info,
                  const SkeletonEventProperties properties,
                  impl::tracing::SkeletonEventTracingData skeleton_event_tracing_data) noexcept;

    SkeletonEvent(const SkeletonEvent&) = delete;
    SkeletonEvent(SkeletonEvent&&) noexcept = delete;
    SkeletonEvent& operator=(const SkeletonEvent&) & = delete;
    SkeletonEvent& operator=(SkeletonEvent&&) & noexcept = delete;

    ~SkeletonEvent() noexcept override = default;

    /// \brief Sends a value by _copy_ towards a consumer. It will allocate the necessary space and then copy the value
    /// into Shared Memory.
    Result<void> Send(const void* value_ptr,
                      std::optional<SendTraceCallback> send_trace_callback,
                      SampleAllocateeGuard guard) noexcept override;

    Result<void> Send(impl::SampleAllocateePtr<void> sample,
                      std::optional<SendTraceCallback> send_trace_callback) noexcept override;

    Result<impl::SampleAllocateePtr<void>> Allocate(SampleAllocateeGuard guard) noexcept override;

    Result<impl::SamplePtr<void>> GetLatestSample(QualityType quality_type) override;

    Result<void> PrepareOffer() noexcept override;

    void PrepareStopOffer() noexcept override;

    /// \brief Get size for the underlying event-type (including possible dynamic memory allocations) and its alignment
    memory::DataTypeSizeInfo GetSizeInfo() const noexcept override
    {
        return event_sample_size_info;
    };

    BindingType GetBindingType() const noexcept override
    {
        return BindingType::kLoLa;
    }

    void SetSkeletonEventTracingData(impl::tracing::SkeletonEventTracingData tracing_data) noexcept override;

    Result<void> Notify() noexcept override;

    Result<void> SetReceiveHandlerRegistrationChangedHandler(
        ReceiveHandlerRegistrationChangedCallback callback) noexcept override;

    Result<void> UnsetReceiveHandlerRegistrationChangedHandler() noexcept override;

  private:
    Skeleton& parent_;
    std::string_view event_name_;
    ElementFqId element_fq_id_;
    EventDataStorage* event_data_storage_;
    memory::DataTypeSizeInfo event_sample_size_info;
    SkeletonEventProperties event_properties_;

    std::optional<ProviderEventDataControlLocalView<>> provider_control_local_view_qm_;
    std::optional<ProviderEventDataControlLocalView<>> provider_control_local_view_asil_b_;
    std::optional<ConsumerEventDataControlLocalView<>> consumer_control_local_view_qm_;
    std::optional<ConsumerEventDataControlLocalView<>> consumer_control_local_view_asil_b_;
    std::optional<EventDataControlComposite<>> event_data_control_composite_;

    EventSlotStatus::EventTimeStamp current_timestamp_;
    impl::tracing::SkeletonEventTracingData tracing_data_;

    bool qm_disconnect_;
    bool field_getter_enabled_;
    SampleReferenceTracker getter_sample_tracker_;

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

    /// \brief optional RAII guards for tracing transaction log registration/un-registration and cleanup of
    /// "pending" type erased sample pointers which are created in PrepareOffer() and destroyed in
    /// PrepareStopOffer()
    /// - optional as only needed when tracing is enabled and when they haven't been cleaned up via a call to
    /// PrepareStopOffer().
    std::optional<TransactionLogRegistrationGuard> transaction_log_registration_guard_qm_{};
    std::optional<TransactionLogRegistrationGuard> transaction_log_registration_guard_asil_b_{};
    std::optional<tracing::TypeErasedSamplePtrsGuard> type_erased_sample_ptrs_guard_{};
    std::optional<ReceiveHandlerRegistrationChangedCallback> receive_handler_registration_changed_callback_;

    void UpdateCurrentTimestamp();
    void SetQmNotificationsRegistered(bool value);
    void SetAsilBNotificationsRegistered(bool value);
    void ResetGuards() noexcept;
    EventDataControlComposite<>& GetEventDataControlComposite()
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(event_data_control_composite_.has_value());
        return event_data_control_composite_.value();
    }

    ConsumerEventDataControlLocalView<>& GetConsumerEventDataControlLocalView(QualityType quality_type)
    {
        if (quality_type == QualityType::kASIL_B)
        {
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(consumer_control_local_view_asil_b_.has_value());
            return consumer_control_local_view_asil_b_.value();
        }

        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(consumer_control_local_view_qm_.has_value());
        return consumer_control_local_view_qm_.value();
    }
    /// \brief Dispatches NotifyEvent() to QM and ASIL consumers if their respective
    ///        receive-handler registration flags are set.
    Result<void> NotifyConsumersIfHandlersRegistered() noexcept;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_EVENT_H
