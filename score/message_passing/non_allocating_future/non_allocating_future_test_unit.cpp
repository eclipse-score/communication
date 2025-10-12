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
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "score/message_passing/non_allocating_future/non_allocating_future.h"

namespace score::message_passing
{
namespace
{

using namespace ::testing;

class MockMutex
{
  public:
    MOCK_METHOD(void, lock, (), ());
    MOCK_METHOD(void, unlock, (), ());
};

class MockCV
{
  public:
    MOCK_METHOD(void, notify_all, (), ());
    MOCK_METHOD(void, wait, (std::unique_lock<StrictMock<MockMutex>>&, std::function<bool()>), ());
};

class NonAllocatingFutureTestFixture : public ::testing::Test
{
  protected:
    void SetUp() override {}

    void TearDown() override {}

    StrictMock<MockMutex> mutex_;
    StrictMock<MockCV> condition_;
};

TEST_F(NonAllocatingFutureTestFixture, NonSyncFutureMethodsGiveExpectedAccessButDontTriggerSyncMocks)
{
    // strict mocks ensure no unexpected calls

    std::int32_t counter{0};
    detail::NonAllocatingFuture future1{mutex_, condition_};
    detail::NonAllocatingFuture future2{mutex_, condition_, counter};
    (void)future1;
    EXPECT_EQ(&future2.GetValueForUpdate(), &counter);
    EXPECT_EQ(&future2.GetValue(), &counter);

    future2.GetValueForUpdate() = future2.GetValue() + 1;
    EXPECT_EQ(counter, 1);
}

TEST_F(NonAllocatingFutureTestFixture, MarkReadyTriggersNotifySequence)
{
    InSequence is;

    // Ready
    EXPECT_CALL(mutex_, lock());
    EXPECT_CALL(condition_, notify_all());
    EXPECT_CALL(mutex_, unlock());

    detail::NonAllocatingFuture future{mutex_, condition_};
    future.MarkReady();
}

TEST_F(NonAllocatingFutureTestFixture, CopyMarkReadyTriggersNotifySequence)
{
    InSequence is;

    // Ready
    EXPECT_CALL(mutex_, lock()).Times(1);
    EXPECT_CALL(condition_, notify_all()).Times(1);
    EXPECT_CALL(mutex_, unlock()).Times(1);

    std::int32_t counter{0};
    const std::int32_t counter1{1};
    detail::NonAllocatingFuture future{mutex_, condition_, counter};
    future.UpdateValueMarkReady(counter1);
    EXPECT_EQ(counter, 1);
}

TEST_F(NonAllocatingFutureTestFixture, MoveMarkReadyTriggersNotifySequence)
{
    InSequence is;

    // Ready
    EXPECT_CALL(mutex_, lock()).Times(1);
    EXPECT_CALL(condition_, notify_all()).Times(1);
    EXPECT_CALL(mutex_, unlock()).Times(1);

    std::int32_t counter{0};
    const std::int32_t counter1{1};
    detail::NonAllocatingFuture future{mutex_, condition_, counter};
    future.UpdateValueMarkReady(std::move(counter1));
    EXPECT_EQ(counter, 1);
}

TEST_F(NonAllocatingFutureTestFixture, WaitWithoutReadyCallsCvWithCorrectMutexAndFalsePredicate)
{
    InSequence is;

    // Wait Not Ready
    EXPECT_CALL(mutex_, lock()).Times(1);
    EXPECT_CALL(condition_, wait(_, _)).Times(1).WillOnce([this](auto&& lock, auto&& predicate) {
        EXPECT_EQ(lock.mutex(), &mutex_);
        EXPECT_EQ(predicate(), false);
    });
    EXPECT_CALL(mutex_, unlock()).Times(1);

    detail::NonAllocatingFuture future{mutex_, condition_};
    future.Wait();
}

TEST_F(NonAllocatingFutureTestFixture, WaitWithReadyCallsCvWithCorrectMutexAndTruePredicate)
{
    InSequence is;

    // Ready
    EXPECT_CALL(mutex_, lock()).Times(1);
    EXPECT_CALL(condition_, notify_all()).Times(1);
    EXPECT_CALL(mutex_, unlock()).Times(1);

    // Wait Ready
    EXPECT_CALL(mutex_, lock()).Times(1);
    EXPECT_CALL(condition_, wait(_, _)).Times(1).WillOnce([this](auto&& lock, auto&& predicate) {
        EXPECT_EQ(lock.mutex(), &mutex_);
        EXPECT_EQ(predicate(), true);
    });
    EXPECT_CALL(mutex_, unlock()).Times(1);

    detail::NonAllocatingFuture future{mutex_, condition_};
    future.MarkReady();
    future.Wait();
}

TEST_F(NonAllocatingFutureTestFixture, WaitWithChangedReadyCallsCvWithCorrectMutexAndChangedPredicate)
{
    InSequence is;

    // Wait Not Ready
    EXPECT_CALL(mutex_, lock()).Times(1);
    EXPECT_CALL(condition_, wait(_, _)).Times(1).WillOnce([this](auto&& lock, auto&& predicate) {
        EXPECT_EQ(lock.mutex(), &mutex_);
        EXPECT_EQ(predicate(), false);
    });
    EXPECT_CALL(mutex_, unlock()).Times(1);

    // Ready
    EXPECT_CALL(mutex_, lock()).Times(1);
    EXPECT_CALL(condition_, notify_all()).Times(1);
    EXPECT_CALL(mutex_, unlock()).Times(1);

    // Wait Ready
    EXPECT_CALL(mutex_, lock()).Times(1);
    EXPECT_CALL(condition_, wait(_, _)).Times(1).WillOnce([this](auto&& lock, auto&& predicate) {
        EXPECT_EQ(lock.mutex(), &mutex_);
        EXPECT_EQ(predicate(), true);
    });
    EXPECT_CALL(mutex_, unlock()).Times(1);

    detail::NonAllocatingFuture future{mutex_, condition_};
    future.Wait();
    future.MarkReady();
    future.Wait();
}

}  // namespace
}  // namespace score::message_passing
