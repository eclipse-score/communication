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
#include "score/mw/com/impl/bindings/someip/sample_allocatee_ptr.h"

#include <score/assert.hpp>

#include <utility>

namespace score::mw::com::impl::someip
{

ISlotOwner::~ISlotOwner() = default;

SampleAllocateePtr::SampleAllocateePtr(std::nullptr_t /* ptr */) noexcept
    : managed_object_{nullptr}, event_slot_index_{kUninitialisedEventSlotIndex}, owning_event_{nullptr}
{
}

SampleAllocateePtr::SampleAllocateePtr(pointer ptr, ISlotOwner& owning_event, const SlotIndexType slot_index) noexcept
    : managed_object_{ptr}, event_slot_index_{slot_index}, owning_event_{&owning_event}
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
    swap(this->owning_event_, other.owning_event_);
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

void SampleAllocateePtr::internal_delete() noexcept
{
    managed_object_ = nullptr;
    if (event_slot_index_ < kUninitialisedEventSlotIndex)
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
            owning_event_ != nullptr,
            "The only time that owning_event_ is nullptr is if the SampleAllocateePtr is default initialised or "
            "initialised with a nullptr. In both these cases event_slot_index_ == kUninitialisedEventSlotIndex so we "
            "will never enter this branch.");
        owning_event_->DiscardSlot(event_slot_index_);
        event_slot_index_ = kUninitialisedEventSlotIndex;
    }
}

void swap(SampleAllocateePtr& lhs, SampleAllocateePtr& rhs) noexcept
{
    lhs.swap(rhs);
}

typename SampleAllocateePtr::pointer SampleAllocateePtrView::GetManagedObject() const noexcept
{
    return ptr_.managed_object_;
}

ISlotOwner& SampleAllocateePtrMutableView::GetOwningEvent() const noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(ptr_.owning_event_ != nullptr);
    return *ptr_.owning_event_;
}

}  // namespace score::mw::com::impl::someip
