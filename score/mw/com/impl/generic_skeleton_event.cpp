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
#include "score/mw/com/impl/generic_skeleton_event.h"
#include "score/mw/com/impl/generic_skeleton_event_binding.h"
#include "score/mw/com/impl/tracing/skeleton_event_tracing.h"
#include "score/mw/com/impl/skeleton_base.h"

#include <functional>

namespace score::mw::com::impl
{

GenericSkeletonEvent::GenericSkeletonEvent(SkeletonBase& skeleton_base,
                                           const std::string_view event_name,
                                           std::unique_ptr<GenericSkeletonEventBinding> binding)
    : SkeletonEventBase(skeleton_base, event_name, std::move(binding))
{
    SkeletonBaseView{skeleton_base}.RegisterEvent(event_name, *this);

    if (binding_ != nullptr)
    {
        const SkeletonBaseView skeleton_base_view{skeleton_base};
        const auto& instance_identifier = skeleton_base_view.GetAssociatedInstanceIdentifier();
        const auto binding_type = binding_->GetBindingType();
        auto tracing_data =
            tracing::GenerateSkeletonTracingStructFromEventConfig(instance_identifier, binding_type, event_name);
        binding_->SetSkeletonEventTracingData(tracing_data);
    }
}

Result<score::Blank> GenericSkeletonEvent::Send(SampleAllocateePtr<void> sample) noexcept
{
    if (!service_offered_flag_.IsSet())
    {
        score::mw::log::LogError("lola")
            << "GenericSkeletonEvent::Send failed as Event has not yet been offered or has been stop offered";
        return MakeUnexpected(ComErrc::kNotOffered);
    }

    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(binding_ != nullptr, "Binding is not initialized!");
    auto* const binding = static_cast<GenericSkeletonEventBinding*>(binding_.get());
    

    const auto send_result = binding->Send(std::move(sample));

    if (!send_result.has_value())
    {
        score::mw::log::LogError("lola") << "GenericSkeletonEvent::Send failed: " << send_result.error().Message()
                                       << ": " << send_result.error().UserMessage();
        return MakeUnexpected(ComErrc::kBindingFailure);
    }
    return send_result;
}

Result<SampleAllocateePtr<void>> GenericSkeletonEvent::Allocate() noexcept
{
    if (!service_offered_flag_.IsSet())
    {
        score::mw::log::LogError("lola")
            << "GenericSkeletonEvent::Allocate failed as Event has not yet been offered or has been stop offered";
        return MakeUnexpected(ComErrc::kNotOffered);
    }
    auto* const binding = static_cast<GenericSkeletonEventBinding*>(binding_.get());
    

    auto result = binding->Allocate(); 

    if (!result.has_value())
    {
        score::mw::log::LogError("lola") << "SkeletonEvent::Allocate failed: " << result.error().Message()
                                       << ": " << result.error().UserMessage();    

        return MakeUnexpected<SampleAllocateePtr<void>>(ComErrc::kSampleAllocationFailure);
    }
    return result;
}

DataTypeMetaInfo GenericSkeletonEvent::GetSizeInfo() const noexcept
{
    const auto* const binding = static_cast<const GenericSkeletonEventBinding*>(binding_.get());
    const auto size_info_pair = binding->GetSizeInfo();
    return {size_info_pair.first, size_info_pair.second};
}

} // namespace score::mw::com::impl