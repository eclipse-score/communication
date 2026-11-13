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
#include "score/mw/com/impl/bindings/someip/serialization/strategy/someip_layout.h"

#include <gtest/gtest.h>

namespace score
{
namespace serialization
{
namespace someip_detail
{
namespace
{

void ExpectDefault(const ElementLayout& leaf)
{
    EXPECT_EQ(leaf.byte_order, ByteOrder::kBigEndian);
    EXPECT_EQ(leaf.alignment_bits, kDefaultAlignmentBits);
    EXPECT_EQ(leaf.length_field_bytes, kDefaultLengthFieldBytes);
    EXPECT_EQ(leaf.nested, nullptr);
}

TEST(SomeIpLengthFieldBytesTest, AcceptsWidthsPermittedBySomeIp)
{
    EXPECT_TRUE(IsValidLengthFieldBytes(0U));
    EXPECT_TRUE(IsValidLengthFieldBytes(sizeof(std::uint8_t)));
    EXPECT_TRUE(IsValidLengthFieldBytes(sizeof(std::uint16_t)));
    EXPECT_TRUE(IsValidLengthFieldBytes(sizeof(std::uint32_t)));
}

TEST(SomeIpLengthFieldBytesTest, RejectsOtherWidths)
{
    EXPECT_FALSE(IsValidLengthFieldBytes(sizeof(std::uint16_t) + 1U));
    EXPECT_FALSE(IsValidLengthFieldBytes(sizeof(std::uint32_t) + 1U));
    EXPECT_FALSE(IsValidLengthFieldBytes(sizeof(std::uint64_t)));
}

TEST(SomeIpLayoutCursorTest, RootCursorUsesLayoutDefault)
{
    const SomeIpLayout layout{{ByteOrder::kLittleEndian, kAlign32Bits, 2U, nullptr}, nullptr, 0U};

    const LayoutCursor cursor = RootCursor(layout);

    EXPECT_EQ(cursor.struct_layout, &layout);
    EXPECT_EQ(cursor.leaf.byte_order, ByteOrder::kLittleEndian);
    EXPECT_EQ(cursor.leaf.alignment_bits, kAlign32Bits);
    EXPECT_EQ(cursor.leaf.length_field_bytes, 2U);
}

TEST(SomeIpLayoutCursorTest, ElementAtNullLayoutReturnsDefault)
{
    ExpectDefault(ElementAt(nullptr, 0U));
}

TEST(SomeIpLayoutCursorTest, ElementAtReturnsPerFieldOverride)
{
    static constexpr ElementLayout kFields[] = {{ByteOrder::kBigEndian, kAlign8Bits},
                                                {ByteOrder::kLittleEndian, kAlign64Bits}};
    const SomeIpLayout layout{{ByteOrder::kBigEndian, kAlign16Bits}, kFields, 2U};

    const ElementLayout leaf = ElementAt(&layout, 1U);

    EXPECT_EQ(leaf.byte_order, ByteOrder::kLittleEndian);
    EXPECT_EQ(leaf.alignment_bits, kAlign64Bits);
}

TEST(SomeIpLayoutCursorTest, ElementAtOutOfRangeIndexFallsBackToLayoutDefault)
{
    static constexpr ElementLayout kFields[] = {{ByteOrder::kLittleEndian, kAlign64Bits}};
    const SomeIpLayout layout{{ByteOrder::kBigEndian, kAlign16Bits}, kFields, 1U};

    const ElementLayout leaf = ElementAt(&layout, 5U);

    EXPECT_EQ(leaf.byte_order, ByteOrder::kBigEndian);
    EXPECT_EQ(leaf.alignment_bits, kAlign16Bits);
}

TEST(SomeIpLayoutCursorTest, ElementAtWithoutPerFieldReturnsLayoutDefault)
{
    const SomeIpLayout layout{{ByteOrder::kLittleEndian, kAlign32Bits}, nullptr, 0U};

    const ElementLayout leaf = ElementAt(&layout, 0U);

    EXPECT_EQ(leaf.byte_order, ByteOrder::kLittleEndian);
    EXPECT_EQ(leaf.alignment_bits, kAlign32Bits);
}

TEST(SomeIpLayoutCursorTest, EnterFieldDescendsIntoNestedLayout)
{
    static constexpr SomeIpLayout kInner{{ByteOrder::kLittleEndian, kAlign8Bits}, nullptr, 0U};
    static constexpr ElementLayout kFields[] = {{ByteOrder::kBigEndian, kAlign32Bits, 1U, &kInner}};
    const SomeIpLayout layout{{}, kFields, 1U};

    const LayoutCursor cursor = EnterField(RootCursor(layout), 0U);

    EXPECT_EQ(cursor.struct_layout, &kInner);
    EXPECT_EQ(cursor.leaf.alignment_bits, kAlign32Bits);
    EXPECT_EQ(cursor.leaf.length_field_bytes, 1U);
}

TEST(SomeIpLayoutCursorTest, EnterFieldOfNullNestedYieldsDefaultsNotParent)
{
    static constexpr ElementLayout kFields[] = {{ByteOrder::kLittleEndian, kAlign32Bits, 1U, nullptr}};
    const SomeIpLayout layout{{}, kFields, 1U};

    const LayoutCursor field = EnterField(RootCursor(layout), 0U);
    const LayoutCursor grandchild = EnterField(field, 0U);

    EXPECT_EQ(grandchild.struct_layout, nullptr);
    ExpectDefault(grandchild.leaf);
}

TEST(SomeIpLayoutCursorTest, EnterArrayElementUsesNestedDefault)
{
    static constexpr SomeIpLayout kElement{{ByteOrder::kLittleEndian, kAlign16Bits}, nullptr, 0U};
    const SomeIpLayout layout{{ByteOrder::kBigEndian, kAlign8Bits, 2U, &kElement}, nullptr, 0U};

    const LayoutCursor cursor = EnterArrayElement(RootCursor(layout));

    EXPECT_EQ(cursor.struct_layout, &kElement);
    EXPECT_EQ(cursor.leaf.byte_order, ByteOrder::kLittleEndian);
    EXPECT_EQ(cursor.leaf.alignment_bits, kAlign16Bits);
}

TEST(SomeIpLayoutCursorTest, EnterArrayElementOfNullNestedYieldsDefaultsNotParent)
{
    const SomeIpLayout layout{{ByteOrder::kLittleEndian, kAlign64Bits, 1U, nullptr}, nullptr, 0U};

    const LayoutCursor cursor = EnterArrayElement(RootCursor(layout));

    EXPECT_EQ(cursor.struct_layout, nullptr);
    ExpectDefault(cursor.leaf);
}

}  // namespace
}  // namespace someip_detail
}  // namespace serialization
}  // namespace score
