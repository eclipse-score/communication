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

constexpr std::size_t kDummySize{8U};
constexpr std::size_t kDummyAlignment{16U};

constexpr std::size_t kInvalidAlignment{17U};

TEST(DataTypeSizeInfoTest, SettingSizeUpdatesInternalSize)
{
    // Given a DataTypeSizeInfo
    DataTypeSizeInfo unit{kDummySize, kDummyAlignment};

    // When updating the size with the setter
    const auto new_size = kDummySize + 1U;
    unit.SetSize(new_size);

    // Then the stored size is updated
    EXPECT_EQ(unit.Size(), new_size);
}

TEST(DataTypeSizeInfoTest, SettingAlignmentUpdatesInternalAlignment)
{
    // Given a DataTypeSizeInfo
    DataTypeSizeInfo unit{kDummySize, kDummyAlignment};

    // When updating the alignment with the setter
    const auto new_alignment = kDummyAlignment * 2U;
    unit.SetAlignment(new_alignment);

    // Then the stored alignment is updated
    EXPECT_EQ(unit.Alignment(), new_alignment);
}

TEST(DataTypeSizeInfoTest, ConstructingWithAlignmentNotPowerOfTwoTerminates)
{
    // When constructing a DataTypeSizeInfo with invalid alignment
    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(DataTypeSizeInfo(kDummySize, kInvalidAlignment));
}

TEST(DataTypeSizeInfoTest, SettingAlignmentWithAlignmentNotPowerOfTwoTerminates)
{
    // Given a DataTypeSizeInfo
    DataTypeSizeInfo unit{kDummySize, kDummyAlignment};

    // When updating the alignment with the setter with an invalid alignment
    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(unit.SetAlignment(kInvalidAlignment));
}

TEST(DataTypeSizeInfoEqualityTest, ObjectsWithSameSizeAndAlignmentCompareTrue)
{
    // Given two DataTypeSizeInfo objects with the same size and alignment
    DataTypeSizeInfo unit{kDummySize, kDummyAlignment};
    DataTypeSizeInfo unit2{kDummySize, kDummyAlignment};

    // When comparing the two objects
    const auto compare_result = unit == unit2;

    // Then the result should be true
    EXPECT_TRUE(compare_result);
}

TEST(DataTypeSizeInfoEqualityTest, ObjectsWithDifferentSizeCompareFalse)
{
    // Given two DataTypeSizeInfo objects with the different sizes
    DataTypeSizeInfo unit{kDummySize, kDummyAlignment};
    DataTypeSizeInfo unit2{kDummySize + 1U, kDummyAlignment};

    // When comparing the two objects
    const auto compare_result = unit == unit2;

    // Then the result should be false
    EXPECT_FALSE(compare_result);
}

TEST(DataTypeSizeInfoEqualityTest, ObjectsWithDifferentAlignemtCompareFalse)
{
    // Given two DataTypeSizeInfo objects with the different alignments
    DataTypeSizeInfo unit{kDummySize, kDummyAlignment};
    DataTypeSizeInfo unit2{kDummySize, kDummyAlignment * 2U};

    // When comparing the two objects
    const auto compare_result = unit == unit2;

    // Then the result should be false
    EXPECT_FALSE(compare_result);
}

}  // namespace
}  // namespace score::memory
