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
#include "score/mw/com/impl/tracing/configuration/hash_helper_utility.h"

#include <gtest/gtest.h>
#include <cstdint>
#include <vector>

namespace score::mw::com::impl::tracing::configuration
{
namespace
{

const std::vector<std::uint32_t> kDummyVectorOfIntsNoOverflow{1U, 2U, 3U, 4U};
const std::vector<std::uint32_t> kDummyVectorOfIntsWithOverflow{4294967295U, 1U, 2U};

TEST(HashHelperUtilityTest, AccumulatingVectorOfIntsWhoseSumDoesNotOverflowWillReturnSumOfAllVectorElements)
{
    // When accumulating a vector of ints
    std::uint32_t sum{0};
    const auto [result, overflow_error] =
        Accumulate(kDummyVectorOfIntsNoOverflow.cbegin(), kDummyVectorOfIntsNoOverflow.cend(), sum);

    // Then the result should be equal to the sum of all elements in the vector
    EXPECT_EQ(result, 10);
}

TEST(HashHelperUtilityTest, AccumulatingVectorOfIntsWhoseSumDoesNotOverflowWillReturnOverflowDidNotOccur)
{
    // When accumulating a vector of ints
    std::uint32_t sum{0};
    const auto [result, overflow_error] =
        Accumulate(kDummyVectorOfIntsNoOverflow.cbegin(), kDummyVectorOfIntsNoOverflow.cend(), sum);

    // Then the overflow error flag should be false
    EXPECT_FALSE(overflow_error);
}

TEST(HashHelperUtilityTest, AccumulatingVectorOfIntsWhoseSumOverflowsWillReturnOverflowDidOccur)
{
    // When accumulating a vector of ints whose sum would overflow
    std::uint32_t sum{0};
    const auto [result, overflow_error] =
        Accumulate(kDummyVectorOfIntsWithOverflow.cbegin(), kDummyVectorOfIntsWithOverflow.cend(), sum);

    // Then the overflow error flag should be true
    EXPECT_TRUE(overflow_error);
}

}  // namespace
}  // namespace score::mw::com::impl::tracing::configuration
