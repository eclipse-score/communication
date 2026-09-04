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
#include "score/mw/com/impl/plumbing/sample_ptr.h"
#include "score/mw/com/impl/skeleton_event_binding.h"

#include <gmock/gmock.h>

#include <memory>

namespace score::mw::com::impl::mock_binding
{

class SkeletonEvent : public SkeletonEventBinding
{
  public:
    MOCK_METHOD(Result<void>,
                Send,
                (const void* value, std::optional<SendTraceCallback>, SampleAllocateeGuard),
                (noexcept, override));
    MOCK_METHOD(Result<void>,
                Send,
                (score::mw::com::impl::SampleAllocateePtr<void> sample, std::optional<SendTraceCallback>),
                (noexcept, override));
    MOCK_METHOD(Result<score::mw::com::impl::SampleAllocateePtr<void>>,
                Allocate,
                (SampleAllocateeGuard),
                (noexcept, override));
    MOCK_METHOD(Result<score::mw::com::impl::SamplePtr<void>>, GetLatestSample, (QualityType), (override));
    MOCK_METHOD(Result<void>,
                PrepareOffer,
                (const std::optional<impl::InitializeSampleCallback>&),
                (noexcept, override));
    MOCK_METHOD(void, PrepareStopOffer, (), (noexcept, override));
    MOCK_METHOD(memory::DataTypeSizeInfo, GetSizeInfo, (), (const, noexcept, override));
    MOCK_METHOD(BindingType, GetBindingType, (), (const, noexcept, override));
    MOCK_METHOD(void, SetSkeletonEventTracingData, (impl::tracing::SkeletonEventTracingData), (noexcept, override));
    MOCK_METHOD(Result<void>, Notify, (), (noexcept, override));
    MOCK_METHOD(Result<void>,
                SetReceiveHandlerRegistrationChangedHandler,
                (ReceiveHandlerRegistrationChangedCallback),
                (noexcept, override));
    MOCK_METHOD(Result<void>, UnsetReceiveHandlerRegistrationChangedHandler, (), (noexcept, override));
};

class SkeletonEventFacade : public SkeletonEventBinding
{
    SkeletonEvent& skeleton_event_;

  public:
    SkeletonEventFacade(SkeletonEvent& skeleton_event) : SkeletonEventBinding{}, skeleton_event_{skeleton_event} {}

    ~SkeletonEventFacade() override = default;
    Result<void> Send(const void* value,
                      std::optional<SendTraceCallback> callback,
                      SampleAllocateeGuard guard) noexcept override
    {
        return skeleton_event_.Send(value, std::move(callback), std::move(guard));
    };
    Result<void> Send(score::mw::com::impl::SampleAllocateePtr<void> sample,
                      std::optional<SendTraceCallback> callback) noexcept override
    {
        return skeleton_event_.Send(std::move(sample), std::move(callback));
    }
    Result<impl::SampleAllocateePtr<void>> Allocate(SampleAllocateeGuard guard) noexcept override
    {
        return skeleton_event_.Allocate(std::move(guard));
    };
    Result<score::mw::com::impl::SamplePtr<void>> GetLatestSample(QualityType quality_type) override
    {
        return skeleton_event_.GetLatestSample(quality_type);
    }
    Result<void> PrepareOffer(
        const std::optional<InitializeSampleCallback>& initialize_sample_callback) noexcept override
    {
        return skeleton_event_.PrepareOffer(initialize_sample_callback);
    }
    void PrepareStopOffer() noexcept override
    {
        return skeleton_event_.PrepareStopOffer();
    }
    memory::DataTypeSizeInfo GetSizeInfo() const noexcept override
    {
        return skeleton_event_.GetSizeInfo();
    }
    BindingType GetBindingType() const noexcept override
    {
        return skeleton_event_.GetBindingType();
    }
    void SetSkeletonEventTracingData(impl::tracing::SkeletonEventTracingData tracing_data) noexcept override
    {
        return skeleton_event_.SetSkeletonEventTracingData(tracing_data);
    }
    Result<void> Notify() noexcept override
    {
        return skeleton_event_.Notify();
    }
    Result<void> SetReceiveHandlerRegistrationChangedHandler(
        ReceiveHandlerRegistrationChangedCallback callback) noexcept override
    {
        return skeleton_event_.SetReceiveHandlerRegistrationChangedHandler(std::move(callback));
    }
    Result<void> UnsetReceiveHandlerRegistrationChangedHandler() noexcept override
    {
        return skeleton_event_.UnsetReceiveHandlerRegistrationChangedHandler();
    }
};
}  // namespace score::mw::com::impl::mock_binding

#endif  // SCORE_MW_COM_IMPL_BINDINGS_MOCK_BINDING_SKELETON_EVENT_H
