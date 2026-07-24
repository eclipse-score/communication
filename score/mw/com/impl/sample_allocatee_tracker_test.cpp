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

#include <gtest/gtest.h>

#include <utility>

namespace score::mw::com::impl
{

namespace
{

class SampleAllocateeTrackerTest : public ::testing::Test
{
  protected:
    SampleAllocateeTracker tracker_{};
};

TEST_F(SampleAllocateeTrackerTest, InitialStateHasNoAllocations)
{
    // When constructing a SampleAllocateeTracker
    // Then the number of allocations should be 0
    EXPECT_EQ(tracker_.GetNumAllocated(), 0U);
}

TEST_F(SampleAllocateeTrackerTest, AllocateIncrementsCounter)
{
    // When allocating a sample
    SampleAllocateeGuard guard = tracker_.Allocate();

    // Then the tracker should have one allocation
    EXPECT_EQ(tracker_.GetNumAllocated(), 1U);
}

TEST_F(SampleAllocateeTrackerTest, MultipleAllocationsIncrementCounter)
{
    // When allocating multiple samples
    SampleAllocateeGuard guard1 = tracker_.Allocate();
    SampleAllocateeGuard guard2 = tracker_.Allocate();
    SampleAllocateeGuard guard3 = tracker_.Allocate();

    // Then the counter should reflect all allocations
    EXPECT_EQ(tracker_.GetNumAllocated(), 3U);
}

TEST_F(SampleAllocateeTrackerTest, GuardDestructionDecrementsCounter)
{
    {
        // Given a SampleAllocateeTracker with multiple allocations and then let them go out of scope.
        SampleAllocateeGuard guard1 = tracker_.Allocate();
        SampleAllocateeGuard guard2 = tracker_.Allocate();
        EXPECT_EQ(tracker_.GetNumAllocated(), 2U);
    }

    // When the guards go out of scope
    // Then the counter should return to zero
    EXPECT_EQ(tracker_.GetNumAllocated(), 0U);
}

TEST_F(SampleAllocateeTrackerTest, PartialGuardDeallocation)
{
    // Given a SampleAllocateeTracker with one allocation
    SampleAllocateeGuard guard1 = tracker_.Allocate();

    {
        // Create additional two guards and then let them go out of scope, leaving guard1 still alive
        SampleAllocateeGuard guard2 = tracker_.Allocate();
        SampleAllocateeGuard guard3 = tracker_.Allocate();
        EXPECT_EQ(tracker_.GetNumAllocated(), 3U);
    }

    // When some guards are destroyed but not all
    // Then the counter should reflect remaining allocations
    EXPECT_EQ(tracker_.GetNumAllocated(), 1U);
}

TEST(SampleAllocateeTrackerDeathTest, TerminatesWhenDestroyedWithOutstandingAllocations)
{
    // Given a SampleAllocateeTracker with an outstanding guard
    auto tracker = std::make_unique<SampleAllocateeTracker>();
    SampleAllocateeGuard guard = tracker->Allocate();
    EXPECT_EQ(tracker->GetNumAllocated(), 1U);

    // When destroying the tracker while the guard is still alive
    // Then the application should terminate
    EXPECT_DEATH(tracker.reset(), "");
}

}  // namespace

}  // namespace score::mw::com::impl
