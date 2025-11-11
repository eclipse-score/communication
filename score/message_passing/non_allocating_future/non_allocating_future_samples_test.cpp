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
#include <gtest/gtest.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include "score/message_passing/non_allocating_future/non_allocating_future.h"

namespace score::message_passing
{
namespace
{

using namespace ::testing;

class NonAllocatingFutureSamplesFixture : public ::testing::Test
{
  protected:
    void SetUp() override {}

    void TearDown() override {}

    std::mutex mutex_;
    std::condition_variable condition_;
};

TEST_F(NonAllocatingFutureSamplesFixture, VoidFutureSequentialUse)
{
    std::int32_t counter{0};
    detail::NonAllocatingFuture future1{mutex_, condition_};
    std::thread t1{[&counter, &future1]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        ++counter;
        future1.MarkReady();
    }};
    future1.Wait();
    EXPECT_EQ(counter, 1);
    t1.join();

    detail::NonAllocatingFuture future2{mutex_, condition_};
    std::thread t2{[&counter, &future2]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        ++counter;
        future2.MarkReady();
    }};
    future2.Wait();
    EXPECT_EQ(counter, 2);
    t2.join();
}

TEST_F(NonAllocatingFutureSamplesFixture, VoidFutureConcurrentUse)
{
    std::atomic<std::int32_t> counter{0};
    detail::NonAllocatingFuture future1{mutex_, condition_};
    detail::NonAllocatingFuture future2{mutex_, condition_};
    std::thread t1{[&counter, &future1]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        ++counter;
        future1.MarkReady();
    }};
    std::thread t2{[&counter, &future2]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        ++counter;
        future2.MarkReady();
    }};
    future1.Wait();
    future2.Wait();
    EXPECT_EQ(counter, 2);
    t1.join();
    t2.join();
}

TEST_F(NonAllocatingFutureSamplesFixture, NonVoidFutureSequentialUse)
{
    std::int32_t counter{0};
    detail::NonAllocatingFuture future1{mutex_, condition_, counter};
    std::thread t1{[&future1]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        std::int32_t value{1};
        future1.UpdateValueMarkReady(value);
    }};
    future1.Wait();
    EXPECT_EQ(counter, 1);
    EXPECT_EQ(future1.GetValue(), 1);
    t1.join();

    detail::NonAllocatingFuture future2{mutex_, condition_, counter};
    std::thread t2{[&future2]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        std::int32_t value{2};
        future2.UpdateValueMarkReady(std::move(value));
    }};
    future2.Wait();
    EXPECT_EQ(counter, 2);
    EXPECT_EQ(future2.GetValue(), 2);
    t2.join();
}

TEST_F(NonAllocatingFutureSamplesFixture, NonVoidFutureConcurrentUse)
{
    std::int32_t counter1{0};
    std::int32_t counter2{0};
    detail::NonAllocatingFuture future1{mutex_, condition_, counter1};
    detail::NonAllocatingFuture future2{mutex_, condition_, counter2};
    std::thread t1{[&future1]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        std::int32_t value{1};
        future1.UpdateValueMarkReady(value);
    }};
    std::thread t2{[&future2]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        std::int32_t value{2};
        future2.UpdateValueMarkReady(std::move(value));
    }};
    future1.Wait();
    future2.Wait();
    EXPECT_EQ(counter1, 1);
    EXPECT_EQ(counter2, 2);
    EXPECT_EQ(future1.GetValue(), 1);
    EXPECT_EQ(future2.GetValue(), 2);
    t1.join();
    t2.join();
}

TEST_F(NonAllocatingFutureSamplesFixture, NonVoidMarkReady)
{
    std::vector<std::int32_t> vec;
    detail::NonAllocatingFuture future{mutex_, condition_, vec};
    std::thread t{[&future]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        std::int32_t value{1};
        future.GetValueForUpdate().push_back(value);
        future.MarkReady();
    }};
    future.Wait();
    EXPECT_EQ(future.GetValue().size(), 1);
    EXPECT_EQ(future.GetValue()[0], 1);
    t.join();
}

TEST_F(NonAllocatingFutureSamplesFixture, OptionalBySignal)
{
    // In this test, we use two instances of NonAllocatingFuture to send signals in both directions.
    // Some care shall be taken about the lifetimes of the objects we are using - in this case, this is easy.
    bool maybe_in;
    std::optional<std::int32_t> maybe_out;
    detail::NonAllocatingFuture future_in{mutex_, condition_, maybe_in};
    detail::NonAllocatingFuture future_out{mutex_, condition_, maybe_out};
    std::thread t{[&future_in, &future_out]() {
        future_in.Wait();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        future_out.UpdateValueMarkReady(future_in.GetValue() ? std::optional<std::int32_t>{1}
                                                             : std::optional<std::int32_t>{});
    }};
    future_in.UpdateValueMarkReady(false);
    future_out.Wait();
    EXPECT_EQ(future_out.GetValue().value_or(-1), -1);
    t.join();
}

}  // namespace
}  // namespace score::message_passing
