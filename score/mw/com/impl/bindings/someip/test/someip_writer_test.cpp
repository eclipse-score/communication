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
#include "score/mw/com/impl/bindings/someip/someip_writer.h"

#include "score/mw/com/impl/bindings/someip/vector_serialization_sink.h"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace score::mw::com::impl::someip
{
namespace
{

constexpr std::size_t kCapacity{128U};

class SomeIpWriterFixture : public ::testing::Test
{
  protected:
    std::vector<std::byte> buffer_{};
    VectorSerializationSink sink_{buffer_, kCapacity};
    SomeIpWriter unit_{sink_};
};

TEST_F(SomeIpWriterFixture, WritesAnUInt8AsIs)
{
    EXPECT_TRUE(unit_.Write(std::uint8_t{0xAB}));

    ASSERT_EQ(buffer_.size(), 1U);
    EXPECT_EQ(buffer_[0U], std::byte{0xAB});
}

TEST_F(SomeIpWriterFixture, WritesAnUInt16InNetworkByteOrder)
{
    EXPECT_TRUE(unit_.Write(std::uint16_t{0x1234}));

    ASSERT_EQ(buffer_.size(), 2U);
    EXPECT_EQ(buffer_[0U], std::byte{0x12});
    EXPECT_EQ(buffer_[1U], std::byte{0x34});
}

TEST_F(SomeIpWriterFixture, WritesAnUInt32InNetworkByteOrder)
{
    EXPECT_TRUE(unit_.Write(std::uint32_t{0x12345678U}));

    ASSERT_EQ(buffer_.size(), 4U);
    const std::array<std::byte, 4U> expected{std::byte{0x12}, std::byte{0x34}, std::byte{0x56}, std::byte{0x78}};
    EXPECT_TRUE(std::equal(buffer_.cbegin(), buffer_.cend(), expected.cbegin()));
}

TEST_F(SomeIpWriterFixture, WritesAnUInt64InNetworkByteOrder)
{
    EXPECT_TRUE(unit_.Write(std::uint64_t{0x0123456789ABCDEFULL}));

    ASSERT_EQ(buffer_.size(), 8U);
    const std::array<std::byte, 8U> expected{std::byte{0x01},
                                             std::byte{0x23},
                                             std::byte{0x45},
                                             std::byte{0x67},
                                             std::byte{0x89},
                                             std::byte{0xAB},
                                             std::byte{0xCD},
                                             std::byte{0xEF}};
    EXPECT_TRUE(std::equal(buffer_.cbegin(), buffer_.cend(), expected.cbegin()));
}

/// \brief The length of a variable length member is only known after its content was written, so the field is
///        reserved first and patched afterwards.
TEST_F(SomeIpWriterFixture, PatchesTheLengthFieldOfALengthDelimitedMember)
{
    ASSERT_TRUE(unit_.BeginLengthDelimited());
    ASSERT_TRUE(unit_.Write(std::uint16_t{0xAABB}));
    ASSERT_TRUE(unit_.Write(std::uint8_t{0xCC}));
    ASSERT_TRUE(unit_.EndLengthDelimited());

    ASSERT_EQ(buffer_.size(), 7U);
    // Length field holds the 3 bytes of content which followed it.
    EXPECT_EQ(buffer_[0U], std::byte{0x00});
    EXPECT_EQ(buffer_[1U], std::byte{0x00});
    EXPECT_EQ(buffer_[2U], std::byte{0x00});
    EXPECT_EQ(buffer_[3U], std::byte{0x03});
    EXPECT_EQ(buffer_[4U], std::byte{0xAA});
    EXPECT_FALSE(unit_.HasFailed());
}

TEST_F(SomeIpWriterFixture, PatchesNestedLengthFieldsIndependently)
{
    ASSERT_TRUE(unit_.BeginLengthDelimited());  // outer
    ASSERT_TRUE(unit_.BeginLengthDelimited());  // inner
    ASSERT_TRUE(unit_.Write(std::uint8_t{0x01}));
    ASSERT_TRUE(unit_.EndLengthDelimited());  // inner: 1 byte
    ASSERT_TRUE(unit_.EndLengthDelimited());  // outer: 4 byte length field + 1 byte content

    ASSERT_EQ(buffer_.size(), 9U);
    EXPECT_EQ(buffer_[3U], std::byte{0x05});  // outer length
    EXPECT_EQ(buffer_[7U], std::byte{0x01});  // inner length
    EXPECT_FALSE(unit_.HasFailed());
}

TEST_F(SomeIpWriterFixture, EndWithoutBeginFails)
{
    EXPECT_FALSE(unit_.EndLengthDelimited());

    EXPECT_TRUE(unit_.HasFailed());
}

/// \brief An unterminated length delimited member leaves a length field holding a placeholder, so the content is
///        structurally incomplete even though every single write succeeded.
TEST_F(SomeIpWriterFixture, AnUnterminatedLengthDelimitedMemberIsReportedAsFailed)
{
    ASSERT_TRUE(unit_.BeginLengthDelimited());
    ASSERT_TRUE(unit_.Write(std::uint8_t{0x01}));

    EXPECT_TRUE(unit_.HasFailed());
}

TEST_F(SomeIpWriterFixture, AFailedWriteIsReportedToTheSink)
{
    std::vector<std::byte> small_buffer{};
    VectorSerializationSink small_sink{small_buffer, 1U};
    SomeIpWriter unit{small_sink};

    EXPECT_FALSE(unit.Write(std::uint32_t{0U}));

    EXPECT_TRUE(unit.HasFailed());
    EXPECT_TRUE(small_sink.HasWriteFailed());
}

/// \brief Once the writer failed it must not keep appending, otherwise a partially written member would look like
///        valid content.
TEST_F(SomeIpWriterFixture, StopsWritingAfterAFailure)
{
    std::vector<std::byte> small_buffer{};
    VectorSerializationSink small_sink{small_buffer, 2U};
    SomeIpWriter unit{small_sink};

    EXPECT_FALSE(unit.Write(std::uint32_t{0U}));
    const auto size_after_failure = small_buffer.size();

    EXPECT_FALSE(unit.Write(std::uint8_t{0xFF}));
    EXPECT_EQ(small_buffer.size(), size_after_failure);
}

}  // namespace
}  // namespace score::mw::com::impl::someip
