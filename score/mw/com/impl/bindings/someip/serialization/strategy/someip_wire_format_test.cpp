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
#include "score/mw/com/impl/bindings/someip/serialization/strategy/someip_wire_format.h"

#include "score/mw/com/impl/bindings/someip/serialization/strategy/someip_layout.h"
#include "score/serialization/serialize_iterator.h"

#include <gtest/gtest.h>

#include <score/utility.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace score
{
namespace serialization
{
namespace
{

using Buffer = std::vector<std::uint8_t>;

constexpr std::uint8_t kDataByte{0xAA};  // arbitrary non-zero payload byte

Buffer WriteLengthField(const std::uint64_t length, const ElementLayout& layout)
{
    Buffer buffer{};
    someip_detail::WriteLengthField(SerializeInto(buffer), length, layout);
    return buffer;
}

std::uint64_t ReadLengthField(const Buffer& buffer, const ElementLayout& layout)
{
    DeSerializeIterator<Buffer> iterator{buffer};
    return someip_detail::ReadLengthField(iterator, layout);
}

TEST(SomeIpWireFormat, LengthFieldIsBigEndianByDefault)
{
    // Given a 4-byte big-endian length field ...
    const ElementLayout layout{ByteOrder::kBigEndian, kAlign8Bits, 4U};

    // ... when a length is written ...
    const Buffer buffer = WriteLengthField(0x01020304U, layout);

    // ... then the bytes are most-significant-first and it round-trips.
    ASSERT_EQ(buffer.size(), 4U);
    EXPECT_EQ(buffer, (Buffer{0x01, 0x02, 0x03, 0x04}));
    EXPECT_EQ(ReadLengthField(buffer, layout), 0x01020304U);
}

TEST(SomeIpWireFormat, LengthFieldCanBeLittleEndian)
{
    // Given a 4-byte little-endian length field ...
    const ElementLayout layout{ByteOrder::kLittleEndian, kAlign8Bits, 4U};

    // ... when a length is written ...
    const Buffer buffer = WriteLengthField(0x01020304U, layout);

    // ... then the bytes are least-significant-first and it round-trips.
    ASSERT_EQ(buffer.size(), 4U);
    EXPECT_EQ(buffer, (Buffer{0x04, 0x03, 0x02, 0x01}));
    EXPECT_EQ(ReadLengthField(buffer, layout), 0x01020304U);
}

TEST(SomeIpWireFormat, LengthFieldWidthIsConfigurable)
{
    // Given length fields of different configured widths ...
    const ElementLayout one_byte{ByteOrder::kBigEndian, kAlign8Bits, 1U};
    const ElementLayout two_byte{ByteOrder::kBigEndian, kAlign8Bits, 2U};

    // ... when a length is written ... then exactly that many bytes are emitted and it round-trips.
    const Buffer one = WriteLengthField(0x07U, one_byte);
    EXPECT_EQ(one, (Buffer{0x07}));
    EXPECT_EQ(ReadLengthField(one, one_byte), 0x07U);

    const Buffer two = WriteLengthField(0x0102U, two_byte);
    EXPECT_EQ(two, (Buffer{0x01, 0x02}));
    EXPECT_EQ(ReadLengthField(two, two_byte), 0x0102U);
}

TEST(SomeIpWireFormat, AlignmentPaddingRoundTrips)
{
    // Given one byte written at offset 0 and a 32-bit alignment ...
    Buffer buffer{};
    auto iterator = SerializeInto(buffer);
    iterator = kDataByte;  // Offset() now reports 1

    // ... when padding is emitted ... then the next write position is aligned to 4 bytes.
    someip_detail::EmitAlignmentPadding(iterator, kAlign32Bits);
    ASSERT_EQ(buffer.size(), 4U);  // 1 data byte + 3 padding bytes
    EXPECT_EQ(buffer, (Buffer{kDataByte, kPaddingByte, kPaddingByte, kPaddingByte}));

    // ... and when the same alignment is skipped on read ... then the iterator advances past the padding.
    DeSerializeIterator<Buffer> read{buffer};
    EXPECT_EQ(*read, kDataByte);
    score::cpp::ignore = ++read;
    someip_detail::SkipAlignmentPadding(read, kAlign32Bits);
    EXPECT_EQ(read, read.End());
}

TEST(SomeIpWireFormat, PackedAlignmentEmitsNoPadding)
{
    // Given one byte written at offset 0 and a packed (8-bit) alignment ...
    Buffer buffer{};
    auto iterator = SerializeInto(buffer);
    iterator = kDataByte;

    // ... when padding is emitted ... then no padding bytes are added.
    someip_detail::EmitAlignmentPadding(iterator, kDefaultAlignmentBits);
    EXPECT_EQ(buffer.size(), 1U);
}

TEST(SomeIpWireFormat, AlignmentCoversAllSomeIpBoundaries)
{
    // SOME/IP permits 8, 16, 32, 64, 128 or 256-bit alignment behind variable-length data.
    constexpr std::array<std::uint16_t, 6U> kAlignments{
        kAlign8Bits, kAlign16Bits, kAlign32Bits, kAlign64Bits, kAlign128Bits, kAlign256Bits};
    for (const std::uint16_t alignment_bits : kAlignments)
    {
        const std::size_t align_bytes = static_cast<std::size_t>(alignment_bits) / kBitsPerByte;

        // One data byte at offset 0, then padding: the result is padded up to the next alignment boundary.
        Buffer buffer{};
        auto iterator = SerializeInto(buffer);
        iterator = kDataByte;
        someip_detail::EmitAlignmentPadding(iterator, alignment_bits);

        const std::size_t expected_size = (align_bytes == 1U) ? 1U : align_bytes;
        EXPECT_EQ(buffer.size(), expected_size) << "alignment_bits=" << alignment_bits;
        EXPECT_EQ(buffer.size() % align_bytes, 0U) << "alignment_bits=" << alignment_bits;
        EXPECT_EQ(buffer.at(0), kDataByte) << "alignment_bits=" << alignment_bits;
        for (std::size_t index{1U}; index < buffer.size(); ++index)
        {
            EXPECT_EQ(buffer.at(index), kPaddingByte) << "alignment_bits=" << alignment_bits;  // padding
        }

        // The reader skips exactly the same padding.
        DeSerializeIterator<Buffer> read{buffer};
        EXPECT_EQ(*read, kDataByte);
        score::cpp::ignore = ++read;
        someip_detail::SkipAlignmentPadding(read, alignment_bits);
        EXPECT_EQ(read, read.End()) << "alignment_bits=" << alignment_bits;
    }
}

}  // namespace
}  // namespace serialization
}  // namespace score
