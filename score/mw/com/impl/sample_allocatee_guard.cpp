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
#include "score/mw/com/impl/sample_allocatee_guard.h"
#include "score/mw/com/impl/sample_allocatee_tracker.h"

#include <type_traits>

namespace score::mw::com::impl
{

// Static assert to ensure SampleAllocateeTracker is not moveable (guards hold pointers to it).
// Must be in the .cpp where SampleAllocateeTracker is a complete type.
static_assert(!std::is_move_assignable_v<SampleAllocateeTracker> &&
                  !std::is_move_constructible_v<SampleAllocateeTracker>,
              "SampleAllocateeTracker must not be movable, otherwise the guard's stored pointer would dangle");
SampleAllocateeGuard::SampleAllocateeGuard(SampleAllocateeTracker& tracker) : tracker_{&tracker} {}

SampleAllocateeGuard::SampleAllocateeGuard(SampleAllocateeGuard&& other) noexcept : tracker_{other.tracker_}
{
    other.tracker_ = nullptr;
}

SampleAllocateeGuard& SampleAllocateeGuard::operator=(SampleAllocateeGuard&& other) noexcept
{
    if (this != &other)
    {
        if (tracker_ != nullptr)
        {
            Deallocate();
        }
        tracker_ = other.tracker_;
        other.tracker_ = nullptr;
    }
    return *this;
}

SampleAllocateeGuard::~SampleAllocateeGuard() noexcept
{
    Deallocate();
}

void SampleAllocateeGuard::Deallocate()
{
    if (tracker_ != nullptr)
    {
        tracker_->Deallocate();
        tracker_ = nullptr;
    }
}

}  // namespace score::mw::com::impl
