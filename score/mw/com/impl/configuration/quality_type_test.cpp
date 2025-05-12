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
#include "score/mw/com/impl/configuration/quality_type.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl
{
namespace
{

constexpr auto kInvalidString = "kInvalid";
constexpr auto kAsilQmString = "kASIL_QM";
constexpr auto kAsilBString = "kASIL_B";

static_assert(static_cast<std::int32_t>(QualityType::kInvalid) == 0x00, "Enum values not as expected!");
static_assert(static_cast<std::int32_t>(QualityType::kASIL_QM) == 0x01, "Enum values not as expected!");
static_assert(static_cast<std::int32_t>(QualityType::kASIL_B) == 0x02, "Enum values not as expected!");

TEST(QualityType, DifferentIsNotCompatbile)
{
    EXPECT_FALSE(areCompatible(QualityType::kASIL_B, QualityType::kASIL_QM));
}

TEST(QualityType, MessageForInvalid)
{
    std::stringstream buffer1;
    buffer1 << QualityType::kInvalid;
    ASSERT_EQ(buffer1.str(), "Invalid");
}

TEST(QualityType, MessageForASIL_QM)
{
    std::stringstream buffer2;
    buffer2 << QualityType::kASIL_QM;
    ASSERT_EQ(buffer2.str(), "QM");
}

TEST(QualityType, MessageForASIL_B)
{
    std::stringstream buffer3;
    buffer3 << QualityType::kASIL_B;
    ASSERT_EQ(buffer3.str(), "B");
}

TEST(QualityType, MessageForDefault)
{
    std::stringstream buffer4;
    buffer4 << static_cast<QualityType>(42);
    ASSERT_EQ(buffer4.str(), "(unknown)");
}

class QualityTypeParamaterisedFixture : public ::testing::TestWithParam<std::tuple<std::string, QualityType>>
{
};

TEST_P(QualityTypeParamaterisedFixture, ToString)
{
    const auto param_tuple = GetParam();
    const auto quality_string = std::get<std::string>(param_tuple);
    const auto quality_type = std::get<QualityType>(param_tuple);

    EXPECT_EQ(ToString(quality_type), quality_string);
}

TEST_P(QualityTypeParamaterisedFixture, FromString)
{
    const auto param_tuple = GetParam();
    const auto quality_string = std::get<std::string>(param_tuple);
    const auto quality_type = std::get<QualityType>(param_tuple);

    EXPECT_EQ(FromString(quality_string), quality_type);
}

const std::vector<std::tuple<std::string, QualityType>> quality_type_to_string_variations{
    {kInvalidString, QualityType::kInvalid},
    {kAsilQmString, QualityType::kASIL_QM},
    {kAsilBString, QualityType::kASIL_B}};
INSTANTIATE_TEST_SUITE_P(QualityTypeParamaterisedFixture,
                         QualityTypeParamaterisedFixture,
                         ::testing::ValuesIn(quality_type_to_string_variations));

TEST(QualityTypeDeathTest, ToString)
{
    EXPECT_DEATH(ToString(static_cast<QualityType>(10)), ".*");
}

TEST(QualityTypeDeathTest, FromString)
{
    EXPECT_DEATH(FromString("Invalid String"), ".*");
}

}  // namespace
}  // namespace score::mw::com::impl
