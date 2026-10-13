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
#include "score/mw/com/impl/bindings/someip/skeleton_event.h"

#include "score/mw/com/impl/com_error.h"

#include "score/mw/log/logging.h"

#include <score/utility.hpp>

#include <utility>

namespace score::mw::com::impl::someip
{

SkeletonEvent::SkeletonEvent(Skeleton& parent,
                             const ElementFqId element_fq_id,
                             const std::string_view event_name,
                             const memory::DataTypeSizeInfo size_info,
                             const SkeletonEventProperties properties,
                             impl::tracing::SkeletonEventTracingData skeleton_event_tracing_data) noexcept
    : SkeletonEventBinding{},
      parent_{parent},
      event_name_{event_name},
      element_fq_id_{element_fq_id},
      event_data_storage_{nullptr},
      event_sample_size_info_{size_info},
      event_properties_{properties},
      slot_allocation_control_{},
      is_offered_{false},
      tracing_data_{skeleton_event_tracing_data},
      receive_handler_registration_changed_callback_{}
{
}

Result<void> SkeletonEvent::Send(impl::SampleAllocateePtr<void>, std::optional<SendTraceCallback>) noexcept
{
    return MakeUnexpected(ComErrc::kBindingFailure, "SOME/IP event sending is unsupported without a transport");
}

Result<impl::SampleAllocateePtr<void>> SkeletonEvent::Allocate(SampleAllocateeGuard guard) noexcept
{
    if (event_data_storage_ == nullptr)
    {
        ::score::mw::log::LogError("someip")
            << "SkeletonEvent::Allocate failed as the event has not been offered:" << event_name_;
        return MakeUnexpected(ComErrc::kNotOffered);
    }

    const auto slot_index = slot_allocation_control_.AllocateSlot();
    if (!slot_index.has_value())
    {
        if (!event_properties_.enforce_max_samples)
        {
            ::score::mw::log::LogError("someip")
                << "SkeletonEvent: Allocation of event slot failed. Hint: enforceMaxSamples was "
                   "disabled by config. Might be the root cause!";
        }
        return MakeUnexpected(ComErrc::kBindingFailure);
    }

    return MakeSampleAllocateePtr(
        SampleAllocateePtr(event_data_storage_->GetTypeErasedDataSlot(*slot_index, event_sample_size_info_.Size()),
                           slot_allocation_control_,
                           *slot_index),
        std::move(guard));
}

Result<impl::SamplePtr<void>> SkeletonEvent::GetLatestSample(QualityType quality_type)
{
    score::cpp::ignore = quality_type;
    // TODO(someip-field-getter): Implement SOME/IP field getter support.
    ::score::mw::log::LogError("someip") << "SkeletonEvent::GetLatestSample is not supported by the SOME/IP binding:"
                                         << event_name_;
    return MakeUnexpected(ComErrc::kBindingFailure,
                          "GetLatestSample (field getter) is not supported by the SOME/IP binding");
}

Result<void> SkeletonEvent::PrepareOffer(const std::optional<InitializeSampleCallback>&) noexcept
{
    return MakeUnexpected(ComErrc::kBindingFailure, "SOME/IP event offering is unsupported without a transport");
}

void SkeletonEvent::PrepareStopOffer() noexcept
{
    if (!is_offered_)
    {
        return;
    }
    is_offered_ = false;

    // The EventDataStorage itself stays alive in the parent Skeleton, so that a subsequent offer can reuse it without
    // re-initializing slots which a consumer of the previous offering could still be reading.
    event_data_storage_ = nullptr;
    slot_allocation_control_.Clear();
}

void SkeletonEvent::SetSkeletonEventTracingData(impl::tracing::SkeletonEventTracingData tracing_data) noexcept
{
    tracing_data_ = tracing_data;
}

}  // namespace score::mw::com::impl::someip
