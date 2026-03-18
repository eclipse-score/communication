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
#include "score/mw/com/impl/sample_reference_tracker.h"

#include "score/language/safecpp/safe_math/safe_math.h"

#include <score/assert.hpp>
#include <score/utility.hpp>
#include <algorithm>

namespace score::mw::com::impl
{

TrackerGuardFactory makeTrakerGuardFactory(SampleReferenceTracker& tracker,
                                           const std::size_t num_available_guards) noexcept
{
    // NOLINTNEXTLINE(score-no-unnamed-temporary-objects): By returning the unnammed object, we allow for copy-elision.
    return TrackerGuardFactory(tracker, num_available_guards);
}

void deallocateSampleReferenceTracker(SampleReferenceTracker& instance,
                                      const std::size_t num_deallocations = 1U) noexcept
{
    instance.Deallocate(num_deallocations);
}

SampleReferenceGuard makeSampleReferenceGuard(SampleReferenceTracker& tracker) noexcept
{
    return SampleReferenceGuard(tracker);
}

SampleReferenceGuard::SampleReferenceGuard(SampleReferenceTracker& tracker) noexcept : tracker_{&tracker} {}

SampleReferenceGuard::SampleReferenceGuard(SampleReferenceGuard&& other) noexcept : tracker_{other.tracker_}
{
    other.tracker_ = nullptr;
}

SampleReferenceGuard& SampleReferenceGuard::operator=(SampleReferenceGuard&& other) noexcept
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

SampleReferenceGuard::~SampleReferenceGuard() noexcept
{
    Deallocate();
}

void SampleReferenceGuard::Deallocate() noexcept
{
    if (tracker_ != nullptr)
    {
        deallocateSampleReferenceTracker(*tracker_);
        tracker_ = nullptr;
    }
}

TrackerGuardFactory::TrackerGuardFactory(SampleReferenceTracker& tracker,
                                         const std::size_t num_available_guards) noexcept
    : tracker_{tracker}, num_available_guards_{num_available_guards}
{
}

TrackerGuardFactory::TrackerGuardFactory(TrackerGuardFactory&& other) noexcept
    : tracker_{other.tracker_}, num_available_guards_{other.num_available_guards_}
{
    other.num_available_guards_ = 0U;
}

TrackerGuardFactory::~TrackerGuardFactory() noexcept
{
    if (num_available_guards_ > 0U)
    {
        deallocateSampleReferenceTracker(tracker_, num_available_guards_);
    }
}

std::size_t TrackerGuardFactory::GetNumAvailableGuards() const noexcept
{
    return num_available_guards_;
}

std::optional<SampleReferenceGuard> TrackerGuardFactory::TakeGuard() noexcept
{
    // LCOV_EXCL_BR_START (Tool incorrectly marks the branch when the condition is false as not covered. However, the
    // line in that branch is marked as covered indicating that the branch is indeed taken. Suppression can be removed
    // when bug is fixed in Ticket-189467).
    if (num_available_guards_ > 0U)
    {
        // LCOV_EXCL_BR_STOP
        --num_available_guards_;
        return makeSampleReferenceGuard(tracker_);
    }
    else
    {
        return std::nullopt;
    }
}

SampleReferenceTracker::SampleReferenceTracker(const std::size_t max_num_samples) noexcept
    : available_samples_{max_num_samples}, max_num_samples_{max_num_samples}
{
}

std::size_t SampleReferenceTracker::GetNumAvailableSamples() const noexcept
{
    // Using relaxed memory order since you never can be sure anyway in a multi-threaded environment whether the value
    // you just loaded is still valid. Therefore, using a stronger memory order doesn't have any merit.
    return available_samples_.load(std::memory_order_relaxed);
}

bool SampleReferenceTracker::IsUsed() const noexcept
{
    return GetNumAvailableSamples() < max_num_samples_;
}

TrackerGuardFactory SampleReferenceTracker::Allocate(std::size_t num_samples) noexcept
{
    std::size_t num_allocated_samples{0U};
    std::size_t num_samples_available = GetNumAvailableSamples();

    // LCOV_EXCL_BR_START (Tool incorrectly marks the decision as "Decision couldn't be analyzed" despite all branches
    // being marked by the tool as taken. Suppression can be removed when bug is fixed in Ticket-188259).
    while ((num_samples > 0U) && (num_samples_available > 0U))
    {
        // LCOV_EXCL_BR_STOP
        const std::size_t num_samples_to_allocate = std::min(num_samples_available, num_samples);
        // A simple subtract won't be sufficient as some other thread might have acquired some samples as well.
        // Therefore, use compare_exchange_weak to make sure we still have the same number of samples available that
        // we used to calculate the number of needed samples.
        //
        // In case compare_exchange fails we do not end the loop as it could either be a spurious failure or another
        // thread is also acquiring some samples. In that case, we simply retry until we run out of samples or we
        // have all samples we need.
        if (available_samples_.compare_exchange_weak(
                num_samples_available, num_samples_available - num_samples_to_allocate, std::memory_order_acq_rel))
        {
            // Suppress "AUTOSAR C++14 A4-7-1" rule finding. This rule states: "An integer expression shall
            // not lead to data loss.".
            // Addition of num_allocated_samples and num_samples_to_allocate can never overflow! Because
            // num_allocated_samples starts with 0 and gets incremented by num_samples_to_allocate which is also
            // a std::size_t. So this can't overflow! And if we are already in this if-branch, then no further
            // loop-iteration will take place. As you hit the next line, which leads to while-loop condition
            // invalidation.
            // coverity[autosar_cpp14_a4_7_1_violation : FALSE]
            num_allocated_samples += num_samples_to_allocate;
            num_samples -= num_samples_to_allocate;
        }
    }

    return makeTrakerGuardFactory(*this, num_allocated_samples);
}

void SampleReferenceTracker::Reset(const std::size_t max_num_samples) noexcept
{
    available_samples_ = max_num_samples;
    max_num_samples_ = max_num_samples;
}

void SampleReferenceTracker::Deallocate(const std::size_t num_deallocations) noexcept
{
    // fetch_add  doesn't return error.
    score::cpp::ignore = available_samples_.fetch_add(num_deallocations, std::memory_order_relaxed);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
        available_samples_.load() <= max_num_samples_,
        "Available samples is larger than the maximum allowed number of samples.");
}

}  // namespace score::mw::com::impl
