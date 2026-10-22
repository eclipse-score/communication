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

}  // namespace
}  // namespace serialization
}  // namespace score
