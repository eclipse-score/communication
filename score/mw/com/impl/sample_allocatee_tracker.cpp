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
#include "score/mw/com/impl/sample_allocatee_tracker.h"

#include "score/mw/log/logging.h"

#include <score/assert.hpp>
#include <score/utility.hpp>

#include <cstdlib>

namespace score::mw::com::impl
{
SampleAllocateeTracker::~SampleAllocateeTracker() noexcept
{
    const std::size_t num_allocated = GetNumAllocated();
    if (num_allocated != 0U)
    {
        score::mw::log::LogFatal("lola") << "SampleAllocateeTracker destroyed while still holding" << num_allocated
                                         << "outstanding SampleAllocateePtr instance(s), terminating.";
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
    }
}

[[nodiscard]] SampleAllocateeGuard SampleAllocateeTracker::Allocate()
{
    score::cpp::ignore = allocated_count_.fetch_add(1U);
    return SampleAllocateeGuard{*this};
}

std::size_t SampleAllocateeTracker::GetNumAllocated() const
{
    return allocated_count_.load();
}

void SampleAllocateeTracker::Deallocate()
{
    const std::size_t previous = allocated_count_.fetch_sub(1U);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(previous > 0U, "Deallocating more samples than were allocated.");
}

}  // namespace score::mw::com::impl
