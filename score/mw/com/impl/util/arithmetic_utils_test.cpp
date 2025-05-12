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
#include "score/mw/com/impl/util/arithmetic_utils.h"

#include <score/utility.hpp>

#include <gtest/gtest.h>
#include <atomic>
#include <cstdint>
#include <limits>

namespace score::mw::com::impl
{

namespace
{

TEST(ArithmeticUtilsAdditionTest, AddWithoutOverflowReturnsCorrectValueWhenArgumentsDontOverflow)
{
    // When calculating the sum of two values that should not overflow
    constexpr std::uint32_t lhs = 100U;
    constexpr std::uint32_t rhs = 200U;
    constexpr auto result = add_without_overflow<std::uint32_t, lhs, rhs>();

    // Then the result is as expected
    EXPECT_EQ(result, 300U);
}

TEST(ArithmeticUtilsAdditionUInt8Test, AddWithoutOverflowReturnsCorrectValueWhenArgumentsDontOverflow)
{
    // When calculating the sum of two values that should not overflow
    constexpr std::uint8_t lhs = 100U;
    constexpr std::uint8_t rhs = 50U;
    constexpr auto result = add_without_overflow<std::uint8_t, lhs, rhs>();

    // Then the result is as expected
    EXPECT_EQ(result, 150U);
}

TEST(ArithmeticUtilsAdditionTest, AddWithoutOverflowReturnsCorrectValueWhenArgumentsAreMaxPossible)
{
    // When calculating the sum of two values that should not overflow
    constexpr std::uint32_t lhs = std::numeric_limits<std::uint32_t>::max() - 1U;
    constexpr std::uint32_t rhs = 1U;
    constexpr auto result = add_without_overflow<std::uint32_t, lhs, rhs>();

    // Then the result is as expected
    EXPECT_EQ(result, std::numeric_limits<std::uint32_t>::max());
}

TEST(ArithmeticUtilsAdditionUInt8Test, AddWithoutOverflowReturnsCorrectValueWhenArgumentsAreMaxPossible)
{
    // When calculating the sum of two values that should not overflow
    constexpr std::uint8_t lhs = std::numeric_limits<std::uint8_t>::max() - 1U;
    constexpr std::uint8_t rhs = 1U;
    constexpr auto result = add_without_overflow<std::uint8_t, lhs, rhs>();

    // Then the result is as expected
    EXPECT_EQ(result, std::numeric_limits<std::uint8_t>::max());
}

/// TODO: We currently have no way of testing compile time assertions within our testing framework. This test should be
/// enabled once that's possible (Ticket-178659)
// TEST(ArithmeticUtilsAdditionTest, AddWithoutOverflowDoesNotCompileWhenArgumentsWouldOverflow)
// {
//     // When calculating the sum of two values that would overflow
//     // Then the code should not compile
//     constexpr std::uint32_t lhs = std::numeric_limits<std::uint32_t>::max();
//     constexpr std::uint32_t rhs = 1U;
//     score::cpp::ignore = add_without_overflow<std::uint32_t, lhs, rhs>();
// }

/// TODO: We currently have no way of testing compile time assertions within our testing framework. This test should be
/// enabled once that's possible (Ticket-178659)
// TEST(ArithmeticUtilsAdditionDeathUInt8Test, AddWithoutOverflowDoesNotCompileWhenArgumentsWouldOverflow)
// {
//     // When calculating the sum of two values that would overflow
//     // Then the code should not compile
//     constexpr std::uint8_t lhs = 100U;
//     constexpr std::uint8_t rhs = 200U;
//     score::cpp::ignore = add_without_overflow<std::uint8_t, lhs, rhs>();
// }

TEST(ArithmeticUtilsMultiplicationTest, MultiplyWithoutOverflowReturnsCorrectValueWhenArgumentsDontOverflow)
{
    // When calculating the product of two values that should not overflow
    constexpr std::uint32_t lhs = 100U;
    constexpr std::uint32_t rhs = 200U;
    constexpr auto result = multiply_without_overflow<std::uint32_t, lhs, rhs>();

    // Then the result is as expected
    EXPECT_EQ(result, 20000U);
}

TEST(ArithmeticUtilsMultiplicationUInt8Test, MultiplyWithoutOverflowReturnsCorrectValueWhenArgumentsDontOverflow)
{
    // When calculating the product of two values that should not overflow
    constexpr std::uint8_t lhs = 10U;
    constexpr std::uint8_t rhs = 5U;
    constexpr auto result = multiply_without_overflow<std::uint8_t, lhs, rhs>();

    // Then the result is as expected
    EXPECT_EQ(result, 50U);
}

/// TODO: We currently have no way of testing compile time assertions within our testing framework. This test should be
/// enabled once that's possible (Ticket-178659)
// TEST(ArithmeticUtilsMultiplicationTest, MultiplyWithoutOverflowDoesNotCompileWhenArgumentsWouldOverflow)
// {
//     // When calculating the product of two values that would overflow
//     // Then the code should not compile
//     constexpr std::uint32_t lhs = std::numeric_limits<std::uint32_t>::max();
//     constexpr std::uint32_t rhs = 2U;
//     score::cpp::ignore = multiply_without_overflow<std::uint32_t, lhs, rhs>();
// }

/// TODO: We currently have no way of testing compile time assertions within our testing framework. This test should be
/// enabled once that's possible (Ticket-178659)
// TEST(ArithmeticUtilsMultiplicationUInt8Test, MultiplyWithoutOverflowDoesNotCompileWhenArgumentsWouldOverflow)
// {
//     // When calculating the product of two values that would overflow
//     // Then the code should not compile
//     constexpr std::uint8_t lhs = 100U;
//     constexpr std::uint8_t rhs = 3U;
//     constexpr auto result = multiply_without_overflow<std::uint8_t, lhs, rhs>();
// }

}  // namespace

}  // namespace score::mw::com::impl
