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
#include "score/memory/pmr_ring_buffer.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace score::memory
{

namespace
{

TEST(PmrRingBufferTest, EmptyBufferIsEmpty)
{
    PmrRingBuffer<std::int32_t> buffer1(1U, score::cpp::pmr::polymorphic_allocator<>());
    PmrRingBuffer<std::int32_t> buffer2(2U, score::cpp::pmr::polymorphic_allocator<>());
    EXPECT_TRUE(buffer1.empty());
    EXPECT_TRUE(buffer2.empty());
    EXPECT_FALSE(buffer1.full());
    EXPECT_FALSE(buffer2.full());
    EXPECT_EQ(buffer1.size(), 0U);
    EXPECT_EQ(buffer2.size(), 0U);
}

TEST(PmrRingBufferTest, BufferWithOneElement)
{
    PmrRingBuffer<std::int32_t> buffer1(1U, score::cpp::pmr::polymorphic_allocator<>());
    PmrRingBuffer<std::int32_t> buffer2(2U, score::cpp::pmr::polymorphic_allocator<>());
    buffer1.emplace_back(1);
    buffer2.emplace_back(2);
    EXPECT_FALSE(buffer1.empty());
    EXPECT_FALSE(buffer2.empty());
    EXPECT_TRUE(buffer1.full());
    EXPECT_FALSE(buffer2.full());
    EXPECT_EQ(buffer1.size(), 1U);
    EXPECT_EQ(buffer2.size(), 1U);
}

TEST(PmrRingBufferTest, BufferWithTwoElements)
{
    PmrRingBuffer<std::int32_t> buffer1(1U, score::cpp::pmr::polymorphic_allocator<>());
    PmrRingBuffer<std::int32_t> buffer2(2U, score::cpp::pmr::polymorphic_allocator<>());
    buffer1.emplace_back(1);
    buffer1.emplace_back(1);
    buffer2.emplace_back(2);
    buffer2.emplace_back(2);
    EXPECT_FALSE(buffer1.empty());
    EXPECT_FALSE(buffer2.empty());
    EXPECT_TRUE(buffer1.full());
    EXPECT_TRUE(buffer2.full());
    EXPECT_EQ(buffer1.size(), 1U);
    EXPECT_EQ(buffer2.size(), 2U);
}

TEST(PmrRingBufferTest, BufferInBufferOut)
{
    PmrRingBuffer<std::int32_t> buffer2(2U, score::cpp::pmr::polymorphic_allocator<>());
    buffer2.emplace_back(1);
    buffer2.emplace_back(2);
    buffer2.emplace_back(3);
    EXPECT_TRUE(buffer2.full());
    const auto& const_buffer2 = buffer2;
    EXPECT_EQ(buffer2.front(), 2);
    buffer2.pop_front();
    EXPECT_EQ(const_buffer2.front(), 3);
    buffer2.pop_front();
    EXPECT_TRUE(buffer2.empty());

    // popping empty buffer (without accessing its front element) is safe
    buffer2.pop_front();
    EXPECT_TRUE(buffer2.empty());
    EXPECT_FALSE(buffer2.full());
    EXPECT_EQ(buffer2.size(), 0U);
}

TEST(PmrRingBufferTest, PmrAwareElement)
{
    PmrRingBuffer<score::cpp::pmr::vector<std::int32_t>> buffer2(2U, score::cpp::pmr::polymorphic_allocator<>());
    buffer2.emplace_back(std::initializer_list<std::int32_t>{1});
    buffer2.emplace_back(std::initializer_list<std::int32_t>{2, 2});
    buffer2.emplace_back(std::initializer_list<std::int32_t>{3, 3, 3});
    EXPECT_EQ(buffer2.front().size(), 2);
    buffer2.pop_front();
    EXPECT_EQ(buffer2.front().size(), 3);
    buffer2.pop_front();
    EXPECT_TRUE(buffer2.empty());
}

}  // namespace

}  // namespace score::memory
