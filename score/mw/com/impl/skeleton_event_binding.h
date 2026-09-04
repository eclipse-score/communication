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
#ifndef SCORE_MW_COM_IMPL_SKELETON_EVENT_BINDING_H
#define SCORE_MW_COM_IMPL_SKELETON_EVENT_BINDING_H

#include "score/mw/com/impl/binding_type.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/initialize_sample_callback.h"
#include "score/mw/com/impl/plumbing/sample_allocatee_ptr.h"
#include "score/mw/com/impl/plumbing/sample_ptr.h"
#include "score/mw/com/impl/receive_handler_registration_changed_handler.h"
#include "score/mw/com/impl/sample_allocatee_guard.h"
#include "score/mw/com/impl/tracing/skeleton_event_tracing_data.h"

#include "score/memory/data_type_size_info.h"
#include "score/result/result.h"

#include <score/callback.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>

namespace score::mw::com::impl
{

// Will come from plumbing
template <typename SampleType>
class SampleAllocateePtr;

/// \brief The SkeletonEventBinding represents the interface that _every_ binding has to provide, if it wants to support
/// events. It will be used by a concrete SkeletonEvent to perform any binding specific operation.
class SkeletonEventBinding
{
  public:
    virtual ~SkeletonEventBinding() = default;
    using SubscribeTraceCallback = score::cpp::callback<void(std::size_t, bool), 64U>;
    using UnsubscribeTraceCallback = score::cpp::callback<void(), 64U>;
    using SendTraceCallback = score::cpp::callback<void(SampleAllocateePtr<void>&), 64U>;

    SkeletonEventBinding() = default;
    // A SkeletonEventBinding is always held via a pointer in the binding independent impl::SkeletonEvent.
    // Therefore, the binding itself doesn't have to be moveable or copyable, as the pointer can simply be copied when
    // moving the impl::SkeletonEvent.
    SkeletonEventBinding(const SkeletonEventBinding&) = delete;
    SkeletonEventBinding(SkeletonEventBinding&&) noexcept = delete;
    SkeletonEventBinding& operator=(const SkeletonEventBinding&) & = delete;
    SkeletonEventBinding& operator=(SkeletonEventBinding&&) & noexcept = delete;

    /// \brief SampleType is allocated by the user and provided to the middleware to send
    /// \return On failure, returns an error code.
    virtual Result<void> Send(const void*, std::optional<SendTraceCallback>, SampleAllocateeGuard) noexcept = 0;

    /// \brief SampleType is previously allocated by middleware and provided by the user to indicate that he is finished
    /// filling the provided pointer with live.
    /// \return On failure, returns an error code.
    virtual Result<void> Send(SampleAllocateePtr<void>, std::optional<SendTraceCallback>) noexcept = 0;

    /// \brief Allocates memory for SampleType for the user to fill it. This is especially necessary for Zero-Copy
    /// implementations.
    virtual Result<SampleAllocateePtr<void>> Allocate(SampleAllocateeGuard guard) noexcept = 0;

    /// \brief Retrieves the latest sample, intended to support the getter of a SkeletonField.
    virtual Result<SamplePtr<void>> GetLatestSample(QualityType quality_type) = 0;

    /// \brief Used to indicate that the event shall be available to consumer (e.g. binding specific preparation)
    /// \details Method for binding specific preparations for the offer of an event. The initialize_sample_callback is
    /// handed over from the binding independent (strongly typed) layer, in case the binding layer needs to initialize
    /// binding specific (type erased) storage. As only the binding independent layer has strong type knowledge, it is
    /// the one to provide such an initializer.
    /// The callback is optional: callers that don't have type knowledge (e.g. GenericSkeletonEvent, which is
    /// already type-erased on the binding independent layer) or that don't need type-correct initialization hand over
    /// an empty optional. In that case, the binding shall skip initializing the type-erased storage. The callback (if
    /// any) is taken by const reference (instead of by value) and must only be invoked synchronously within
    /// PrepareOffer(), i.e. it must not be stored/moved for later use. This allows the caller (SkeletonEventBase) to
    /// keep ownership of the callback and reuse it across multiple PrepareOffer() calls over the lifetime of the event
    /// (e.g. offer -> stop-offer -> offer again). \param initialize_sample_callback Optional callback to initialize a
    /// sample in the underlying type_erased storage
    virtual Result<void> PrepareOffer(
        const std::optional<InitializeSampleCallback>& initialize_sample_callback) noexcept = 0;

    /// \brief Used to indicate that the event shall no longer be available to consumer (e.g. binding specific
    /// de-initialization)
    virtual void PrepareStopOffer() noexcept = 0;

    /// \brief Get size for the underlying event-type (including possible dynamic memory allocations) and its alignment
    virtual memory::DataTypeSizeInfo GetSizeInfo() const noexcept = 0;

    /// \brief Gets the binding type of the binding
    virtual BindingType GetBindingType() const noexcept = 0;

    /// \todo To be removed in Ticket-134850
    virtual void SetSkeletonEventTracingData(impl::tracing::SkeletonEventTracingData tracing_data) noexcept = 0;

    /// \brief Trigger notification of potential registered receive handlers.
    /// \details This is a specific API for the gateway use-case!
    virtual Result<void> Notify() noexcept = 0;

    /// \brief Sets a callback that will be called when the first ReceiveHandler of a GenericEvent
    /// will get registered or the last ReceiveHandler will be removed.
    /// \details This is a specific API for the gateway use-case!
    virtual Result<void> SetReceiveHandlerRegistrationChangedHandler(
        ReceiveHandlerRegistrationChangedCallback callback) noexcept = 0;

    virtual Result<void> UnsetReceiveHandlerRegistrationChangedHandler() noexcept = 0;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_SKELETON_EVENT_BINDING_H
