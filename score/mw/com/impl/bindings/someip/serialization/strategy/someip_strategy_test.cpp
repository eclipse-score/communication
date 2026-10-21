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

#include <cstdint>
#include <vector>

namespace score
{
namespace serialization
{
namespace
{

TEST_F(SomeIpStrategyFixture, NestedStructIsBigEndianAndPacked)
{
    // Given a nested struct ...
    const Outer value{};

    // ... when it is serialized and deserialized ...
    RoundTrip(value);

    // ... then all fields are emitted big-endian and packed, in declaration order.
    ASSERT_EQ(buffer_.size(), 7U);
    auto it = buffer_.cbegin();
    EXPECT_EQ(*it++, 0x01);  // Outer::a (big-endian)
    EXPECT_EQ(*it++, 0x02);
    EXPECT_EQ(*it++, 0x03);
    EXPECT_EQ(*it++, 0x04);
    EXPECT_EQ(*it++, 0xAA);  // Inner::id (big-endian)
    EXPECT_EQ(*it++, 0xBB);
    EXPECT_EQ(*it++, 0x01);  // Inner::flag
}

TEST_F(SomeIpStrategyFixture, PerElementLayoutByteOrder)
{
    // Given a per-field wire layout supplied out-of-band (the user struct stays plain); SOME/IP scalars are packed:
    //   a -> big-endian
    //   b -> little-endian
    //   c -> big-endian
    static constexpr ElementLayout kFields[] = {
        {ByteOrder::kBigEndian, kAlign8Bits},
        {ByteOrder::kLittleEndian, kAlign8Bits},
        {ByteOrder::kBigEndian, kAlign8Bits},
    };
    const SomeIpLayout layout{{ByteOrder::kBigEndian, kAlign8Bits}, kFields, 3U};
    const SomeIpStrategy strategy{layout};
    const Serializer<SomeIpStrategy> unit{strategy};
    std::vector<std::uint8_t> buffer{};

    Measurement value{};
    value.a = static_cast<std::uint16_t>(0x1122);
    value.b = 0xAABBCCDDU;
    value.c = 0x0A0B0C0DU;

    // ... when the struct is serialized ...
    auto result = unit.Serialize(value, buffer);
    ASSERT_TRUE(result.has_value());

    // ... then each field honours its own byte order, packed ...
    ASSERT_EQ(buffer.size(), 10U);
    auto it = buffer.cbegin();
    EXPECT_EQ(*it++, 0x11);  // a big-endian           (offset 0..1)
    EXPECT_EQ(*it++, 0x22);
    EXPECT_EQ(*it++, 0xDD);  // b little-endian         (offset 2..5)
    EXPECT_EQ(*it++, 0xCC);
    EXPECT_EQ(*it++, 0xBB);
    EXPECT_EQ(*it++, 0xAA);
    EXPECT_EQ(*it++, 0x0A);  // c big-endian, packed    (offset 6..9)
    EXPECT_EQ(*it++, 0x0B);
    EXPECT_EQ(*it++, 0x0C);
    EXPECT_EQ(*it++, 0x0D);

    // ... and it deserializes back to the original values.
    Measurement decoded{};
    result = unit.DeSerialize(buffer, decoded);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(decoded.a, 0x1122);
    EXPECT_EQ(decoded.b, 0xAABBCCDDU);
    EXPECT_EQ(decoded.c, 0x0A0B0C0DU);

    EXPECT_EQ(GetMaxSize<Measurement>(layout), 2U + 4U + 4U);  // packed, no padding
}

struct FixedFields
{
    std::uint16_t a{};  // field 0
    std::uint16_t b{};  // field 1
};
SCORE_STRUCT_VISITABLE(FixedFields, a, b)

TEST_F(SomeIpStrategyFixture, FixedLengthElementsAreNeverPadded)
{
    // Given fixed-length scalar fields configured (misconfigured, really) with large alignment ...
    static constexpr ElementLayout kFields[] = {
        {ByteOrder::kBigEndian, kAlign256Bits},
        {ByteOrder::kBigEndian, kAlign256Bits},
    };
    const SomeIpLayout layout{{ByteOrder::kBigEndian, kAlign8Bits}, kFields, 2U};
    const SomeIpStrategy strategy{layout};
    const Serializer<SomeIpStrategy> unit{strategy};
    std::vector<std::uint8_t> buffer{};

    FixedFields value{};
    value.a = 0x1122;
    value.b = 0x3344;

    // ... when serialized ... then no padding is inserted behind the fixed-length elements; they stay packed.
    auto result = unit.Serialize(value, buffer);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(buffer.size(), 4U);  // two packed uint16, no padding
    auto it = buffer.cbegin();
    EXPECT_EQ(*it++, 0x11);
    EXPECT_EQ(*it++, 0x22);
    EXPECT_EQ(*it++, 0x33);
    EXPECT_EQ(*it++, 0x44);

    FixedFields decoded{};
    result = unit.DeSerialize(buffer, decoded);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(decoded.a, 0x1122);
    EXPECT_EQ(decoded.b, 0x3344);
}

}  // namespace
}  // namespace serialization
}  // namespace score
