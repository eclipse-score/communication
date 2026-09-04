/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SKELETON_EVENT_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SKELETON_EVENT_H

#include "score/mw/com/impl/bindings/someip/skeleton.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/configuration/someip_service_type_deployment.h"
#include "score/mw/com/impl/skeleton_event_binding.h"

#include <string>
#include <string_view>

namespace score::mw::com::impl::someip
{

/// \brief Skeleton side event binding of the SOME/IP technical binding.
///
/// Created by the binding independent SkeletonEventBindingFactoryImpl, which resolves the wire event id from the
/// SOME/IP service type deployment and passes the already created someip::Skeleton as parent.
template <typename SampleType>
class SkeletonEvent final : public SkeletonEventBinding<SampleType>
{
  public:
    SkeletonEvent(Skeleton& parent, const SomeIpEventId event_id, const std::string_view event_name) noexcept
        : SkeletonEventBinding<SampleType>{}, parent_{parent}, event_id_{event_id}, event_name_{event_name}
    {
    }

    const Skeleton& GetParent() const noexcept
    {
        return parent_;
    }

    SomeIpEventId GetEventId() const noexcept
    {
        return event_id_;
    }

    std::string_view GetEventName() const noexcept
    {
        return event_name_;
    }

    Result<void> PrepareOffer() noexcept override
    {
        return {};
    }
    void PrepareStopOffer() noexcept override {}
    BindingType GetBindingType() const noexcept override
    {
        return BindingType::kSomeIp;
    }
    void SetSkeletonEventTracingData(impl::tracing::SkeletonEventTracingData) noexcept override {}
    Result<void> Send(const SampleType&,
                      std::optional<typename SkeletonEventBinding<SampleType>::SendTraceCallback>,
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

  private:
    Skeleton& parent_;
    SomeIpEventId event_id_;
    std::string event_name_;
};

}  // namespace score::mw::com::impl::someip

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SKELETON_EVENT_H
