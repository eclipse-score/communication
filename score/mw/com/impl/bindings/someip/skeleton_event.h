/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SKELETON_EVENT_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SKELETON_EVENT_H

#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/skeleton_event_binding.h"

namespace score::mw::com::impl::someip
{

template <typename SampleType>
class SkeletonEvent final : public SkeletonEventBinding<SampleType>
{
  public:
    Result<void> PrepareOffer() noexcept override { return {}; }
    void PrepareStopOffer() noexcept override {}
    BindingType GetBindingType() const noexcept override { return BindingType::kSomeIp; }
    void SetSkeletonEventTracingData(impl::tracing::SkeletonEventTracingData) noexcept override {}
    Result<void> Send(const SampleType&, std::optional<typename SkeletonEventBinding<SampleType>::SendTraceCallback>,
                      SampleAllocateeGuard) noexcept override
    {
        return MakeUnexpected(ComErrc::kBindingFailure);
    }
    Result<void> Send(SampleAllocateePtr<SampleType>,
                      std::optional<typename SkeletonEventBinding<SampleType>::SendTraceCallback>) noexcept override
    {
        return MakeUnexpected(ComErrc::kBindingFailure);
    }
    Result<SampleAllocateePtr<SampleType>> Allocate(SampleAllocateeGuard) noexcept override
    {
        return MakeUnexpected<SampleAllocateePtr<SampleType>>(ComErrc::kSampleAllocationFailure);
    }
    Result<SamplePtr<SampleType>> GetLatestSample(QualityType) override
    {
        return MakeUnexpected<SamplePtr<SampleType>>(ComErrc::kBindingFailure);
    }
};

}  // namespace score::mw::com::impl::someip

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SKELETON_EVENT_H
