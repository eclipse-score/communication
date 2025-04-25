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
#include "score/memory/split_string_view.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace score::cpp::literals::string_view_literals;

namespace score::memory
{

namespace
{

using StringSequence = std::vector<score::cpp::string_view>;

const auto kSeperator = '|';

std::string Join(const StringSequence& string_sequence)
{
    std::string result{};
    for (score::cpp::string_view substring : string_sequence)
    {
        result.append(std::string{substring.begin(), substring.size()});
        result.push_back(kSeperator);
    }

    if (!result.empty())
    {
        // Remove trailing seperator
        result.pop_back();
    }

    return result;
}

StringSequence GetSplitSequence(std::string input)
{
    StringSequence seq{};
    LazySplitStringView splitter(input, kSeperator);
    for (const auto& segment : splitter)
    {
        seq.push_back(segment);
    }
    return seq;
}

void ExpectEqualSequences(const StringSequence& lhs, const StringSequence& rhs)
{
    EXPECT_TRUE(std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end()));
}

TEST(StringSplitterTests, EmptyStringShallReturnEmptyRange)
{
    LazySplitStringView splitter{"", kSeperator};
    EXPECT_TRUE(splitter.begin() == splitter.end());
}

TEST(StringSplitterTests, NoSeperatorShallReturnOneItem)
{
    StringSequence seq{"Hello World"_sv};
    ExpectEqualSequences(GetSplitSequence(Join(seq)), seq);
}

TEST(StringSplitterTests, OneSeperatorShallReturnTwoItems)
{
    StringSequence seq{"Hello World"_sv, "Foobar"_sv};

    ExpectEqualSequences(GetSplitSequence(Join(seq)), seq);
}

TEST(StringSplitterTests, SeperatorAtBeginShallReturnEmptyString)
{
    StringSequence seq{""_sv, "Hello World"_sv, "Foobar"_sv};

    ExpectEqualSequences(GetSplitSequence(Join(seq)), seq);
}

TEST(StringSplitterTests, SeperatorAtEndShallBeDiscarded)
{
    StringSequence seq{"Hello World"_sv, "Foobar"_sv};

    ExpectEqualSequences(GetSplitSequence(Join(seq) + kSeperator), seq);
}

TEST(StringSplitterTests, SeperatorOnlyStringShallReturnEmptySubstring)
{
    ExpectEqualSequences(GetSplitSequence(std::string{"|"}), StringSequence{""_sv});
}

TEST(StringSplitterTests, TwoSeperatorsShallReturnTwoEmptySubstring)
{
    ExpectEqualSequences(GetSplitSequence(std::string{"||"}), StringSequence{""_sv, ""_sv});
}

TEST(StringSplitterTests, MultipleSeperatorsInRowShallReturnEmptySubstring)
{
    ExpectEqualSequences(GetSplitSequence(std::string{"Foo||Bar"}), StringSequence{"Foo"_sv, ""_sv, "Bar"_sv});
}

}  // namespace

}  // namespace score::memory
