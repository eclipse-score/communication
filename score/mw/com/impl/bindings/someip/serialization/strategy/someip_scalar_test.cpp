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
#include "score/mw/com/impl/bindings/someip/serialization/strategy/someip_scalar.h"

#include "score/mw/com/impl/bindings/someip/serialization/strategy/someip_layout.h"
#include "score/serialization/serialize_iterator.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace score
{
namespace serialization
{
namespace
{

using Buffer = std::vector<std::uint8_t>;

template <typename T>
Buffer SerializeScalar(const T& value, const ElementLayout& layout = {})
{
    Buffer buffer{};
    someip_detail::SerializeScalar(SerializeInto(buffer), value, layout);
    return buffer;
}

template <typename T>
T DeSerializeScalar(const Buffer& buffer, const ElementLayout& layout = {})
{
    DeSerializeIterator<Buffer> iterator{buffer};
    T value{};
    someip_detail::DeSerializeScalar(iterator, value, layout);
    return value;
}

/// @brief Serializes a scalar in both byte orders and checks: big-endian matches the given bytes, little-endian is
/// their reverse, and both orders round-trip back to the original value.
template <typename T>
void ExpectBothByteOrders(const T& value, const Buffer& big_endian_bytes)
{
    // Given a scalar value and its expected big-endian representation (the function arguments) ...

    // ... when it is serialized with the default (big-endian) layout ...
    const Buffer big_endian = SerializeScalar(value);
    // ... then the bytes are most-significant-first and it round-trips.
    EXPECT_EQ(big_endian, big_endian_bytes);
    EXPECT_EQ(DeSerializeScalar<T>(big_endian), value);

    // ... and when it is serialized with a little-endian layout ...
    const ElementLayout little_endian_layout{ByteOrder::kLittleEndian, kAlign8Bits};
    const Buffer little_endian = SerializeScalar(value, little_endian_layout);
    // ... then the bytes are the reverse and it round-trips.
    const Buffer reversed{big_endian_bytes.rbegin(), big_endian_bytes.rend()};
    EXPECT_EQ(little_endian, reversed);
    EXPECT_EQ(DeSerializeScalar<T>(little_endian, little_endian_layout), value);
}

TEST(SomeIpScalar, Uint8)
{
    ExpectBothByteOrders<std::uint8_t>(0xABU, {0xAB});
}

TEST(SomeIpScalar, Sint8)
{
    ExpectBothByteOrders<std::int8_t>(-2, {0xFE});  // two's complement
}

TEST(SomeIpScalar, Uint16)
{
    ExpectBothByteOrders<std::uint16_t>(0xAABBU, {0xAA, 0xBB});
}

TEST(SomeIpScalar, Sint16)
{
    ExpectBothByteOrders<std::int16_t>(-2, {0xFF, 0xFE});
}

TEST(SomeIpScalar, Uint32)
{
    ExpectBothByteOrders<std::uint32_t>(0xAABBCCDDU, {0xAA, 0xBB, 0xCC, 0xDD});
}

TEST(SomeIpScalar, Sint32)
{
    ExpectBothByteOrders<std::int32_t>(-2, {0xFF, 0xFF, 0xFF, 0xFE});
}

TEST(SomeIpScalar, Uint64)
{
    ExpectBothByteOrders<std::uint64_t>(0x0102030405060708U, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08});
}

TEST(SomeIpScalar, Sint64)
{
    ExpectBothByteOrders<std::int64_t>(-2, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE});
}

TEST(SomeIpScalar, Float32Ieee754)
{
    ExpectBothByteOrders<float>(1.0F, {0x3F, 0x80, 0x00, 0x00});  // IEEE-754 binary32
}

TEST(SomeIpScalar, Float64Ieee754)
{
    ExpectBothByteOrders<double>(1.0, {0x3F, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});  // IEEE-754 binary64
}

TEST(SomeIpScalar, EnumUsesUnderlyingWidth)
{
    enum class Color : std::uint16_t
    {
        kRed = 0xAABB,
    };
    ExpectBothByteOrders<Color>(Color::kRed, {0xAA, 0xBB});
}

TEST(SomeIpScalar, BoolIsSingleByte)
{
    // bool uses a dedicated helper: a single packed byte, 0x01 for true / 0x00 for false.

    // Given true ... when serialized ... then it is a single 0x01 byte.
    Buffer true_buffer{};
    someip_detail::SerializeBool(SerializeInto(true_buffer), true);
    ASSERT_EQ(true_buffer.size(), 1U);
    EXPECT_EQ(true_buffer.at(0), 0x01);

    // Given false ... when serialized ... then it is a single 0x00 byte.
    Buffer false_buffer{};
    someip_detail::SerializeBool(SerializeInto(false_buffer), false);
    ASSERT_EQ(false_buffer.size(), 1U);
    EXPECT_EQ(false_buffer.at(0), 0x00);

    // ... and when the 0x01 byte is deserialized ... then it decodes back to true.
    DeSerializeIterator<Buffer> iterator{true_buffer};
    bool decoded{false};
    someip_detail::DeSerializeBool(iterator, decoded);
    EXPECT_TRUE(decoded);
}

TEST(SomeIpScalar, BoolAtEndOfInputLatchesError)
{
    // Given an empty input ...
    Buffer empty{};
    DeSerializeIterator<Buffer> iterator{empty};
    bool decoded{false};

    // ... when a bool is deserialized ...
    someip_detail::DeSerializeBool(iterator, decoded);

    // ... then an error is latched and the value is untouched.
    EXPECT_TRUE(iterator.HasError());
    EXPECT_FALSE(decoded);
}

}  // namespace
}  // namespace serialization
}  // namespace score
