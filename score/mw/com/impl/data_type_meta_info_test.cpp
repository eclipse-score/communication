/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
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
#include "score/mw/com/impl/data_type_meta_info.h"

#include "score/mw/com/impl/com_error.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl
{
namespace
{

TEST(MakeDataTypeSizeInfoTest, ConvertsSizeMultipleOfAlignment)
{
    // Given a valid meta-info (size multiple of a power-of-two alignment)
    const DataTypeMetaInfo meta_info{16U, 8U};

    // When converting it
    const auto result = MakeDataTypeSizeInfo(meta_info);

    // Then the conversion succeeds and preserves size and alignment
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().Size(), 16U);
    EXPECT_EQ(result.value().Alignment(), 8U);
}

TEST(MakeDataTypeSizeInfoTest, ConvertsSizeEqualToAlignment)
{
    // Given a meta-info where size equals alignment
    const DataTypeMetaInfo meta_info{8U, 8U};

    // When converting it
    const auto result = MakeDataTypeSizeInfo(meta_info);

    // Then the conversion succeeds and preserves size and alignment
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().Size(), 8U);
    EXPECT_EQ(result.value().Alignment(), 8U);
}

TEST(MakeDataTypeSizeInfoTest, RejectsZeroAlignment)
{
    // Given a meta-info with zero alignment
    const DataTypeMetaInfo meta_info{16U, 0U};

    // When converting it
    const auto result = MakeDataTypeSizeInfo(meta_info);

    // Then the conversion fails with kInvalidConfiguration
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kInvalidConfiguration);
}

TEST(MakeDataTypeSizeInfoTest, RejectsNonPowerOfTwoAlignment)
{
    // Given a meta-info with a non-power-of-two alignment
    const DataTypeMetaInfo meta_info{24U, 6U};

    // When converting it
    const auto result = MakeDataTypeSizeInfo(meta_info);

    // Then the conversion fails with kInvalidConfiguration
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kInvalidConfiguration);
}

TEST(MakeDataTypeSizeInfoTest, RejectsSizeNotMultipleOfAlignment)
{
    // Given a meta-info whose size is not a multiple of its power-of-two alignment
    const DataTypeMetaInfo meta_info{10U, 8U};

    // When converting it
    const auto result = MakeDataTypeSizeInfo(meta_info);

    // Then the conversion fails with kInvalidConfiguration
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kInvalidConfiguration);
}

}  // namespace
}  // namespace score::mw::com::impl
