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
#include "score/memory/any_string_view.h"

#include <gtest/gtest.h>

namespace score::memory
{
namespace
{

class AnyStringViewFixture : public ::testing::Test
{
  public:
    std::string_view std_string_view_{"foo"};
    score::cpp::string_view score_string_view_{"foo"};
    std::string string_{"foo"};
};

TEST_F(AnyStringViewFixture, ConvertStdToStd)
{
    std::string_view unit = AnyStringView{std_string_view_};
    EXPECT_EQ(unit, std_string_view_);
}

TEST_F(AnyStringViewFixture, ConvertStdToAmp)
{
    score::cpp::string_view unit = AnyStringView{std_string_view_};
    EXPECT_EQ(unit, score_string_view_);
}

TEST_F(AnyStringViewFixture, ConvertAmpToAmp)
{
    score::cpp::string_view unit = AnyStringView{score_string_view_};
    EXPECT_EQ(unit, score_string_view_);
}

TEST_F(AnyStringViewFixture, ConvertAmpToStd)
{
    std::string_view unit = AnyStringView{score_string_view_};
    EXPECT_EQ(unit, std_string_view_);
}

TEST_F(AnyStringViewFixture, ConvertLiteralToStd)
{
    std::string_view unit = AnyStringView{"foo"};
    EXPECT_EQ(unit, std_string_view_);
}

TEST_F(AnyStringViewFixture, ConvertLiteralToAmp)
{
    score::cpp::string_view unit = AnyStringView{"foo"};
    EXPECT_EQ(unit, score_string_view_);
}

TEST_F(AnyStringViewFixture, ConvertStringToAmp)
{
    score::cpp::string_view unit = AnyStringView{string_};
    EXPECT_EQ(unit, score_string_view_);
}

TEST_F(AnyStringViewFixture, ConvertStringToStd)
{
    std::string_view unit = AnyStringView{string_};
    EXPECT_EQ(unit, std_string_view_);
}

std::string_view constexpr SomeFunction(AnyStringView unit)
{
    return unit;
}

TEST_F(AnyStringViewFixture, ConvertImplicitStringToStd)
{
    std::string_view unit = SomeFunction(string_);
    EXPECT_EQ(unit, std_string_view_);
}

TEST_F(AnyStringViewFixture, ConvertImplicitStdToStd)
{
    std::string_view unit = SomeFunction(std_string_view_);
    EXPECT_EQ(unit, std_string_view_);
}

TEST_F(AnyStringViewFixture, ConvertImplicitAmpToStd)
{
    std::string_view unit = SomeFunction(score_string_view_);
    EXPECT_EQ(unit, std_string_view_);
}

TEST_F(AnyStringViewFixture, ConvertImplicitLiteralToStd)
{
    std::string_view unit = SomeFunction("foo");
    EXPECT_EQ(unit, std_string_view_);
}

}  // namespace
}  // namespace score::memory
