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
#include "score/mw/com/impl/plumbing/sample_allocatee_ptr.h"
#include "score/mw/com/impl/tracing/skeleton_event_tracing_data.h"

#include "score/result/result.h"

#include <score/callback.hpp>
#include <score/optional.hpp>

#include <cstddef>
#include <cstdint>

namespace score::mw::com::impl
{

// Will come from plumbing
template <typename SampleType>
class SampleAllocateePtr;

class SkeletonEventBindingBase
{
  public:
    using SubscribeTraceCallback = score::cpp::callback<void(std::size_t, bool), 64U>;
    using UnsubscribeTraceCallback = score::cpp::callback<void(), 64U>;

    SkeletonEventBindingBase() = default;

    // A SkeletonEventBindingBase is always held via a pointer in the binding independent impl::SkeletonEvent.
    // Therefore, the binding itself doesn't have to be moveable or copyable, as the pointer can simply be copied when
    // moving the impl::SkeletonEvent.
    SkeletonEventBindingBase(const SkeletonEventBindingBase&) = delete;
    SkeletonEventBindingBase(SkeletonEventBindingBase&&) noexcept = delete;
    SkeletonEventBindingBase& operator=(const SkeletonEventBindingBase&) & = delete;
    SkeletonEventBindingBase& operator=(SkeletonEventBindingBase&&) & noexcept = delete;

    virtual ~SkeletonEventBindingBase();

    /// \brief Used to indicate that the event shall be available to consumer (e.g. binding specific preparation)
    virtual ResultBlank PrepareOffer() noexcept = 0;

    /// \brief Used to indicate that the event shall no longer be available to consumer (e.g. binding specific
    /// de-initialization)
    virtual void PrepareStopOffer() noexcept = 0;

    /// \brief Calculate the necessary memory for the underlying event-type (including possible dynamic memory
    /// allocations)
    virtual std::size_t GetMaxSize() const noexcept = 0;

    /// \brief Gets the binding type of the binding
    virtual BindingType GetBindingType() const noexcept = 0;

    /// \todo To be removed in Ticket-134850
    virtual void SetSkeletonEventTracingData(impl::tracing::SkeletonEventTracingData tracing_data) noexcept = 0;
};

/// \brief The SkeletonEventBinding represents the interface that _every_ binding has to provide, if it wants to support
/// events. It will be used by a concrete SkeletonEvent to perform any binding specific operation.
template <typename SampleType>
class SkeletonEventBinding : public SkeletonEventBindingBase
{
  public:
    using SendTraceCallback = score::cpp::callback<void(SampleAllocateePtr<SampleType>&), 64U>;

    /// \brief SampleType is allocated by the user and provided to the middleware to send
    /// \return On failure, returns an error code.
    virtual ResultBlank Send(const SampleType&, score::cpp::optional<SendTraceCallback>) noexcept = 0;

    /// \brief SampleType is previously allocated by middleware and provided by the user to indicate that he is finished
    /// filling the provided pointer with live.
    /// \return On failure, returns an error code.
    virtual ResultBlank Send(SampleAllocateePtr<SampleType>, score::cpp::optional<SendTraceCallback>) noexcept = 0;

    /// \brief Allocates memory for SampleType for the user to fill it. This is especially necessary for Zero-Copy
    /// implementations.
    virtual Result<SampleAllocateePtr<SampleType>> Allocate() noexcept = 0;

    std::size_t GetMaxSize() const noexcept override
    {
        return sizeof(SampleType);
    }
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_SKELETON_EVENT_BINDING_H
