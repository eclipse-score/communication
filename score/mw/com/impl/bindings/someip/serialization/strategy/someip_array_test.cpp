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
#include "score/mw/com/impl/bindings/someip/serialization/strategy/someip_strategy_test_fixtures.h"

#include "score/serialization/types/length_prefixed_vector.h"

#include <gtest/gtest.h>

#include <score/static_vector.hpp>

#include <cstdint>
#include <vector>

namespace score
{
namespace serialization
{
namespace
{

TEST_F(SomeIpStrategyFixture, LengthPrefixedVectorIsCompact)
{
    // Given a length-prefixed vector (a one-byte length field followed by the actual elements, no capacity padding) ...
    const LengthPrefixedVector<std::uint16_t, std::uint8_t> value{0x0A0B, 0x0C0D, 0x0E0F};

    // ... when it is serialized and deserialized ...
    RoundTrip(value);

    // ... then only the actual element count and bytes are emitted, big-endian.
    ASSERT_EQ(buffer_.size(), 1U + (3U * sizeof(std::uint16_t)));  // length + actual bytes only
    auto it = buffer_.cbegin();
    EXPECT_EQ(*it++, 0x03);  // length = actual element count (3)
    EXPECT_EQ(*it++, 0x0A);  // elements, big-endian
    EXPECT_EQ(*it++, 0x0B);
    EXPECT_EQ(*it++, 0x0C);
    EXPECT_EQ(*it++, 0x0D);
    EXPECT_EQ(*it++, 0x0E);
    EXPECT_EQ(*it++, 0x0F);
}

TEST_F(SomeIpStrategyFixture, ConfigurableLengthFieldWidth)
{
    // Given a layout where field 0 (id) uses the default 4-byte length prefix and field 1 (values) a 2-byte one ...
    static constexpr ElementLayout kFields[] = {
        {ByteOrder::kBigEndian, kAlign8Bits, 4U},
        {ByteOrder::kBigEndian, kAlign8Bits, 2U},
    };
    const SomeIpLayout layout{{ByteOrder::kBigEndian, kAlign8Bits, 4U}, kFields, 2U};
    const SomeIpStrategy strategy{layout};
    const Serializer<SomeIpStrategy> unit{strategy};
    std::vector<std::uint8_t> buffer{};

    BoundedMessage value{};
    value.id = 0x0102;
    value.values = score::cpp::static_vector<std::uint32_t, 4U>{0xAABBCCDDU};

    // ... when the message is serialized ...
    auto result = unit.Serialize(value, buffer);
    ASSERT_TRUE(result.has_value());

    // ... then the vector length is written with the configured 2-byte prefix ...
    ASSERT_EQ(buffer.size(), 2U + 2U + 4U);  // id + 2-byte length + one element
    auto it = buffer.cbegin();
    EXPECT_EQ(*it++, 0x01);  // id big-endian
    EXPECT_EQ(*it++, 0x02);
    EXPECT_EQ(*it++, 0x00);  // 2-byte big-endian length = 1
    EXPECT_EQ(*it++, 0x01);
    EXPECT_EQ(*it++, 0xAA);  // single element, big-endian
    EXPECT_EQ(*it++, 0xBB);
    EXPECT_EQ(*it++, 0xCC);
    EXPECT_EQ(*it++, 0xDD);

    // ... and it deserializes back to the original message.
    BoundedMessage decoded{};
    result = unit.DeSerialize(buffer, decoded);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(decoded.id, 0x0102);
    ASSERT_EQ(decoded.values.size(), 1U);
    EXPECT_EQ(decoded.values[0], 0xAABBCCDDU);

    // GetMaxSize accounts for the 2-byte length field: id (2) + [ 2 + capacity 4 * sizeof(uint32) ].
    EXPECT_EQ(GetMaxSize<BoundedMessage>(layout), 2U + 2U + (4U * sizeof(std::uint32_t)));
}

struct AlignedArrayMessage
{
    score::cpp::static_vector<std::uint8_t, 4U> data{};  // field 0: variable-length, aligns following data to 32 bits
    std::uint32_t trailer{};                             // field 1
};
SCORE_STRUCT_VISITABLE(AlignedArrayMessage, data, trailer)

TEST_F(SomeIpStrategyFixture, VariableLengthMemberPadsFollowingDataToAlignment)
{
    // Given a layout where the array (field 0) aligns the data behind it to 32 bits (SOME/IP inserts padding after
    // variable-length data), and the trailing uint32 (field 1) is packed ...
    static constexpr ElementLayout kFields[] = {
        {ByteOrder::kBigEndian, kAlign32Bits, 1U},  // array: 1-byte length, pad after to a 32-bit boundary
        {ByteOrder::kBigEndian, kAlign8Bits, 4U},   // trailer: packed uint32
    };
    const SomeIpLayout layout{{ByteOrder::kBigEndian, kAlign8Bits}, kFields, 2U};
    const SomeIpStrategy strategy{layout};
    const Serializer<SomeIpStrategy> unit{strategy};
    std::vector<std::uint8_t> buffer{};

    AlignedArrayMessage value{};
    value.data = score::cpp::static_vector<std::uint8_t, 4U>{0xAA};
    value.trailer = 0x01020304U;

    // ... when the message is serialized ...
    auto result = unit.Serialize(value, buffer);
    ASSERT_TRUE(result.has_value());

    // ... then padding is inserted after the array so the trailer starts on a 4-byte boundary (offset 4) ...
    ASSERT_EQ(buffer.size(), 1U + 1U + 2U + 4U);  // length + element + padding(2) + trailer(4)
    auto it = buffer.cbegin();
    EXPECT_EQ(*it++, 0x01);  // array length = 1
    EXPECT_EQ(*it++, 0xAA);  // array element           (offset 1)
    EXPECT_EQ(*it++, 0x00);  // padding to align trailer (offset 2..3)
    EXPECT_EQ(*it++, 0x00);
    EXPECT_EQ(*it++, 0x01);  // trailer big-endian       (offset 4..7)
    EXPECT_EQ(*it++, 0x02);
    EXPECT_EQ(*it++, 0x03);
    EXPECT_EQ(*it++, 0x04);

    // ... and it deserializes back to the original message.
    AlignedArrayMessage decoded{};
    result = unit.DeSerialize(buffer, decoded);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(decoded.data.size(), 1U);
    EXPECT_EQ(decoded.data[0], 0xAA);
    EXPECT_EQ(decoded.trailer, 0x01020304U);
}

struct TrailingArrayMessage
{
    std::uint32_t header{};                              // field 0
    score::cpp::static_vector<std::uint8_t, 4U> data{};  // field 1: LAST element
};
SCORE_STRUCT_VISITABLE(TrailingArrayMessage, header, data)

TEST_F(SomeIpStrategyFixture, LastVariableLengthMemberEmitsNoTrailingPadding)
{
    // Given the array is the last element, configured with 256-bit alignment ...
    static constexpr ElementLayout kFields[] = {
        {ByteOrder::kBigEndian, kAlign8Bits, 4U},    // header: packed uint32
        {ByteOrder::kBigEndian, kAlign256Bits, 1U},  // data: 256-bit alignment, but last -> no padding behind it
    };
    const SomeIpLayout layout{{ByteOrder::kBigEndian, kAlign8Bits}, kFields, 2U};
    const SomeIpStrategy strategy{layout};
    const Serializer<SomeIpStrategy> unit{strategy};
    std::vector<std::uint8_t> buffer{};

    TrailingArrayMessage value{};
    value.header = 0x01020304U;
    value.data = score::cpp::static_vector<std::uint8_t, 4U>{0xAA};

    // ... when serialized ... then no trailing padding is added behind the final variable-length element.
    auto result = unit.Serialize(value, buffer);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(buffer.size(), 4U + 1U + 1U);  // header(4) + length(1) + element(1), no padding
    auto it = buffer.cbegin();
    EXPECT_EQ(*it++, 0x01);  // header big-endian
    EXPECT_EQ(*it++, 0x02);
    EXPECT_EQ(*it++, 0x03);
    EXPECT_EQ(*it++, 0x04);
    EXPECT_EQ(*it++, 0x01);  // array length = 1
    EXPECT_EQ(*it++, 0xAA);  // array element, no padding behind it

    TrailingArrayMessage decoded{};
    result = unit.DeSerialize(buffer, decoded);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(decoded.header, 0x01020304U);
    ASSERT_EQ(decoded.data.size(), 1U);
    EXPECT_EQ(decoded.data[0], 0xAA);
}

struct OffsetAlignedMessage
{
    std::uint8_t pre{};                                  // field 0: shifts the array's start position
    score::cpp::static_vector<std::uint8_t, 4U> data{};  // field 1: aligns following data to 32 bits
    std::uint32_t trailer{};                             // field 2
};
SCORE_STRUCT_VISITABLE(OffsetAlignedMessage, pre, data, trailer)

TEST_F(SomeIpStrategyFixture, AlignmentIsCalculatedFromMessageStart)
{
    // Given a leading byte before the array, the trailing padding is sized from the absolute message offset (not from
    // the array's own start): here the trailer must land on a 4-byte boundary measured from the message start.
    static constexpr ElementLayout kFields[] = {
        {ByteOrder::kBigEndian, kAlign8Bits, 4U},   // pre: packed uint8
        {ByteOrder::kBigEndian, kAlign32Bits, 1U},  // data: 1-byte length, align following data to 32 bits
        {ByteOrder::kBigEndian, kAlign8Bits, 4U},   // trailer: packed uint32
    };
    const SomeIpLayout layout{{ByteOrder::kBigEndian, kAlign8Bits}, kFields, 3U};
    const SomeIpStrategy strategy{layout};
    const Serializer<SomeIpStrategy> unit{strategy};
    std::vector<std::uint8_t> buffer{};

    OffsetAlignedMessage value{};
    value.pre = 0x05;
    value.data = score::cpp::static_vector<std::uint8_t, 4U>{0xAA};
    value.trailer = 0x01020304U;

    // ... when serialized ... then only one padding byte is needed (offset 3 -> 4), unlike the two bytes required when
    // the array starts at offset 0.
    auto result = unit.Serialize(value, buffer);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(buffer.size(), 1U + 1U + 1U + 1U + 4U);  // pre + length + element + padding(1) + trailer(4)
    auto it = buffer.cbegin();
    EXPECT_EQ(*it++, 0x05);  // pre                      (offset 0)
    EXPECT_EQ(*it++, 0x01);  // array length = 1         (offset 1)
    EXPECT_EQ(*it++, 0xAA);  // array element            (offset 2)
    EXPECT_EQ(*it++, 0x00);  // single padding byte      (offset 3)
    EXPECT_EQ(*it++, 0x01);  // trailer big-endian       (offset 4..7)
    EXPECT_EQ(*it++, 0x02);
    EXPECT_EQ(*it++, 0x03);
    EXPECT_EQ(*it++, 0x04);

    OffsetAlignedMessage decoded{};
    result = unit.DeSerialize(buffer, decoded);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(decoded.pre, 0x05);
    ASSERT_EQ(decoded.data.size(), 1U);
    EXPECT_EQ(decoded.data[0], 0xAA);
    EXPECT_EQ(decoded.trailer, 0x01020304U);
}

}  // namespace
}  // namespace serialization
}  // namespace score
