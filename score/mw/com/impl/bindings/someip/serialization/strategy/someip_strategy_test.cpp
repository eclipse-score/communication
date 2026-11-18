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

struct DeepLeaf
{
    std::uint16_t id{};
    std::uint8_t flag{};
};
SCORE_STRUCT_VISITABLE(DeepLeaf, id, flag)

struct DeepMiddle
{
    std::uint32_t value{};
    DeepLeaf leaf{};
};
SCORE_STRUCT_VISITABLE(DeepMiddle, value, leaf)

struct DeepOuter
{
    std::uint32_t value{};
    DeepMiddle middle{};
};
SCORE_STRUCT_VISITABLE(DeepOuter, value, middle)

constexpr ElementLayout WithNestedLayout(const ElementLayout layout, const SomeIpLayout* const nested)
{
    return {layout.byte_order, layout.alignment_bits, layout.length_field_bytes, nested};
}

DeepOuter MakeDeepOuter()
{
    DeepOuter value{};
    value.value = 0x01020304U;
    value.middle.value = 0x05060708U;
    value.middle.leaf.id = 0xAABB;
    value.middle.leaf.flag = 0x01;
    return value;
}

void ExpectEqual(const DeepMiddle& actual, const DeepMiddle& expected)
{
    EXPECT_EQ(actual.value, expected.value);
    EXPECT_EQ(actual.leaf.id, expected.leaf.id);
    EXPECT_EQ(actual.leaf.flag, expected.leaf.flag);
}

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

TEST_F(SomeIpStrategyFixture, NestedStructUsesOneCustomLayoutAcrossTwoLevels)
{
    // Given one custom layout shared by all fields across two nested struct levels ...
    static constexpr ElementLayout kCustomFieldLayout{ByteOrder::kLittleEndian, kAlign8Bits};
    static constexpr SomeIpLayout kLeafLayout{kCustomFieldLayout, nullptr, 0U};
    static constexpr ElementLayout kMiddleFields[] = {
        kCustomFieldLayout,
        WithNestedLayout(kCustomFieldLayout, &kLeafLayout),
    };
    const SomeIpLayout layout{kCustomFieldLayout, kMiddleFields, 2U};
    const Serializer<SomeIpStrategy> unit{SomeIpStrategy{layout}};
    std::vector<std::uint8_t> buffer{};
    const DeepMiddle value = MakeDeepOuter().middle;

    // ... when the struct is serialized ...
    auto result = unit.Serialize(value, buffer);
    ASSERT_TRUE(result.has_value());

    // ... then the custom little-endian layout is used at both levels ...
    const std::vector<std::uint8_t> expected{0x08, 0x07, 0x06, 0x05, 0xBB, 0xAA, 0x01};
    EXPECT_EQ(buffer, expected);

    // ... and it deserializes back to the original value.
    DeepMiddle decoded{};
    result = unit.DeSerialize(buffer, decoded);
    ASSERT_TRUE(result.has_value());
    ExpectEqual(decoded, value);
    EXPECT_EQ(GetMaxSize<DeepMiddle>(layout), 7U);
}

TEST_F(SomeIpStrategyFixture, NestedStructUsesOneCustomLayoutAcrossThreeLevels)
{
    // Given one custom layout shared by all fields across three nested struct levels ...
    static constexpr ElementLayout kCustomFieldLayout{ByteOrder::kLittleEndian, kAlign8Bits};
    static constexpr SomeIpLayout kLeafLayout{kCustomFieldLayout, nullptr, 0U};
    static constexpr ElementLayout kMiddleFields[] = {
        kCustomFieldLayout,
        WithNestedLayout(kCustomFieldLayout, &kLeafLayout),
    };
    static constexpr SomeIpLayout kMiddleLayout{kCustomFieldLayout, kMiddleFields, 2U};
    static constexpr ElementLayout kOuterFields[] = {
        kCustomFieldLayout,
        WithNestedLayout(kCustomFieldLayout, &kMiddleLayout),
    };
    const SomeIpLayout layout{kCustomFieldLayout, kOuterFields, 2U};
    const Serializer<SomeIpStrategy> unit{SomeIpStrategy{layout}};
    std::vector<std::uint8_t> buffer{};
    const DeepOuter value = MakeDeepOuter();

    // ... when the struct is serialized ...
    auto result = unit.Serialize(value, buffer);
    ASSERT_TRUE(result.has_value());

    // ... then the custom little-endian layout is used at all three levels ...
    const std::vector<std::uint8_t> expected{0x04, 0x03, 0x02, 0x01, 0x08, 0x07, 0x06, 0x05, 0xBB, 0xAA, 0x01};
    EXPECT_EQ(buffer, expected);

    // ... and it deserializes back to the original value.
    DeepOuter decoded{};
    result = unit.DeSerialize(buffer, decoded);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(decoded.value, value.value);
    ExpectEqual(decoded.middle, value.middle);
    EXPECT_EQ(GetMaxSize<DeepOuter>(layout), 11U);
}

TEST_F(SomeIpStrategyFixture, NestedStructUsesConfigurableDefaultWhenLayoutIsMissing)
{
    // Given no layout tree, only a little-endian fallback ...
    const FallbackLayout default_layout{ByteOrder::kLittleEndian, kAlign8Bits};
    const Serializer<SomeIpStrategy> unit{SomeIpStrategy{default_layout}};
    std::vector<std::uint8_t> buffer{};
    const DeepOuter value = MakeDeepOuter();

    // ... when a three-level nested struct is serialized ...
    auto result = unit.Serialize(value, buffer);
    ASSERT_TRUE(result.has_value());

    // ... then every level uses the fallback ...
    const std::vector<std::uint8_t> expected{0x04, 0x03, 0x02, 0x01, 0x08, 0x07, 0x06, 0x05, 0xBB, 0xAA, 0x01};
    EXPECT_EQ(buffer, expected);

    // ... and it deserializes back to the original value.
    DeepOuter decoded{};
    result = unit.DeSerialize(buffer, decoded);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(decoded.value, value.value);
    ExpectEqual(decoded.middle, value.middle);
    EXPECT_EQ(GetMaxSize<DeepOuter>(default_layout), 11U);
}

TEST_F(SomeIpStrategyFixture, MissingNestedLayoutUsesConfigurableDefault)
{
    // Given a big-endian layout whose nested struct field has no nested layout, plus a little-endian fallback ...
    static constexpr ElementLayout kBigEndian{ByteOrder::kBigEndian, kAlign8Bits};
    static constexpr ElementLayout kMiddleFields[] = {kBigEndian, kBigEndian};
    const SomeIpLayout layout{kBigEndian, kMiddleFields, 2U};
    const FallbackLayout default_layout{ByteOrder::kLittleEndian, kAlign8Bits};
    const Serializer<SomeIpStrategy> unit{SomeIpStrategy{layout, default_layout}};
    std::vector<std::uint8_t> buffer{};
    const DeepMiddle value = MakeDeepOuter().middle;

    // ... when the struct is serialized ...
    auto result = unit.Serialize(value, buffer);
    ASSERT_TRUE(result.has_value());

    // ... then explicit fields are big-endian and the unlayouted sub-struct uses the fallback ...
    const std::vector<std::uint8_t> expected{0x05, 0x06, 0x07, 0x08, 0xBB, 0xAA, 0x01};
    EXPECT_EQ(buffer, expected);

    // ... and it deserializes back to the original value.
    DeepMiddle decoded{};
    result = unit.DeSerialize(buffer, decoded);
    ASSERT_TRUE(result.has_value());
    ExpectEqual(decoded, value);
    EXPECT_EQ(GetMaxSize<DeepMiddle>(layout, default_layout), 7U);
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

TEST_F(SomeIpStrategyFixture, TruncatedScalarInputReturnsError)
{
    // Given a buffer one byte shorter than a serialized Measurement ...
    const SomeIpStrategy strategy{};
    const Serializer<SomeIpStrategy> unit{strategy};
    const std::size_t truncated_size = GetMaxSize<Measurement>() - 1U;
    std::vector<std::uint8_t> buffer(truncated_size, std::uint8_t{});

    // ... when it is deserialized ...
    Measurement decoded{};
    const auto result = unit.DeSerialize(buffer, decoded);

    // ... then the error is reported instead of silently succeeding.
    EXPECT_FALSE(result.has_value());
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
