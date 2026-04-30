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
#include "score/mw/com/test/common_test_resources/command_line_parser.h"

#include <cstdint>
#include <string>

#include <gtest/gtest.h>

namespace score::mw::com::test
{
namespace
{

TEST(GetValueIfProvidedTest, StringCanBeParsed)
{
    const std::string expected_value = "arg_value";
    auto parsed_value = GetValueIfProvided<std::string>({{"test_arg", expected_value}}, "test_arg");

    ASSERT_EQ(expected_value, parsed_value.value());
}

TEST(GetValueIfProvidedTest, IntegerCanBeParsed)
{
    const int expected_value{42};
    auto parsed_value = GetValueIfProvided<int>({{"test_arg", std::to_string(expected_value)}}, "test_arg");

    ASSERT_EQ(expected_value, parsed_value.value());
}

TEST(GetValueIfProvidedTest, BoolTrueCanBeParsed)
{
    const bool expected_value{true};
    const std::string value_str_representation{"true"};
    auto parsed_value = GetValueIfProvided<bool>({{"test_arg", value_str_representation}}, "test_arg");

    ASSERT_EQ(expected_value, parsed_value.value());
}

TEST(GetValueIfProvidedTest, Bool1CanBeParsed)
{
    const bool expected_value{true};
    const std::string value_str_representation{"1"};
    auto parsed_value = GetValueIfProvided<bool>({{"test_arg", value_str_representation}}, "test_arg");

    ASSERT_EQ(expected_value, parsed_value.value());
}

TEST(GetValueIfProvidedTest, BoolFalseCanBeParsed)
{
    const bool expected_value{false};
    const std::string value_str_representation{"false"};
    auto parsed_value = GetValueIfProvided<bool>({{"test_arg", value_str_representation}}, "test_arg");

    ASSERT_EQ(expected_value, parsed_value.value());
}

TEST(GetValueIfProvidedTest, Bool0CanBeParsed)
{
    const bool expected_value{false};
    const std::string value_str_representation{"0"};
    auto parsed_value = GetValueIfProvided<bool>({{"test_arg", value_str_representation}}, "test_arg");

    ASSERT_EQ(expected_value, parsed_value.value());
}

TEST(GetValueIfProvidedTest, MultipleArgumentsCanBeParsed)
{
    const std::string expected_value1{"arg_value"};
    const std::int16_t expected_value2{-2};
    const bool expected_value3 = false;
    const std::unordered_map<std::string, std::string> arg_dict{{"test_arg1", expected_value1},
                                                                {"test_arg2", std::to_string(expected_value2)},
                                                                {"test_arg3", "false"},
                                                                {"unparsed_arg", "bla"}};

    auto parsed_value1 = GetValueIfProvided<std::string>(arg_dict, "test_arg1");
    auto parsed_value2 = GetValueIfProvided<std::int16_t>(arg_dict, "test_arg2");
    auto parsed_value3 = GetValueIfProvided<bool>(arg_dict, "test_arg3");

    ASSERT_EQ(expected_value1, parsed_value1.value());
    ASSERT_EQ(expected_value2, parsed_value2.value());
    ASSERT_EQ(expected_value3, parsed_value3.value());
}

}  // namespace
}  // namespace score::mw::com::test
