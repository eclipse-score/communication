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
#include "score/mw/com/impl/bindings/someip/vector_serialization_sink.h"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <vector>

namespace score::mw::com::impl::someip
{
namespace
{

constexpr std::size_t kCapacity{64U};

TEST(VectorSerializationSinkTest, WriteAppendsTheBytesToTheBuffer)
{
    std::vector<std::byte> buffer{};
    VectorSerializationSink unit{buffer, kCapacity};
    constexpr std::array<std::byte, 2U> data{std::byte{0xA1U}, std::byte{0xB2U}};

    EXPECT_TRUE(unit.Write(data.data(), data.size()));

    ASSERT_EQ(buffer.size(), data.size());
    EXPECT_EQ(buffer[0U], std::byte{0xA1U});
    EXPECT_EQ(buffer[1U], std::byte{0xB2U});
}

TEST(VectorSerializationSinkTest, ConsecutiveWritesAreAppendedContiguously)
{
    std::vector<std::byte> buffer{};
    VectorSerializationSink unit{buffer, kCapacity};
    constexpr std::array<std::byte, 2U> first{std::byte{0x01U}, std::byte{0x02U}};
    constexpr std::array<std::byte, 1U> second{std::byte{0x03U}};

    EXPECT_TRUE(unit.Write(first.data(), first.size()));
    EXPECT_TRUE(unit.Write(second.data(), second.size()));

    ASSERT_EQ(buffer.size(), 3U);
    EXPECT_EQ(buffer[0U], std::byte{0x01U});
    EXPECT_EQ(buffer[1U], std::byte{0x02U});
    EXPECT_EQ(buffer[2U], std::byte{0x03U});
}

TEST(VectorSerializationSinkTest, WritingToANonEmptyBufferPreservesItsContent)
{
    std::vector<std::byte> buffer{std::byte{0xFFU}};
    VectorSerializationSink unit{buffer, kCapacity};
    constexpr std::array<std::byte, 1U> data{std::byte{0x11U}};

    EXPECT_TRUE(unit.Write(data.data(), data.size()));

    ASSERT_EQ(buffer.size(), 2U);
    EXPECT_EQ(buffer[0U], std::byte{0xFFU});
    EXPECT_EQ(buffer[1U], std::byte{0x11U});
}

TEST(VectorSerializationSinkTest, WritingZeroBytesSucceedsAndDoesNotChangeTheBuffer)
{
    std::vector<std::byte> buffer{};
    VectorSerializationSink unit{buffer, kCapacity};

    EXPECT_TRUE(unit.Write(nullptr, 0U));

    EXPECT_TRUE(buffer.empty());
    EXPECT_FALSE(unit.HasWriteFailed());
}

TEST(VectorSerializationSinkTest, WritingFromANullptrFails)
{
    std::vector<std::byte> buffer{};
    VectorSerializationSink unit{buffer, kCapacity};

    EXPECT_FALSE(unit.Write(nullptr, 1U));

    EXPECT_TRUE(buffer.empty());
}

TEST(VectorSerializationSinkTest, HasWriteFailedIsFalseUntilAWriteFails)
{
    std::vector<std::byte> buffer{};
    VectorSerializationSink unit{buffer, kCapacity};
    constexpr std::array<std::byte, 1U> data{std::byte{0x01U}};

    EXPECT_FALSE(unit.HasWriteFailed());
    EXPECT_TRUE(unit.Write(data.data(), data.size()));
    EXPECT_FALSE(unit.HasWriteFailed());

    EXPECT_FALSE(unit.Write(nullptr, 1U));
    EXPECT_TRUE(unit.HasWriteFailed());
}

/// \brief The failure flag is sticky, so that a caller can check it once after the whole serialization finished.
TEST(VectorSerializationSinkTest, HasWriteFailedStaysSetAfterASucceedingWrite)
{
    std::vector<std::byte> buffer{};
    VectorSerializationSink unit{buffer, kCapacity};
    constexpr std::array<std::byte, 1U> data{std::byte{0x01U}};

    EXPECT_FALSE(unit.Write(nullptr, 1U));
    EXPECT_TRUE(unit.Write(data.data(), data.size()));

    EXPECT_TRUE(unit.HasWriteFailed());
}

/// \brief The bound is what keeps the memory needed per sample predictable, so a write which would exceed it has to
///        fail instead of growing the buffer.
TEST(VectorSerializationSinkTest, WritingBeyondTheCapacityFails)
{
    std::vector<std::byte> buffer{};
    VectorSerializationSink unit{buffer, 2U};
    constexpr std::array<std::byte, 3U> data{std::byte{0x01U}, std::byte{0x02U}, std::byte{0x03U}};

    EXPECT_FALSE(unit.Write(data.data(), data.size()));

    EXPECT_TRUE(unit.HasWriteFailed());
    EXPECT_TRUE(buffer.empty());
}

TEST(VectorSerializationSinkTest, WritingExactlyUpToTheCapacitySucceeds)
{
    std::vector<std::byte> buffer{};
    VectorSerializationSink unit{buffer, 2U};
    constexpr std::array<std::byte, 2U> data{std::byte{0x01U}, std::byte{0x02U}};

    EXPECT_TRUE(unit.Write(data.data(), data.size()));

    EXPECT_FALSE(unit.HasWriteFailed());
    EXPECT_EQ(buffer.size(), 2U);
}

TEST(VectorSerializationSinkTest, AccumulatedWritesRespectTheCapacity)
{
    std::vector<std::byte> buffer{};
    VectorSerializationSink unit{buffer, 2U};
    constexpr std::array<std::byte, 2U> data{std::byte{0x01U}, std::byte{0x02U}};

    EXPECT_TRUE(unit.Write(data.data(), data.size()));
    EXPECT_FALSE(unit.Write(data.data(), 1U));

    EXPECT_EQ(buffer.size(), 2U);
}

TEST(VectorSerializationSinkTest, PatchOverwritesAlreadyWrittenBytes)
{
    std::vector<std::byte> buffer{};
    VectorSerializationSink unit{buffer, kCapacity};
    constexpr std::array<std::byte, 3U> data{std::byte{0x01U}, std::byte{0x02U}, std::byte{0x03U}};
    ASSERT_TRUE(unit.Write(data.data(), data.size()));
    constexpr std::array<std::byte, 2U> patch{std::byte{0xAAU}, std::byte{0xBBU}};

    EXPECT_TRUE(unit.Patch(1U, patch.data(), patch.size()));

    EXPECT_EQ(buffer[0U], std::byte{0x01U});
    EXPECT_EQ(buffer[1U], std::byte{0xAAU});
    EXPECT_EQ(buffer[2U], std::byte{0xBBU});
}

TEST(VectorSerializationSinkTest, PatchBeyondTheWrittenContentFails)
{
    std::vector<std::byte> buffer{};
    VectorSerializationSink unit{buffer, kCapacity};
    constexpr std::array<std::byte, 1U> data{std::byte{0x01U}};
    ASSERT_TRUE(unit.Write(data.data(), data.size()));

    EXPECT_FALSE(unit.Patch(1U, data.data(), data.size()));

    EXPECT_TRUE(unit.HasWriteFailed());
}

}  // namespace
}  // namespace score::mw::com::impl::someip
