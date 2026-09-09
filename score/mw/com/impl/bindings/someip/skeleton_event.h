/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SKELETON_EVENT_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SKELETON_EVENT_H

#include "score/mw/com/impl/bindings/someip/element_fq_id.h"
#include "score/mw/com/impl/bindings/someip/event_data_storage.h"
#include "score/mw/com/impl/bindings/someip/i_slot_owner.h"
#include "score/mw/com/impl/bindings/someip/skeleton.h"
#include "score/mw/com/impl/bindings/someip/skeleton_event_properties.h"
#include "score/mw/com/impl/binding_type.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/initialize_sample_callback.h"
#include "score/mw/com/impl/plumbing/sample_allocatee_ptr.h"
#include "score/mw/com/impl/plumbing/sample_ptr.h"
#include "score/mw/com/impl/skeleton_event_binding.h"
#include "score/mw/com/impl/tracing/skeleton_event_tracing_data.h"

#include "score/memory/data_type_size_info.h"
#include "score/result/result.h"

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace score::mw::com::impl::someip
{

/// \brief Represents a binding specific instance (SOME/IP) of an event within a skeleton.
///
/// \details Mirrors lola::SkeletonEvent. As on the LoLa side, this class is type-erased: it only knows the size and
///          alignment of one sample and moves opaque bytes around. The essential difference is what Send() does with
///          those bytes: LoLa marks a shared-memory slot as ready so that consumers can read it in place, whereas the
///          SOME/IP binding hands the bytes to the transport.
///
/// This class is _not_ user-facing.
///
/// All operations on this class are _not_ thread-safe, in a manner that they shall not be invoked in parallel by
/// different threads.
class SkeletonEvent final : public SkeletonEventBinding, public ISlotOwner
{
    // Suppress "AUTOSAR C++14 A11-3-1", The rule declares: "Friend declarations shall not be used".
    // Design decision: The "*Attorney" class is a helper, which sets the internal state of this class accessing
    // private members and used for testing purposes only.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class SkeletonEventAttorney;

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

    /// \brief Sends a value by _copy_ towards a consumer. It will allocate the necessary slot and then copy the value
    /// into it.
    Result<void> Send(const void* value_ptr,
                      std::optional<SendTraceCallback> send_trace_callback,
                      SampleAllocateeGuard guard) noexcept override;

    Result<void> Send(impl::SampleAllocateePtr<void> sample,
                      std::optional<SendTraceCallback> send_trace_callback) noexcept override;

    Result<impl::SampleAllocateePtr<void>> Allocate(SampleAllocateeGuard guard) noexcept override;

    /// \brief Not supported by the SOME/IP binding yet.
    /// \details GetLatestSample() only serves the getter of a SkeletonField, and fields are out of scope of the
    ///          current SOME/IP increment. Returning an error (instead of aborting) keeps a misconfigured field
    ///          diagnosable at runtime.
    Result<impl::SamplePtr<void>> GetLatestSample(QualityType quality_type) override;

    Result<void> PrepareOffer(
        const std::optional<InitializeSampleCallback>& initialize_sample_callback) noexcept override;

    void PrepareStopOffer() noexcept override;

    /// \brief Get size and alignment for the underlying event-type (including possible dynamic memory allocations).
    memory::DataTypeSizeInfo GetEventDataTypeSizeInfo() const noexcept override
    {
        return event_sample_size_info_;
    }

    BindingType GetBindingType() const noexcept override
    {
        return BindingType::kSomeIp;
    }

    void SetSkeletonEventTracingData(impl::tracing::SkeletonEventTracingData tracing_data) noexcept override;

    Result<void> Notify() noexcept override;

    Result<void> SetReceiveHandlerRegistrationChangedHandler(
        ReceiveHandlerRegistrationChangedCallback callback) noexcept override;

    Result<void> UnsetReceiveHandlerRegistrationChangedHandler() noexcept override;

    /// \brief Returns a slot which was handed out by Allocate() but never sent.
    /// \details Called by someip::SampleAllocateePtr when it is destroyed without a preceding Send().
    void DiscardSlot(SlotIndexType slot_index) noexcept override;

  private:
    /// \brief Claims a free slot.
    /// \return The index of the claimed slot, or an empty optional if all slots are currently in use.
    std::optional<SlotIndexType> AllocateSlot() noexcept;

    Skeleton& parent_;
    std::string_view event_name_;
    ElementFqId element_fq_id_;
    EventDataStorage* event_data_storage_;
    memory::DataTypeSizeInfo event_sample_size_info_;
    SkeletonEventProperties event_properties_;

    /// \brief Per slot "currently handed out to the user" flag.
    /// \details This is the SOME/IP counterpart of LoLa's EventDataControl. It can be this much simpler because the
    ///          slots never leave the process: there is no cross-process reference counting and no partial-restart
    ///          rollback to perform.
    std::vector<bool> slot_in_use_;

    bool is_offered_;
    impl::tracing::SkeletonEventTracingData tracing_data_;
    std::optional<ReceiveHandlerRegistrationChangedCallback> receive_handler_registration_changed_callback_;
};

}  // namespace score::mw::com::impl::someip

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SKELETON_EVENT_H
