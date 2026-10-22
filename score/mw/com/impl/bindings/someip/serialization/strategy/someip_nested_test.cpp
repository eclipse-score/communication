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

TEST_F(SomeIpStrategyFixture, NestedStructResolvesPerLevelByteOrder)
{
    // Given a layout where the nested Inner struct uses little-endian for its id, supplied via Outer's nested layout
    // (the user types stay plain) ...
    static constexpr ElementLayout kInnerFields[] = {
        {ByteOrder::kLittleEndian, kAlign8Bits},  // Inner::id little-endian
        {ByteOrder::kBigEndian, kAlign8Bits},     // Inner::flag
    };
    static constexpr SomeIpLayout kInnerLayout{{ByteOrder::kBigEndian, kAlign8Bits}, kInnerFields, 2U};
    static constexpr ElementLayout kOuterFields[] = {
        {ByteOrder::kBigEndian, kAlign8Bits},                                           // Outer::a big-endian
        {ByteOrder::kBigEndian, kAlign8Bits, kDefaultLengthFieldBytes, &kInnerLayout},  // Outer::inner -> nested layout
    };
    const SomeIpLayout layout{{ByteOrder::kBigEndian, kAlign8Bits}, kOuterFields, 2U};
    const SomeIpStrategy strategy{layout};
    const Serializer<SomeIpStrategy> unit{strategy};
    std::vector<std::uint8_t> buffer{};

    Outer value{};
    value.a = 0x01020304U;
    value.inner.id = 0xAABB;
    value.inner.flag = 0x01;

    // ... when the struct is serialized ...
    auto result = unit.Serialize(value, buffer);
    ASSERT_TRUE(result.has_value());

    // ... then the outer field is big-endian while the nested id honours its own little-endian layout.
    ASSERT_EQ(buffer.size(), 7U);
    auto it = buffer.cbegin();
    EXPECT_EQ(*it++, 0x01);  // a big-endian
    EXPECT_EQ(*it++, 0x02);
    EXPECT_EQ(*it++, 0x03);
    EXPECT_EQ(*it++, 0x04);
    EXPECT_EQ(*it++, 0xBB);  // inner.id little-endian
    EXPECT_EQ(*it++, 0xAA);
    EXPECT_EQ(*it++, 0x01);  // inner.flag

    // ... and it deserializes back to the original value.
    Outer decoded{};
    result = unit.DeSerialize(buffer, decoded);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(decoded, value);
}

struct InnerArray
{
    std::uint8_t tag{};                                  // field 0
    score::cpp::static_vector<std::uint8_t, 4U> data{};  // field 1: variable-length, last in InnerArray
};
SCORE_STRUCT_VISITABLE(InnerArray, tag, data)

struct OuterArray
{
    InnerArray    inner{};    // field 0
    std::uint32_t trailer{};  // field 1
};
SCORE_STRUCT_VISITABLE(OuterArray, inner, trailer)

TEST_F(SomeIpStrategyFixture, PadsVariableLengthMemberLastInNestedStructButNotInMessage)
{
    // Given a layout where the array is the last field of the nested InnerArray and aligns following data to 32 bits,
    // while the enclosing OuterArray still has a trailer after the nested struct ...
    static constexpr ElementLayout kInnerFields[] = {
        {ByteOrder::kBigEndian, kAlign8Bits},       // tag
        {ByteOrder::kBigEndian, kAlign32Bits, 1U},  // data: 1-byte length, align following data to 32 bits
    };
    static constexpr SomeIpLayout kInnerLayout{{ByteOrder::kBigEndian, kAlign8Bits}, kInnerFields, 2U};
    static constexpr ElementLayout kOuterFields[] = {
        {ByteOrder::kBigEndian, kAlign8Bits, kDefaultLengthFieldBytes, &kInnerLayout},  // inner struct
        {ByteOrder::kBigEndian, kAlign8Bits, 4U},                                       // trailer: packed uint32
    };
    const SomeIpLayout layout{{ByteOrder::kBigEndian, kAlign8Bits}, kOuterFields, 2U};
    const SomeIpStrategy strategy{layout};
    const Serializer<SomeIpStrategy> unit{strategy};
    std::vector<std::uint8_t> buffer{};

    OuterArray value{};
    value.inner.tag = 0x09;
    value.inner.data = score::cpp::static_vector<std::uint8_t, 4U>{0xAA};
    value.trailer = 0x01020304U;

    // ... when the message is serialized ...
    auto result = unit.Serialize(value, buffer);
    ASSERT_TRUE(result.has_value());

    // ... then padding is inserted behind the array even though it is the last field of the nested struct, because the
    // trailer still follows in the enclosing struct, so the trailer starts on a 4-byte boundary.
    ASSERT_EQ(buffer.size(), 1U + 1U + 1U + 1U + 4U);  // tag + length + element + padding(1) + trailer(4)
    auto it = buffer.cbegin();
    EXPECT_EQ(*it++, 0x09);  // tag                       (offset 0)
    EXPECT_EQ(*it++, 0x01);  // array length = 1          (offset 1)
    EXPECT_EQ(*it++, 0xAA);  // array element             (offset 2)
    EXPECT_EQ(*it++, 0x00);  // padding to align trailer  (offset 3)
    EXPECT_EQ(*it++, 0x01);  // trailer big-endian        (offset 4..7)
    EXPECT_EQ(*it++, 0x02);
    EXPECT_EQ(*it++, 0x03);
    EXPECT_EQ(*it++, 0x04);

    // ... and it deserializes back to the original values.
    OuterArray decoded{};
    result = unit.DeSerialize(buffer, decoded);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(decoded.inner.tag, 0x09);
    ASSERT_EQ(decoded.inner.data.size(), 1U);
    EXPECT_EQ(decoded.inner.data[0], 0xAA);
    EXPECT_EQ(decoded.trailer, 0x01020304U);

    // GetMaxSize mirrors the nested layout: tag(1) + [length(1) + capacity 4 + worst-case pad 3] + trailer(4).
    EXPECT_EQ(GetMaxSize<OuterArray>(layout), 1U + (1U + 4U + 3U) + 4U);
}

struct Matrix
{
    // Array of arrays: outer uses a 2-byte length field, inner a 1-byte one, inner scalars little-endian.
    score::cpp::static_vector<score::cpp::static_vector<std::uint16_t, 2U>, 2U> rows{};  // field 0
};
SCORE_STRUCT_VISITABLE(Matrix, rows)

TEST_F(SomeIpStrategyFixture, ArrayOfArrayUsesPerLevelLengthFieldAndByteOrder)
{
    // Given an array-of-array layout: the outer array has a 2-byte length field, each inner array a 1-byte one, and the
    // inner scalars are little-endian -- all supplied through the outer array's nested layout.
    static constexpr SomeIpLayout kInnerLayout{{ByteOrder::kLittleEndian, kAlign8Bits, 1U}, nullptr, 0U};
    static constexpr ElementLayout kRootFields[] = {
        {ByteOrder::kBigEndian, kAlign8Bits, 2U, &kInnerLayout},  // rows: 2-byte outer length, inner via nested layout
    };
    const SomeIpLayout layout{{ByteOrder::kBigEndian, kAlign8Bits}, kRootFields, 1U};
    const SomeIpStrategy strategy{layout};
    const Serializer<SomeIpStrategy> unit{strategy};
    std::vector<std::uint8_t> buffer{};

    Matrix value{};
    value.rows.push_back(score::cpp::static_vector<std::uint16_t, 2U>{0x0102, 0x0304});
    value.rows.push_back(score::cpp::static_vector<std::uint16_t, 2U>{0x0506});

    // ... when the matrix is serialized ...
    auto result = unit.Serialize(value, buffer);
    ASSERT_TRUE(result.has_value());

    // ... then the outer length is 2 bytes, each inner length 1 byte, and the scalars little-endian.
    ASSERT_EQ(buffer.size(), 2U + (1U + 4U) + (1U + 2U));
    auto it = buffer.cbegin();
    EXPECT_EQ(*it++, 0x00);  // outer length = 2 (2-byte big-endian)
    EXPECT_EQ(*it++, 0x02);
    EXPECT_EQ(*it++, 0x02);  // row 0: inner length = 2 (1 byte)
    EXPECT_EQ(*it++, 0x02);  // 0x0102 little-endian
    EXPECT_EQ(*it++, 0x01);
    EXPECT_EQ(*it++, 0x04);  // 0x0304 little-endian
    EXPECT_EQ(*it++, 0x03);
    EXPECT_EQ(*it++, 0x01);  // row 1: inner length = 1 (1 byte)
    EXPECT_EQ(*it++, 0x06);  // 0x0506 little-endian
    EXPECT_EQ(*it++, 0x05);

    // ... and it deserializes back to the original matrix.
    Matrix decoded{};
    result = unit.DeSerialize(buffer, decoded);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(decoded.rows.size(), 2U);
    ASSERT_EQ(decoded.rows[0].size(), 2U);
    EXPECT_EQ(decoded.rows[0][0], 0x0102);
    EXPECT_EQ(decoded.rows[0][1], 0x0304);
    ASSERT_EQ(decoded.rows[1].size(), 1U);
    EXPECT_EQ(decoded.rows[1][0], 0x0506);

    // GetMaxSize mirrors the per-level layout: outer length(2) + 2 * [inner length(1) + capacity 2 * sizeof(uint16)].
    EXPECT_EQ(GetMaxSize<Matrix>(layout), 2U + (2U * (1U + (2U * sizeof(std::uint16_t)))));
}

}  // namespace
}  // namespace serialization
}  // namespace score
