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
#include "score/memory/shared/pointer_arithmetic_util.h"
#include "score/mw/com/impl/bindings/lola/skeleton.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event_properties.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/impl/skeleton_event_binding.h"

namespace score::mw::com::impl::lola
{
GenericSkeletonEvent::GenericSkeletonEvent(Skeleton& parent,
                                           const SkeletonEventProperties& event_properties,
                                           const ElementFqId& event_fqn,
                                           const DataTypeMetaInfo& size_info,
                                           impl::tracing::SkeletonEventTracingData tracing_data)
    : size_info_(size_info),
      event_shared_impl_(parent,
                         event_properties,
                         event_fqn,
                         event_data_control_composite_,
                         current_timestamp_,
                         tracing_data)
{
}

ResultBlank GenericSkeletonEvent::PrepareOffer() noexcept
{
    const auto registration_result =
        event_shared_impl_.GetParent().RegisterGeneric(event_shared_impl_.GetElementFQId(),
                                                       event_shared_impl_.GetEventProperties(),
                                                       size_info_.size,
                                                       size_info_.alignment);

    event_data_storage_ = static_cast<std::uint8_t*>(registration_result.type_erased_event_data_storage_ptr);
    event_data_control_composite_ = registration_result.event_data_control_composite;

    event_shared_impl_.PrepareOfferCommon();

    return {};
}

ResultBlank GenericSkeletonEvent::Send(score::mw::com::impl::SampleAllocateePtr<void> sample) noexcept
{
    return event_shared_impl_.Send(sample);
}

Result<score::mw::com::impl::SampleAllocateePtr<void>> GenericSkeletonEvent::Allocate() noexcept
{
    auto allocated_slot_result = event_shared_impl_.AllocateSlot();
    if (!allocated_slot_result.has_value())
    {
        return MakeUnexpected<impl::SampleAllocateePtr<void>>(allocated_slot_result.error());
    }

    const auto slot_index = allocated_slot_result.value();

    // Calculate the exact slot spacing based on alignment padding
    const auto aligned_size = memory::shared::CalculateAlignedSize(size_info_.size, size_info_.alignment);
    std::size_t offset = static_cast<std::size_t>(slot_index) * aligned_size;
    void* data_ptr = static_cast<void*>(memory::shared::AddOffsetToPointer(event_data_storage_, offset));

    auto lola_ptr = lola::SampleAllocateePtr<void>(data_ptr, event_data_control_composite_.value(), slot_index);
    return impl::MakeSampleAllocateePtr(std::move(lola_ptr));
}

std::pair<size_t, size_t> GenericSkeletonEvent::GetSizeInfo() const noexcept
{
    return {size_info_.size, size_info_.alignment};
}

void GenericSkeletonEvent::PrepareStopOffer() noexcept
{
    event_shared_impl_.PrepareStopOfferCommon();
    event_data_control_composite_.reset();
    event_data_storage_ = nullptr;
}

BindingType GenericSkeletonEvent::GetBindingType() const noexcept
{
    return BindingType::kLoLa;
}

}  // namespace score::mw::com::impl::lola
