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

#include <functional>

namespace score::mw::com::impl
{

GenericSkeletonEvent::GenericSkeletonEvent(SkeletonBase& skeleton_base,
                                             const std::string_view event_name,
                                             std::unique_ptr<GenericSkeletonEventBinding> binding)
    : SkeletonEventBase(skeleton_base, event_name, std::move(binding))
{
}

Result<score::Blank> GenericSkeletonEvent::Send(SampleAllocateePtr<void> sample) noexcept
{
    auto* const binding = static_cast<GenericSkeletonEventBinding*>(binding_.get());
    std::cout<<"auto* const binding = static_cast<GenericSkeletonEventBinding*>(binding_.get());"<<std::endl;
    auto* const generic_sample_ptr = SampleAllocateePtrView<void>{sample}.As<GenericEventSamplePtr>();
    std::cout<<"auto* const generic_sample_ptr = SampleAllocateePtrView<void>{sample}.As<GenericEventSamplePtr>();"<<std::endl;
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(generic_sample_ptr != nullptr);
    auto result = binding->Send(generic_sample_ptr->GetControlSlotIndicator());
    return result;
}

Result<SampleAllocateePtr<void>> GenericSkeletonEvent::Allocate() noexcept
{
    auto* binding = static_cast<GenericSkeletonEventBinding*>(binding_.get());
    auto result = binding->Allocate();
    if (!result.has_value())
    {
        return MakeUnexpected<SampleAllocateePtr<void>>(ComErrc::kSampleAllocationFailure);
    }

    auto deallocator = [binding_ptr = binding_.get()](lola::ControlSlotCompositeIndicator indicator) {
        auto* const generic_binding = static_cast<GenericSkeletonEventBinding*>(binding_ptr);
        generic_binding->Deallocate(indicator);
    };

    return MakeSampleAllocateePtr(
        GenericEventSamplePtr(result.value().first, result.value().second, std::move(deallocator)));
}

SizeInfo GenericSkeletonEvent::GetSizeInfo() const noexcept
{
    const auto* const binding = static_cast<const GenericSkeletonEventBinding*>(binding_.get());
    const auto size_info_pair = binding->GetSizeInfo();
    return {size_info_pair.first, size_info_pair.second};
}

} // namespace score::mw::com::impl