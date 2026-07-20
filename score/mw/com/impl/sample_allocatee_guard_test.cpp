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

class SampleAllocateeGuardTest : public ::testing::Test
{
  protected:
    SampleAllocateeTracker tracker_{};
    SampleAllocateeGuard guard_{tracker_.Allocate()};
};

TEST_F(SampleAllocateeGuardTest, DestructorDecrementsCounter)
{
    EXPECT_EQ(tracker_.GetNumAllocated(), 1U);
    std::optional<SampleAllocateeGuard> guard{std::move(guard_)};

    // When the guard is destroyed
    guard.reset();

    // Then the counter is decremented to 0
    EXPECT_EQ(tracker_.GetNumAllocated(), 0U);
}

TEST_F(SampleAllocateeGuardTest, MoveConstructorDoesNotIncrementCounter)
{
    // Given a SampleAllocateeTracker with one allocation
    EXPECT_EQ(tracker_.GetNumAllocated(), 1U);

    // When move constructing a new guard
    SampleAllocateeGuard guard2{std::move(guard_)};

    // Then the counter should not be decremented
    EXPECT_EQ(tracker_.GetNumAllocated(), 1U);
}

TEST_F(SampleAllocateeGuardTest, DestroyingMovedToGuardDecrementsCounter)
{
    // Given a SampleAllocateeTracker with one allocation
    EXPECT_EQ(tracker_.GetNumAllocated(), 1U);
    std::optional<SampleAllocateeGuard> guard2{std::move(guard_)};
    EXPECT_EQ(tracker_.GetNumAllocated(), 1U);

    // When destroying the moved-to guard
    guard2.reset();

    // Then the counter should decrement to 0
    EXPECT_EQ(tracker_.GetNumAllocated(), 0U);
}

TEST_F(SampleAllocateeGuardTest, DestroyingMovedFromGuardDoesNotDecrementCounter)
{
    // Given a SampleAllocateeTracker with one allocation
    EXPECT_EQ(tracker_.GetNumAllocated(), 1U);
    std::optional<SampleAllocateeGuard> moved_from{std::move(guard_)};

    // When move constructing a second guard from the first
    SampleAllocateeGuard guard2{std::move(moved_from.value())};
    EXPECT_EQ(tracker_.GetNumAllocated(), 1U);

    // When destroying the moved-from guard
    moved_from.reset();

    // Then the counter should not have been decremented
    EXPECT_EQ(tracker_.GetNumAllocated(), 1U);
}

TEST_F(SampleAllocateeGuardTest, MoveAssignmentTransfersOwnership)
{
    // Given a SampleAllocateeTracker with two allocations
    SampleAllocateeGuard guard2 = tracker_.Allocate();
    EXPECT_EQ(tracker_.GetNumAllocated(), 2U);

    // When move assigning one guard to another
    guard_ = std::move(guard2);

    // Then one allocation should be deallocated
    EXPECT_EQ(tracker_.GetNumAllocated(), 1U);
}

TEST_F(SampleAllocateeGuardTest, DestroyingMovedToGuardAfterAssignmentDecrementsCounter)
{
    // Given a SampleAllocateeTracker with two allocations
    std::optional<SampleAllocateeGuard> guard1{std::move(guard_)};
    SampleAllocateeGuard guard2 = tracker_.Allocate();
    EXPECT_EQ(tracker_.GetNumAllocated(), 2U);

    // When move assigning guard2 to guard1
    guard1 = std::move(guard2);
    // guard1's original allocation was released
    EXPECT_EQ(tracker_.GetNumAllocated(), 1U);

    // When destroying the moved-to guard
    guard1.reset();

    // Then the counter should be 0
    EXPECT_EQ(tracker_.GetNumAllocated(), 0U);
}

TEST_F(SampleAllocateeGuardTest, DestroyingMovedFromGuardAfterAssignmentDoesNotDecrementCounter)
{
    // Given a SampleAllocateeTracker with two allocations
    std::optional<SampleAllocateeGuard> guard2{tracker_.Allocate()};
    EXPECT_EQ(tracker_.GetNumAllocated(), 2U);

    // When move assigning guard2 to guard_
    guard_ = std::move(guard2.value());
    // guard_'s original allocation was released
    EXPECT_EQ(tracker_.GetNumAllocated(), 1U);

    // When destroying the moved-from guard2
    guard2.reset();

    // Then the counter should still be 1 (guard2 did not decrement)
    EXPECT_EQ(tracker_.GetNumAllocated(), 1U);
}

TEST_F(SampleAllocateeGuardTest, SelfMoveAssignmentDoesNotDoubleDecrement)
{
    // Given a SampleAllocateeTracker with one allocation
    EXPECT_EQ(tracker_.GetNumAllocated(), 1U);

    // When self move assigning the guard (using reference to avoid clang self-move warning)
    SampleAllocateeGuard& guard_ref = guard_;
    guard_ = std::move(guard_ref);

    // Then the counter should still be 1
    EXPECT_EQ(tracker_.GetNumAllocated(), 1U);
}

}  // namespace

}  // namespace score::mw::com::impl
