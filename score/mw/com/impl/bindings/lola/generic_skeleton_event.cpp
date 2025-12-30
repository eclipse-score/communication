/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional information
 * regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/
#include "score/mw/com/impl/bindings/lola/generic_skeleton_event.h"
#include "score/mw/com/impl/bindings/lola/skeleton.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event_properties.h"

namespace score::mw::com::impl::lola
{
GenericSkeletonEvent::GenericSkeletonEvent(Skeleton& parent,
                                             const SkeletonEventProperties& event_properties,
                                             const ElementFqId& event_fqn,
                                             const SizeInfo& size_info)
    : parent_(parent), size_info_(size_info), event_properties_(event_properties), event_fqn_(event_fqn)
{

}

ResultBlank GenericSkeletonEvent::PrepareOffer() noexcept
{
    auto [data_storage, control_composite] =
        parent_.RegisterGeneric(event_fqn_, event_properties_, size_info_.size, size_info_.alignment);

    data_storage_ = data_storage;
    control_.emplace(std::move(control_composite));

    return {};
}

Result<score::Blank> GenericSkeletonEvent::Send(const void* /*data*/) noexcept
{
 
    return MakeUnexpected(ComErrc::kIllegalUseOfAllocate);
}

Result<score::Blank> GenericSkeletonEvent::Send(lola::ControlSlotCompositeIndicator control_slot_indicator) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(control_.has_value());
    control_.value().EventReady(control_slot_indicator, ++current_timestamp_);
    return {};
}

Result<std::pair<void*, lola::ControlSlotCompositeIndicator>> GenericSkeletonEvent::Allocate() noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(control_.has_value());
    auto slot = control_.value().AllocateNextSlot();

    if (!slot.IsValidQM() && !slot.IsValidAsilB())
    {
        return MakeUnexpected<std::pair<void*, lola::ControlSlotCompositeIndicator>>(
            ComErrc::kSampleAllocationFailure);
    }

    
    auto* data_storage = data_storage_.get<EventDataStorage<std::uint8_t>>();
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(data_storage != nullptr);
    auto* data_base_ptr = data_storage->data();
    void* data_ptr = data_base_ptr + (slot.GetIndex() * size_info_.size);
    return std::make_pair(data_ptr, slot);
}

void GenericSkeletonEvent::Deallocate(lola::ControlSlotCompositeIndicator control_slot_indicator) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(control_.has_value());
    control_.value().Discard(control_slot_indicator);
}

std::pair<size_t, size_t> GenericSkeletonEvent::GetSizeInfo() const noexcept
{
    return {size_info_.size, size_info_.alignment};
}

void GenericSkeletonEvent::PrepareStopOffer() noexcept
{
    control_.reset();
    data_storage_ = nullptr;
}

BindingType GenericSkeletonEvent::GetBindingType() const noexcept
{
    return BindingType::kLoLa;
}

void GenericSkeletonEvent::SetSkeletonEventTracingData(impl::tracing::SkeletonEventTracingData tracing_data) noexcept
{
    tracing_data_ = tracing_data;
}

} // namespace score::mw::com::impl::lola