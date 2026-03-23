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
#include "score/memory/string_comparison_adaptor.h"

#include "score/memory/string_literal.h"

#include <score/assert.hpp>
#include <score/string_view.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <type_traits>

namespace score::memory
{
namespace
{

using ::testing::Eq;
using ::testing::IsTrue;
using ::testing::Ne;
using ::testing::StrEq;

/// Set of specialised template functions which allows creating a std::string, std::string_view, score::cpp::string_view or
/// score::StringLiteral in a generic way, solely based on the template type. We use specialized template functions
/// instead of overloading, since the signatures only differ in the return type.
template <typename T>
T CreateUnderlyingString(std::string_view /* string_view */)
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_MESSAGE(false, "There is no default implementation. Only specialized functions should be called.");
    return T{};
}

template <>
std::string CreateUnderlyingString(const std::string_view string_view)
{
    return std::string(string_view);
}

template <>
std::string_view CreateUnderlyingString(const std::string_view string_view)
{
    return string_view;
}

template <>
score::cpp::string_view CreateUnderlyingString(const std::string_view string_view)
{
    return {string_view.data(), string_view.size()};
}

template <>
score::StringLiteral CreateUnderlyingString(const std::string_view string_view)
{
    return string_view.data();
}

template <typename T>
class StringComparisonAdaptorFixture : public ::testing::Test
{
  public:
};

// Gtest will run all tests in the StringComparisonAdaptorFixture once for every type, t, in MyTypes, such that
// TypeParam == t for each run.
using MyTypes = ::testing::Types<std::string, score::cpp::string_view, std::string_view, score::StringLiteral>;
TYPED_TEST_SUITE(StringComparisonAdaptorFixture, MyTypes, );

TEST(StringComparisonAdaptorHelpersFixture, CreateUnderlyingStringReturnCorrectValues)
{
    EXPECT_THAT(CreateUnderlyingString<std::string>("test_string"), "test_string");
    EXPECT_THAT(CreateUnderlyingString<std::string_view>("test_string").data(), StrEq("test_string"));
    EXPECT_THAT(CreateUnderlyingString<score::cpp::string_view>("test_string").data(), StrEq("test_string"));
    EXPECT_THAT(CreateUnderlyingString<score::StringLiteral>("test_string"), StrEq("test_string"));
}

TYPED_TEST(StringComparisonAdaptorFixture, CanBeConvertedImplicitly)
{
    EXPECT_THAT((std::is_convertible<TypeParam, StringComparisonAdaptor>::value), IsTrue());
}

TYPED_TEST(StringComparisonAdaptorFixture, CanBeCopyConstructed)
{
    const auto underlying_string = CreateUnderlyingString<TypeParam>("b");
    StringComparisonAdaptor adaptor{underlying_string};
    EXPECT_THAT(adaptor, Eq(underlying_string));
}

TYPED_TEST(StringComparisonAdaptorFixture, CanBeCopyAssigned)
{
    StringComparisonAdaptor adaptor{"a"};
    const auto underlying_string = CreateUnderlyingString<TypeParam>("b");
    adaptor = underlying_string;
    EXPECT_THAT(adaptor, Eq(underlying_string));
}

TYPED_TEST(StringComparisonAdaptorFixture, GetStringViewReturnsValidStringView)
{
    const auto underlying_string = CreateUnderlyingString<TypeParam>("a");
    StringComparisonAdaptor adaptor{underlying_string};

    EXPECT_THAT(adaptor.GetAsStringView(), Eq(score::cpp::string_view{"a"}));
}

TYPED_TEST(StringComparisonAdaptorFixture, ComparisonReturnsTrueForSameContent)
{
    const auto underlying_string = CreateUnderlyingString<TypeParam>("a");
    StringComparisonAdaptor adaptor{underlying_string};
    EXPECT_THAT(adaptor, Eq("a"));
}

TYPED_TEST(StringComparisonAdaptorFixture, ComparisonReturnsFalseForDifferentContent)
{
    const auto underlying_string_1 = CreateUnderlyingString<TypeParam>("a");
    StringComparisonAdaptor adaptor{underlying_string_1};

    const auto underlying_string_2 = CreateUnderlyingString<TypeParam>("b");
    EXPECT_THAT(adaptor, Ne(underlying_string_2));
}

TYPED_TEST(StringComparisonAdaptorFixture, HashIsSameForTwoEqualAdaptors)
{
    const auto underlying_string_1 = CreateUnderlyingString<TypeParam>("a");
    StringComparisonAdaptor adaptor1{underlying_string_1};

    const auto underlying_string_2 = CreateUnderlyingString<TypeParam>("a");
    StringComparisonAdaptor adaptor2{underlying_string_2};
    EXPECT_THAT(std::hash<StringComparisonAdaptor>{}(adaptor1), Eq(std::hash<StringComparisonAdaptor>{}(adaptor2)));
}

TEST(StringComparisonAdaptor, CanBeCopyConstructedWithAdaptor)
{
    StringComparisonAdaptor adaptor1{"a"};
    StringComparisonAdaptor adaptor2{adaptor1};

    EXPECT_THAT(adaptor2, Eq(StringComparisonAdaptor{"a"}));

    adaptor2 = "b";

    EXPECT_THAT(adaptor1, Eq(StringComparisonAdaptor{"a"}));
    EXPECT_THAT(adaptor2, Eq(StringComparisonAdaptor{"b"}));
}

TEST(StringComparisonAdaptor, CanBeMoveConstructedWithAdaptor)
{
    StringComparisonAdaptor adaptor1{"a"};
    StringComparisonAdaptor adaptor2{std::move(adaptor1)};

    EXPECT_THAT(adaptor2, Eq(StringComparisonAdaptor{"a"}));
}

TEST(StringComparisonAdaptorWithString, CanBeMoveConstructed)
{
    std::string str{"b"};
    StringComparisonAdaptor adaptor{std::move(str)};
    EXPECT_THAT(adaptor, Eq(std::string{"b"}));
}

TEST(StringComparisonAdaptorWithString, CanBeMoveAssigned)
{
    StringComparisonAdaptor adaptor{"a"};
    std::string str{"b"};
    adaptor = std::move(str);
    EXPECT_THAT(adaptor, Eq(std::string{"b"}));
}

TEST(StringComparisonAdaptorWithDifferentContentTypes, ComparisonWorksBetweenStringAndStringView)
{
    std::string str{"a"};
    StringComparisonAdaptor adaptor{str};

    score::cpp::string_view str_view{str};
    EXPECT_THAT(adaptor, Eq(str_view));
}

TEST(StringComparisonAdaptorWithDifferentContentTypes, ComparisonWorksBetweenStringAndCString)
{
    std::string str{"a"};
    StringComparisonAdaptor adaptor{str};

    EXPECT_THAT(adaptor, Eq("a"));
}

TEST(StringComparisonAdaptorWithDifferentContentTypes, ComparisonWorksBetweenStringViewAndString)
{
    std::string str{"a"};
    score::cpp::string_view str_view{str};
    StringComparisonAdaptor adaptor{str_view};

    EXPECT_THAT(adaptor, Eq(str));
}

TEST(StringComparisonAdaptorWithDifferentContentTypes, ComparisonWorksBetweenStringViewAndCString)
{
    std::string str{"a"};
    score::cpp::string_view str_view{str};
    StringComparisonAdaptor adaptor{str_view};

    EXPECT_THAT(adaptor, Eq("a"));
}

TEST(StringComparisonAdaptorWithDifferentContentTypes, ComparisonWorksBetweenCStringAndString)
{
    StringComparisonAdaptor adaptor{"a"};

    std::string str{"a"};
    EXPECT_THAT(adaptor, Eq(str));
}

TEST(StringComparisonAdaptorWithDifferentContentTypes, ComparisonWorksBetweenCStringAndStringView)
{
    StringComparisonAdaptor adaptor{"a"};

    std::string str{"a"};
    score::cpp::string_view str_view{str};
    EXPECT_THAT(adaptor, Eq(str_view));
}

TEST(StringComparisonAdaptorWithDifferentContentTypes, HashIsSameForEqualStringAndStringView)
{
    std::string str1{"a"};
    StringComparisonAdaptor adaptor1{str1};

    std::string str2{"a"};
    score::cpp::string_view str_view{str2};
    StringComparisonAdaptor adaptor2{str_view};
    EXPECT_THAT(std::hash<StringComparisonAdaptor>{}(adaptor1), Eq(std::hash<StringComparisonAdaptor>{}(adaptor2)));
}

TEST(StringComparisonAdaptorWithDifferentContentTypes, HashIsSameForEqualStringAndCString)
{
    std::string str1{"a"};
    StringComparisonAdaptor adaptor1{str1};

    StringComparisonAdaptor adaptor2{"a"};
    EXPECT_THAT(std::hash<StringComparisonAdaptor>{}(adaptor1), Eq(std::hash<StringComparisonAdaptor>{}(adaptor2)));
}

TEST(StringComparisonAdaptorWithDifferentContentTypes, HashIsSameForEqualStringViewAndString)
{
    std::string str1{"a"};
    score::cpp::string_view str_view1{str1};
    StringComparisonAdaptor adaptor1{str_view1};

    std::string str2{"a"};
    StringComparisonAdaptor adaptor2{str2};
    EXPECT_THAT(std::hash<StringComparisonAdaptor>{}(adaptor1), Eq(std::hash<StringComparisonAdaptor>{}(adaptor2)));
}

TEST(StringComparisonAdaptorWithDifferentContentTypes, HashIsSameForEqualStringViewAndCString)
{
    std::string str1{"a"};
    score::cpp::string_view str_view1{str1};
    StringComparisonAdaptor adaptor1{str_view1};

    StringComparisonAdaptor adaptor2{"a"};
    EXPECT_THAT(std::hash<StringComparisonAdaptor>{}(adaptor1), Eq(std::hash<StringComparisonAdaptor>{}(adaptor2)));
}

TEST(StringComparisonAdaptorWithDifferentContentTypes, HashIsSameForCStringViewAndString)
{
    StringComparisonAdaptor adaptor1{"a"};

    std::string str{"a"};
    StringComparisonAdaptor str_adaptor{str};
    EXPECT_THAT(std::hash<StringComparisonAdaptor>{}(adaptor1), Eq(std::hash<StringComparisonAdaptor>{}(str_adaptor)));
}

TEST(StringComparisonAdaptorWithDifferentContentTypes, HashIsSameForEqualCStringAndStringView)
{
    StringComparisonAdaptor adaptor1{"a"};

    std::string string{"a"};
    score::cpp::string_view string_view{string};
    StringComparisonAdaptor string_view_adaptor{string_view};
    EXPECT_THAT(std::hash<StringComparisonAdaptor>{}(adaptor1),
                Eq(std::hash<StringComparisonAdaptor>{}(string_view_adaptor)));
}

TEST(StringComparisonAdaptorLessComparison, LessThan)
{
    StringComparisonAdaptor adaptor1{"ab"};
    StringComparisonAdaptor adaptor2{"ac"};
    EXPECT_EQ(adaptor1 < adaptor2, true);
}

}  // namespace
}  // namespace score::memory
