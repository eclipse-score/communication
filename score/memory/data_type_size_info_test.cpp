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
#include "score/memory/data_type_size_info.h"

#include <gtest/gtest.h>

#include <score/assert_support.hpp>

#include <cstddef>

namespace score::memory
{
namespace
{

constexpr std::size_t kValidSize{32U};
constexpr std::size_t kValidAlignment{16U};

TEST(DataTypeSizeInfoTest, ConstructingWithAlignmentOfZeroTerminates)
{
    // When constructing a DataTypeSizeInfo with invalid alignment
    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(DataTypeSizeInfo(kValidSize, 0U));
}

TEST(DataTypeSizeInfoTest, ConstructingWithAlignmentNotPowerOfTwoTerminates)
{
    // When constructing a DataTypeSizeInfo with invalid alignment
    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(DataTypeSizeInfo(kValidSize, kValidAlignment + 1U));
}

TEST(DataTypeSizeInfoTest, ConstructingWithSizeNotMultipleOfAlignmentTerminates)
{
    // When constructing a DataTypeSizeInfo with invalid alignment
    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(DataTypeSizeInfo(kValidAlignment + 1U, kValidAlignment));
}

TEST(DataTypeSizeInfoEqualityTest, ObjectsWithSameSizeAndAlignmentCompareTrue)
{
    // Given two DataTypeSizeInfo objects with the same size and alignment
    DataTypeSizeInfo unit{kValidSize, kValidAlignment};
    DataTypeSizeInfo unit2{kValidSize, kValidAlignment};

    // When comparing the two objects
    const auto compare_result = unit == unit2;

    // Then the result should be true
    EXPECT_TRUE(compare_result);
}

TEST(DataTypeSizeInfoEqualityTest, ObjectsWithDifferentSizeCompareFalse)
{
    // Given two DataTypeSizeInfo objects with the different sizes
    DataTypeSizeInfo unit{kValidSize, kValidAlignment};
    DataTypeSizeInfo unit2{kValidSize * 2U, kValidAlignment};

    // When comparing the two objects
    const auto compare_result = unit == unit2;

    // Then the result should be false
    EXPECT_FALSE(compare_result);
}

TEST(DataTypeSizeInfoEqualityTest, ObjectsWithDifferentAlignemtCompareFalse)
{
    // Given two DataTypeSizeInfo objects with the different alignments
    DataTypeSizeInfo unit{kValidSize, kValidAlignment};
    DataTypeSizeInfo unit2{kValidSize, kValidAlignment * 2U};

    // When comparing the two objects
    const auto compare_result = unit == unit2;

    // Then the result should be false
    EXPECT_FALSE(compare_result);
}

}  // namespace
}  // namespace score::memory
