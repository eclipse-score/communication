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
 *******************************************************************************/
#include "score/mw/com/impl/bindings/someip/serialization/serialization_helpers.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>

namespace score::mw::com::impl::someip::serialization
{
namespace
{

// Boolean wire encoding.
constexpr std::uint8_t kBoolTrueByte = 0x01U;
constexpr std::uint8_t kBoolFalseByte = 0x00U;
constexpr std::uint8_t kBoolUninitializedByte = 0xAAU;

// 8-bit scalar values and their wire encodings.
constexpr std::uint8_t kValueU8 = 0xABU;
constexpr std::int8_t kValueI8 = -5;
constexpr std::uint8_t kValueI8TwosComplementByte = 0xFBU;

// 16-bit scalar value and its big/little-endian wire encodings.
constexpr std::uint16_t kValueU16 = 0x1234U;
constexpr std::array<std::uint8_t, 2U> kValueU16BigEndian = {0x12U, 0x34U};
constexpr std::array<std::uint8_t, 2U> kValueU16LittleEndian = {0x34U, 0x12U};

// 32-bit scalar value and its big/little-endian wire encodings.
constexpr std::uint32_t kValueU32 = 0x12345678U;
constexpr std::array<std::uint8_t, 4U> kValueU32BigEndian = {0x12U, 0x34U, 0x56U, 0x78U};
constexpr std::array<std::uint8_t, 4U> kValueU32LittleEndian = {0x78U, 0x56U, 0x34U, 0x12U};

// 64-bit scalar value and its big/little-endian wire encodings.
constexpr std::uint64_t kValueU64 = 0x0123456789ABCDEFULL;
constexpr std::array<std::uint8_t, 8U> kValueU64BigEndian = {0x01U, 0x23U, 0x45U, 0x67U, 0x89U, 0xABU, 0xCDU, 0xEFU};
constexpr std::array<std::uint8_t, 8U> kValueU64LittleEndian = {0xEFU, 0xCDU, 0xABU, 0x89U, 0x67U, 0x45U, 0x23U, 0x01U};

// Representative negative signed values used for round-trip checks.
constexpr std::int16_t kNegativeI16 = -12345;
constexpr std::int32_t kNegativeI32 = -987654321;
constexpr std::int64_t kNegativeI64 = -1234567890123456789LL;

// 32-bit float 1.0F == 0x3F800000 in IEEE-754 binary32, and its wire encodings.
constexpr float kFloat32One = 1.0F;
constexpr std::array<std::uint8_t, 4U> kFloat32OneBigEndian = {0x3FU, 0x80U, 0x00U, 0x00U};
constexpr std::array<std::uint8_t, 4U> kFloat32OneLittleEndian = {0x00U, 0x00U, 0x80U, 0x3FU};

// 64-bit double 1.0 == 0x3FF0000000000000 in IEEE-754 binary64, and its wire encodings.
constexpr double kFloat64One = 1.0;
constexpr std::array<std::uint8_t, 8U> kFloat64OneBigEndian = {0x3FU, 0xF0U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
constexpr std::array<std::uint8_t, 8U> kFloat64OneLittleEndian =
    {0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0xF0U, 0x3FU};

// Representative finite floating-point values used for round-trip checks.
constexpr float kFloat32Sample = 3.14159265F;
constexpr double kFloat64Sample = 2.718281828459045;

/// \brief Writes value in the given byte order and checks it reads back unchanged.
template <typename T>
void ExpectRoundTrip(const T value, const ByteOrder order)
{
    std::array<std::uint8_t, sizeof(T)> buffer{};
    WriteScalar<T>(value, order, buffer);
    EXPECT_EQ(ReadScalar<T>(order, buffer), value);
}

TEST(BoolHelper, WritesCanonicalBytes)
{
    // Given a single-byte buffer
    std::array<std::uint8_t, 1U> buffer{kBoolUninitializedByte};

    // When writing TRUE
    WriteBool(true, buffer);
    // Then the byte is the canonical TRUE encoding
    EXPECT_EQ(buffer[0], kBoolTrueByte);

    // When writing FALSE
    WriteBool(false, buffer);
    // Then the byte is the canonical FALSE encoding
    EXPECT_EQ(buffer[0], kBoolFalseByte);
}

TEST(BoolHelper, ReadsOnlyLowestBit)
{
    // Given bytes whose lowest bit is set
    // When reading them as a boolean
    // Then they decode to TRUE and the other bits are ignored
    EXPECT_TRUE(ReadBool(std::array<std::uint8_t, 1U>{0x01U}));
    EXPECT_TRUE(ReadBool(std::array<std::uint8_t, 1U>{0xFFU}));
    EXPECT_TRUE(ReadBool(std::array<std::uint8_t, 1U>{0x03U}));

    // Given bytes whose lowest bit is clear
    // When reading them as a boolean
    // Then they decode to FALSE and the other bits are ignored
    EXPECT_FALSE(ReadBool(std::array<std::uint8_t, 1U>{0x00U}));
    EXPECT_FALSE(ReadBool(std::array<std::uint8_t, 1U>{0xFEU}));
    EXPECT_FALSE(ReadBool(std::array<std::uint8_t, 1U>{0x02U}));
}

TEST(Int8Helper, RoundTripUnsignedAndSigned)
{
    // Given a single-byte buffer
    std::array<std::uint8_t, 1U> buffer{};

    // When writing an unsigned 8-bit value
    WriteScalar<std::uint8_t>(kValueU8, ByteOrder::kBigEndian, buffer);
    // Then the byte matches the value and round-trips back
    EXPECT_EQ(buffer[0], kValueU8);
    EXPECT_EQ(ReadScalar<std::uint8_t>(ByteOrder::kBigEndian, buffer), kValueU8);

    // When writing a negative signed 8-bit value
    WriteScalar<std::int8_t>(kValueI8, ByteOrder::kLittleEndian, buffer);
    // Then the byte is its two's-complement encoding and round-trips back
    EXPECT_EQ(buffer[0], kValueI8TwosComplementByte);
    EXPECT_EQ(ReadScalar<std::int8_t>(ByteOrder::kLittleEndian, buffer), kValueI8);

    // When writing the signed 8-bit minimum
    WriteScalar<std::int8_t>(std::numeric_limits<std::int8_t>::min(), ByteOrder::kBigEndian, buffer);
    // Then it round-trips back unchanged
    EXPECT_EQ(ReadScalar<std::int8_t>(ByteOrder::kBigEndian, buffer), std::numeric_limits<std::int8_t>::min());
}

TEST(Uint16Helper, RespectsByteOrder)
{
    // Given a two-byte buffer
    std::array<std::uint8_t, 2U> buffer{};

    // When writing a 16-bit value in big-endian order
    WriteScalar<std::uint16_t>(kValueU16, ByteOrder::kBigEndian, buffer);
    // Then the bytes are most-significant first and round-trip back
    EXPECT_EQ(buffer, kValueU16BigEndian);
    EXPECT_EQ(ReadScalar<std::uint16_t>(ByteOrder::kBigEndian, buffer), kValueU16);

    // When writing the same value in little-endian order
    WriteScalar<std::uint16_t>(kValueU16, ByteOrder::kLittleEndian, buffer);
    // Then the bytes are least-significant first and round-trip back
    EXPECT_EQ(buffer, kValueU16LittleEndian);
    EXPECT_EQ(ReadScalar<std::uint16_t>(ByteOrder::kLittleEndian, buffer), kValueU16);
}

TEST(Uint32Helper, RespectsByteOrder)
{
    // Given a four-byte buffer
    std::array<std::uint8_t, 4U> buffer{};

    // When writing a 32-bit value in big-endian order
    WriteScalar<std::uint32_t>(kValueU32, ByteOrder::kBigEndian, buffer);
    // Then the bytes are most-significant first and round-trip back
    EXPECT_EQ(buffer, kValueU32BigEndian);
    EXPECT_EQ(ReadScalar<std::uint32_t>(ByteOrder::kBigEndian, buffer), kValueU32);

    // When writing the same value in little-endian order
    WriteScalar<std::uint32_t>(kValueU32, ByteOrder::kLittleEndian, buffer);
    // Then the bytes are least-significant first and round-trip back
    EXPECT_EQ(buffer, kValueU32LittleEndian);
    EXPECT_EQ(ReadScalar<std::uint32_t>(ByteOrder::kLittleEndian, buffer), kValueU32);
}

TEST(Uint64Helper, RespectsByteOrder)
{
    // Given an eight-byte buffer
    std::array<std::uint8_t, 8U> buffer{};

    // When writing a 64-bit value in big-endian order
    WriteScalar<std::uint64_t>(kValueU64, ByteOrder::kBigEndian, buffer);
    // Then the bytes are most-significant first and round-trip back
    EXPECT_EQ(buffer, kValueU64BigEndian);
    EXPECT_EQ(ReadScalar<std::uint64_t>(ByteOrder::kBigEndian, buffer), kValueU64);

    // When writing the same value in little-endian order
    WriteScalar<std::uint64_t>(kValueU64, ByteOrder::kLittleEndian, buffer);
    // Then the bytes are least-significant first and round-trip back
    EXPECT_EQ(buffer, kValueU64LittleEndian);
    EXPECT_EQ(ReadScalar<std::uint64_t>(ByteOrder::kLittleEndian, buffer), kValueU64);
}

TEST(SignedHelpers, RoundTripBothEndianness)
{
    // Given the signed minima and representative negative values
    // When writing each in big-endian order
    // Then each round-trips back unchanged
    ExpectRoundTrip<std::int16_t>(std::numeric_limits<std::int16_t>::min(), ByteOrder::kBigEndian);
    ExpectRoundTrip<std::int16_t>(kNegativeI16, ByteOrder::kBigEndian);
    ExpectRoundTrip<std::int32_t>(std::numeric_limits<std::int32_t>::min(), ByteOrder::kBigEndian);
    ExpectRoundTrip<std::int32_t>(kNegativeI32, ByteOrder::kBigEndian);
    ExpectRoundTrip<std::int64_t>(std::numeric_limits<std::int64_t>::min(), ByteOrder::kBigEndian);
    ExpectRoundTrip<std::int64_t>(kNegativeI64, ByteOrder::kBigEndian);

    // When writing each in little-endian order
    // Then each round-trips back unchanged
    ExpectRoundTrip<std::int16_t>(std::numeric_limits<std::int16_t>::min(), ByteOrder::kLittleEndian);
    ExpectRoundTrip<std::int16_t>(kNegativeI16, ByteOrder::kLittleEndian);
    ExpectRoundTrip<std::int32_t>(std::numeric_limits<std::int32_t>::min(), ByteOrder::kLittleEndian);
    ExpectRoundTrip<std::int32_t>(kNegativeI32, ByteOrder::kLittleEndian);
    ExpectRoundTrip<std::int64_t>(std::numeric_limits<std::int64_t>::min(), ByteOrder::kLittleEndian);
    ExpectRoundTrip<std::int64_t>(kNegativeI64, ByteOrder::kLittleEndian);
}

TEST(Float32Helper, RespectsByteOrder)
{
    // Given a four-byte buffer
    std::array<std::uint8_t, 4U> buffer{};

    // When writing a 32-bit float in big-endian order
    WriteScalar<float>(kFloat32One, ByteOrder::kBigEndian, buffer);
    // Then the bytes are the IEEE-754 pattern most-significant first and round-trip back
    EXPECT_EQ(kFloat32OneBigEndian, buffer);
    EXPECT_EQ(ReadScalar<float>(ByteOrder::kBigEndian, buffer), kFloat32One);

    // When writing the same value in little-endian order
    WriteScalar<float>(kFloat32One, ByteOrder::kLittleEndian, buffer);
    // Then the bytes are least-significant first and round-trip back
    EXPECT_EQ(kFloat32OneLittleEndian, buffer);
    EXPECT_EQ(ReadScalar<float>(ByteOrder::kLittleEndian, buffer), kFloat32One);
}

TEST(Float64Helper, RespectsByteOrder)
{
    // Given an eight-byte buffer
    std::array<std::uint8_t, 8U> buffer{};

    // When writing a 64-bit double in big-endian order
    WriteScalar<double>(kFloat64One, ByteOrder::kBigEndian, buffer);
    // Then the bytes are the IEEE-754 pattern most-significant first and round-trip back
    EXPECT_EQ(kFloat64OneBigEndian, buffer);
    EXPECT_EQ(ReadScalar<double>(ByteOrder::kBigEndian, buffer), kFloat64One);

    // When writing the same value in little-endian order
    WriteScalar<double>(kFloat64One, ByteOrder::kLittleEndian, buffer);
    // Then the bytes are least-significant first and round-trip back
    EXPECT_EQ(kFloat64OneLittleEndian, buffer);
    EXPECT_EQ(ReadScalar<double>(ByteOrder::kLittleEndian, buffer), kFloat64One);
}

TEST(FloatHelpers, Float32RoundTripSpecialValues)
{
    // Given representative special float values
    // When writing each in big-endian order
    // Then each round-trips back unchanged
    ExpectRoundTrip<float>(-0.0F, ByteOrder::kBigEndian);
    ExpectRoundTrip<float>(std::numeric_limits<float>::lowest(), ByteOrder::kBigEndian);
    ExpectRoundTrip<float>(std::numeric_limits<float>::max(), ByteOrder::kBigEndian);
    ExpectRoundTrip<float>(std::numeric_limits<float>::min(), ByteOrder::kBigEndian);
    ExpectRoundTrip<float>(kFloat32Sample, ByteOrder::kBigEndian);
    ExpectRoundTrip<float>(std::numeric_limits<float>::infinity(), ByteOrder::kBigEndian);

    // When writing each in little-endian order
    // Then each round-trips back unchanged
    ExpectRoundTrip<float>(-0.0F, ByteOrder::kLittleEndian);
    ExpectRoundTrip<float>(std::numeric_limits<float>::lowest(), ByteOrder::kLittleEndian);
    ExpectRoundTrip<float>(std::numeric_limits<float>::max(), ByteOrder::kLittleEndian);
    ExpectRoundTrip<float>(std::numeric_limits<float>::min(), ByteOrder::kLittleEndian);
    ExpectRoundTrip<float>(kFloat32Sample, ByteOrder::kLittleEndian);
    ExpectRoundTrip<float>(std::numeric_limits<float>::infinity(), ByteOrder::kLittleEndian);
}

TEST(FloatHelpers, Float64RoundTripSpecialValues)
{
    // Given representative special double values
    // When writing each in big-endian order
    // Then each round-trips back unchanged
    ExpectRoundTrip<double>(-0.0, ByteOrder::kBigEndian);
    ExpectRoundTrip<double>(std::numeric_limits<double>::lowest(), ByteOrder::kBigEndian);
    ExpectRoundTrip<double>(std::numeric_limits<double>::max(), ByteOrder::kBigEndian);
    ExpectRoundTrip<double>(std::numeric_limits<double>::min(), ByteOrder::kBigEndian);
    ExpectRoundTrip<double>(kFloat64Sample, ByteOrder::kBigEndian);
    ExpectRoundTrip<double>(std::numeric_limits<double>::infinity(), ByteOrder::kBigEndian);

    // When writing each in little-endian order
    // Then each round-trips back unchanged
    ExpectRoundTrip<double>(-0.0, ByteOrder::kLittleEndian);
    ExpectRoundTrip<double>(std::numeric_limits<double>::lowest(), ByteOrder::kLittleEndian);
    ExpectRoundTrip<double>(std::numeric_limits<double>::max(), ByteOrder::kLittleEndian);
    ExpectRoundTrip<double>(std::numeric_limits<double>::min(), ByteOrder::kLittleEndian);
    ExpectRoundTrip<double>(kFloat64Sample, ByteOrder::kLittleEndian);
    ExpectRoundTrip<double>(std::numeric_limits<double>::infinity(), ByteOrder::kLittleEndian);
}

}  // namespace
}  // namespace score::mw::com::impl::someip::serialization
