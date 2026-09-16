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
#include "score/mw/com/impl/bindings/lola/sample_allocatee_ptr.h"

#include <score/assert.hpp>

#include <utility>

namespace score::mw::com::impl::lola
{

SampleAllocateePtr::SampleAllocateePtr(std::nullptr_t /* ptr */) noexcept
    : managed_object_{nullptr},
      event_slot_index_{kUninitialisedEventSlotIndex},
      event_data_control_ptr_{nullptr},
      consumer_event_data_control_local_view_{nullptr}
{
}

SampleAllocateePtr::SampleAllocateePtr(pointer ptr,
                                       EventDataControlComposite<>& event_data_ctrl,
                                       ConsumerEventDataControlLocalView<>& consumer_event_data_control_local_view,
                                       const SlotIndexType slot_index) noexcept
    : managed_object_{ptr},
      event_slot_index_{slot_index},
      event_data_control_ptr_{&event_data_ctrl},
      consumer_event_data_control_local_view_{&consumer_event_data_control_local_view}
{
}

SampleAllocateePtr::SampleAllocateePtr(SampleAllocateePtr&& other) noexcept : SampleAllocateePtr()
{
    this->swap(other);
}

SampleAllocateePtr::~SampleAllocateePtr() noexcept
{
    internal_delete();
}

void SampleAllocateePtr::reset() noexcept
{
    internal_delete();
}

void SampleAllocateePtr::swap(SampleAllocateePtr& other) noexcept
{
    // Search for custom swap functions via ADL, and use std::swap if none are found.
    using std::swap;

    swap(this->managed_object_, other.managed_object_);
    swap(this->event_slot_index_, other.event_slot_index_);
    swap(this->event_data_control_ptr_, other.event_data_control_ptr_);
    swap(this->consumer_event_data_control_local_view_, other.consumer_event_data_control_local_view_);
}

SampleAllocateePtr& SampleAllocateePtr::operator=(std::nullptr_t /* ptr */) & noexcept
{
    internal_delete();
    return *this;
}

SampleAllocateePtr& SampleAllocateePtr::operator=(SampleAllocateePtr&& other) & noexcept
{
    this->swap(other);
    return *this;
}

void SampleAllocateePtr::internal_delete()
{
    managed_object_ = nullptr;
    if (event_slot_index_ < kUninitialisedEventSlotIndex)
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
            event_data_control_ptr_ != nullptr,
            "The only time that event_data_control_ptr_ is nullptr is if the SampleAllocateePtr is default "
            "initialised or initialised with a nullptr. In both these, cases event_slot_index_ == "
            "kUninitialisedEventSlotIndex so we will never enter this branch.");
        event_data_control_ptr_->Discard(event_slot_index_);
        event_slot_index_ = kUninitialisedEventSlotIndex;
    }
}

void swap(SampleAllocateePtr& lhs, SampleAllocateePtr& rhs) noexcept
{
    lhs.swap(rhs);
}

const EventDataControlComposite<>& SampleAllocateePtrView::GetEventDataControlComposite() const noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(ptr_.event_data_control_ptr_ != nullptr);
    return *ptr_.event_data_control_ptr_;
}

typename SampleAllocateePtr::pointer SampleAllocateePtrView::GetManagedObject() const noexcept
{
    return ptr_.managed_object_;
}

EventDataControlComposite<>& SampleAllocateePtrMutableView::GetEventDataControlComposite() const noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(ptr_.event_data_control_ptr_ != nullptr);
    return *ptr_.event_data_control_ptr_;
}

ConsumerEventDataControlLocalView<>& SampleAllocateePtrMutableView::GetConsumerEventDataControlLocalView()
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(ptr_.consumer_event_data_control_local_view_ != nullptr);
    return *ptr_.consumer_event_data_control_local_view_;
}

}  // namespace score::mw::com::impl::lola
