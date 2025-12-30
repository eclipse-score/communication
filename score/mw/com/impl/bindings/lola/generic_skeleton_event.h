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
#pragma once

#include "score/mw/com/impl/generic_skeleton_event_binding.h"
#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/event_data_storage.h"
#include "score/mw/com/impl/size_info.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event_properties.h"
#include "score/memory/shared/offset_ptr.h"
#include "score/mw/com/impl/bindings/lola/event_slot_status.h"

#include "score/mw/com/impl/tracing/skeleton_event_tracing_data.h"

namespace score::mw::com::impl::lola
{

class Skeleton;

/// @brief The LoLa binding implementation for a generic skeleton event.
class GenericSkeletonEvent : public GenericSkeletonEventBinding
{
  public:
    GenericSkeletonEvent(Skeleton& parent,
                         const SkeletonEventProperties& event_properties,
                         const ElementFqId& event_fqn,
                         const SizeInfo& size_info);

    Result<score::Blank> Send(const void* data) noexcept override;

    Result<score::Blank> Send(lola::ControlSlotCompositeIndicator control_slot_indicator) noexcept override;

    Result<std::pair<void*, lola::ControlSlotCompositeIndicator>> Allocate() noexcept override;

    void Deallocate(lola::ControlSlotCompositeIndicator control_slot_indicator) noexcept override;

    std::pair<size_t, size_t> GetSizeInfo() const noexcept override;

    
    ResultBlank PrepareOffer() noexcept override;
    void PrepareStopOffer() noexcept override;
    BindingType GetBindingType() const noexcept override;
    void SetSkeletonEventTracingData(impl::tracing::SkeletonEventTracingData tracing_data) noexcept override;

    std::size_t GetMaxSize() const noexcept override
    {
        return size_info_.size;
    }

  private:
    Skeleton& parent_;
    SizeInfo size_info_;
    const SkeletonEventProperties event_properties_;
    const ElementFqId event_fqn_;
    score::cpp::optional<EventDataControlComposite> control_;
    std::atomic<EventSlotStatus::EventTimeStamp> current_timestamp_{0U};
    score::memory::shared::OffsetPtr<void> data_storage_{nullptr};
    impl::tracing::SkeletonEventTracingData tracing_data_{};
};

} // namespace score::mw::com::impl::lola