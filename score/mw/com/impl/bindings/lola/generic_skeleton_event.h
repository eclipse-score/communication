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
#include "score/mw/com/impl/data_type_meta_info.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event_properties.h" 
#include "score/mw/com/impl/bindings/lola/skeleton_event_common.h" 


namespace score::mw::com::impl::lola
{

class TransactionLogRegistrationGuard;
class Skeleton;

/// @brief The LoLa binding implementation for a generic skeleton event.
class GenericSkeletonEvent : public GenericSkeletonEventBinding
{
  public:

    GenericSkeletonEvent(Skeleton& parent,
                         const SkeletonEventProperties& event_properties, 
                         const ElementFqId& event_fqn,                         
                         const DataTypeMetaInfo& size_info,
                         impl::tracing::SkeletonEventTracingData tracing_data = {});
 
    Result<score::Blank> Send(score::mw::com::impl::SampleAllocateePtr<void> sample) noexcept override;

    Result<score::mw::com::impl::SampleAllocateePtr<void>> Allocate() noexcept override;

    std::pair<size_t, size_t> GetSizeInfo() const noexcept override;

    
    ResultBlank PrepareOffer() noexcept override;
    void PrepareStopOffer() noexcept override;
    BindingType GetBindingType() const noexcept override;
    void SetSkeletonEventTracingData(impl::tracing::SkeletonEventTracingData tracing_data) noexcept override
    {
        event_shared_impl_.GetTracingData() = tracing_data;
    }

    std::size_t GetMaxSize() const noexcept override
    {
        return size_info_.size;
    }

  private:
    DataTypeMetaInfo size_info_;
    const SkeletonEventProperties event_properties_;
    score::cpp::optional<EventDataControlComposite> control_{};
    EventSlotStatus::EventTimeStamp current_timestamp_{1U};
    score::memory::shared::OffsetPtr<void> data_storage_{nullptr};
    bool qm_disconnect_{false};
    
    SkeletonEventCommon event_shared_impl_; 
    
};

} // namespace score::mw::com::impl::lola