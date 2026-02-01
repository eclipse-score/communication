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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_MOCK_BINDING_SKELETON_EVENT_H
#define SCORE_MW_COM_IMPL_BINDINGS_MOCK_BINDING_SKELETON_EVENT_H

#include "score/mw/com/impl/plumbing/sample_allocatee_ptr.h"
#include "score/mw/com/impl/skeleton_event_binding.h"

#include <gmock/gmock.h>

#include <memory>

namespace score::mw::com::impl::mock_binding
{

class SkeletonEventBase : public SkeletonEventBindingBase
{
  public:
    MOCK_METHOD(ResultBlank, PrepareOffer, (), (noexcept, override));
    MOCK_METHOD(void, PrepareStopOffer, (), (noexcept, override));
    MOCK_METHOD(std::size_t, GetMaxSize, (), (const, noexcept, override));
    MOCK_METHOD(BindingType, GetBindingType, (), (const, noexcept, override));
    MOCK_METHOD(void, SetSkeletonEventTracingData, (impl::tracing::SkeletonEventTracingData), (noexcept, override));
};

template <typename SampleType>
class SkeletonEvent : public SkeletonEventBinding<SampleType>
{
  public:
    MOCK_METHOD(ResultBlank,
                Send,
                (const SampleType& value, score::cpp::optional<typename SkeletonEventBinding<SampleType>::SendTraceCallback>),
                (noexcept, override));
    MOCK_METHOD(ResultBlank,
                Send,
                (score::mw::com::impl::SampleAllocateePtr<SampleType> sample,
                 score::cpp::optional<typename SkeletonEventBinding<SampleType>::SendTraceCallback>),
                (noexcept, override));
    MOCK_METHOD(Result<score::mw::com::impl::SampleAllocateePtr<SampleType>>, Allocate, (), (noexcept, override));
    MOCK_METHOD(ResultBlank, PrepareOffer, (), (noexcept, override));
    MOCK_METHOD(void, PrepareStopOffer, (), (noexcept, override));
    MOCK_METHOD(std::size_t, GetMaxSize, (), (const, noexcept, override));
    MOCK_METHOD(BindingType, GetBindingType, (), (const, noexcept, override));
    MOCK_METHOD(void, SetSkeletonEventTracingData, (impl::tracing::SkeletonEventTracingData), (noexcept, override));
};

template <typename SampleType>
class SkeletonEventFacade : public SkeletonEventBinding<SampleType>
{
    SkeletonEvent<SampleType>& skeleton_event_;

  public:
    SkeletonEventFacade(SkeletonEvent<SampleType>& skeleton_event)
        : SkeletonEventBinding<SampleType>{}, skeleton_event_{skeleton_event}
    {
    }

    ~SkeletonEventFacade() override = default;
    ResultBlank Send(
        const SampleType& value,
        score::cpp::optional<typename SkeletonEventBinding<SampleType>::SendTraceCallback> callback) noexcept override
    {
        return skeleton_event_.Send(value, std::move(callback));
    };
    ResultBlank Send(
        score::mw::com::impl::SampleAllocateePtr<SampleType> sample,
        score::cpp::optional<typename SkeletonEventBinding<SampleType>::SendTraceCallback> callback) noexcept override
    {
        return skeleton_event_.Send(std::move(sample), std::move(callback));
    }
    Result<score::mw::com::impl::SampleAllocateePtr<SampleType>> Allocate() noexcept override
    {
        return skeleton_event_.Allocate();
    };
    ResultBlank PrepareOffer() noexcept override
    {
        return skeleton_event_.PrepareOffer();
    }
    void PrepareStopOffer() noexcept override
    {
        return skeleton_event_.PrepareStopOffer();
    }
    std::size_t GetMaxSize() const noexcept override
    {
        return skeleton_event_.GetMaxSize();
    }
    BindingType GetBindingType() const noexcept override
    {
        return skeleton_event_.GetBindingType();
    }
    void SetSkeletonEventTracingData(impl::tracing::SkeletonEventTracingData tracing_data) noexcept override
    {
        return skeleton_event_.SetSkeletonEventTracingData(tracing_data);
    }
};
}  // namespace score::mw::com::impl::mock_binding

#endif  // SCORE_MW_COM_IMPL_BINDINGS_MOCK_BINDING_SKELETON_EVENT_H
