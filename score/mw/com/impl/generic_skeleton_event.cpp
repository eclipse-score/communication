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
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/skeleton_base.h"
#include "score/mw/com/impl/skeleton_event_binding.h"
#include "score/mw/com/impl/tracing/skeleton_event_tracing.h"

#include <functional>
#include <optional>

namespace score::mw::com::impl
{

GenericSkeletonEvent::GenericSkeletonEvent(SkeletonBase& skeleton_base,
                                           const std::string_view event_name,
                                           std::unique_ptr<SkeletonEventBinding> binding)
    // GenericSkeletonEvent is already type-erased on the binding independent layer (it has no concrete SampleType),
    // so it cannot provide a type-correct InitializeSampleCallback and hands over an empty optional instead.
    : SkeletonEventBase(event_name, std::nullopt, std::move(binding))
{
    SkeletonBaseView{skeleton_base}.RegisterEvent(event_name, GetReferenceToMoveable());

    if (binding_ != nullptr)
    {
        const SkeletonBaseView skeleton_base_view{skeleton_base};
        const auto& instance_identifier = skeleton_base_view.GetAssociatedInstanceIdentifier();
        const auto binding_type = binding_->GetBindingType();
        tracing_data_ =
            tracing::GenerateSkeletonTracingStructFromEventConfig(instance_identifier, binding_type, event_name);
        binding_->SetSkeletonEventTracingData(tracing_data_);
    }
}

Result<void> GenericSkeletonEvent::Send(SampleAllocateePtr<void> sample) noexcept
{
    if (!service_offered_flag_.IsSet())
    {
        score::mw::log::LogError("lola")
            << "GenericSkeletonEvent::Send failed as Event has not yet been offered or has been stop offered";
        return MakeUnexpected(ComErrc::kNotOffered);
    }

    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(binding_ != nullptr, "Binding is not initialized!");
    auto* const binding = binding_.get();

    // For generic skeletons/skeleton events we do not support (yet) tracing -> SendTraceXCallback is a nullopt!
    const auto send_result = binding->Send(std::move(sample), std::nullopt);

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

    auto result = binding_->Allocate(sample_allocatee_tracker_->Allocate());

    if (!result.has_value())
    {
        score::mw::log::LogError("lola") << "SkeletonEvent::Allocate failed: " << result.error().Message() << ": "
                                         << result.error().UserMessage();

        return MakeUnexpected<SampleAllocateePtr<void>>(ComErrc::kSampleAllocationFailure);
    }
    return result;
}

Result<void> GenericSkeletonEvent::Notify() noexcept
{
    if (!service_offered_flag_.IsSet())
    {
        score::mw::log::LogError("lola")
            << "GenericSkeletonEvent::Notify failed as Event has not yet been offered or has been stop offered";
        return MakeUnexpected(ComErrc::kNotOffered);
    }

    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(binding_ != nullptr, "Binding is not initialized!");
    return binding_->Notify();
}

DataTypeMetaInfo GenericSkeletonEvent::GetSizeInfo() const noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(binding_ != nullptr, "Binding is not initialized!");
    const auto data_type_size_info = binding_->GetSizeInfo();
    return {data_type_size_info.Size(), data_type_size_info.Alignment()};
}

Result<void> GenericSkeletonEvent::SetReceiveHandlerRegistrationChangedHandler(
    ReceiveHandlerRegistrationChangedCallback callback) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(binding_ != nullptr, "Binding is not initialized!");
    return binding_->SetReceiveHandlerRegistrationChangedHandler(std::move(callback));
}

Result<void> GenericSkeletonEvent::UnsetReceiveHandlerRegistrationChangedHandler() noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(binding_ != nullptr, "Binding is not initialized!");
    return binding_->UnsetReceiveHandlerRegistrationChangedHandler();
}
}  // namespace score::mw::com::impl
