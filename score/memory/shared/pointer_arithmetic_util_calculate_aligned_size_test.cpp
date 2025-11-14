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
#include "score/memory/shared/pointer_arithmetic_util.h"

#include "score/memory/data_type_size_info.h"

#include <score/assert_support.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <tuple>
#include <utility>

namespace score::memory::shared::test
{

namespace
{

class PointerArithmeticCalculateAlignedSizeParamaterizedFixture
    : public ::testing::TestWithParam<std::pair<DataTypeSizeInfo, std::size_t>>
{
};

INSTANTIATE_TEST_SUITE_P(PointerArithmeticCalculateAlignedSizeParamaterizedFixture,
                         PointerArithmeticCalculateAlignedSizeParamaterizedFixture,
                         ::testing::Values(

                             std::make_pair(DataTypeSizeInfo{32, 16}, 32),
                             std::make_pair(DataTypeSizeInfo{32, 32}, 32),
                             std::make_pair(DataTypeSizeInfo{16, 16}, 16),
                             std::make_pair(DataTypeSizeInfo{16, 32}, 32),
                             std::make_pair(DataTypeSizeInfo{8, 32}, 32),

                             std::make_pair(DataTypeSizeInfo{64, 32}, 64),
                             std::make_pair(DataTypeSizeInfo{50, 32}, 64),
                             std::make_pair(DataTypeSizeInfo{70, 32}, 96)));
TEST_P(PointerArithmeticCalculateAlignedSizeParamaterizedFixture, ReturnsCorrectCalculatedSize)
{
    auto [data_type_size_info, expected_size] = GetParam();

    // When calling CalculateAlignedSize
    const auto calculated_size = CalculateAlignedSize(data_type_size_info.Size(), data_type_size_info.Alignment());

    // Then the result should be equal to the expected size
    EXPECT_EQ(calculated_size, expected_size);
}

class PointerArithmeticCalculateAlignedSizeOfSequenceParamaterizedFixture
    : public ::testing::TestWithParam<std::pair<std::vector<DataTypeSizeInfo>, std::size_t>>
{
};

INSTANTIATE_TEST_SUITE_P(
    PointerArithmeticCalculateAlignedSizeOfSequenceParamaterizedFixture,
    PointerArithmeticCalculateAlignedSizeOfSequenceParamaterizedFixture,
    ::testing::Values(std::make_pair(std::vector<DataTypeSizeInfo>{{24, 8}, {32, 16}}, 64),

                      std::make_pair(std::vector<DataTypeSizeInfo>{{32, 16}, {24, 8}}, 56),

                      std::make_pair(std::vector<DataTypeSizeInfo>{{8, 8}, {24, 16}, {64, 32}}, 128),

                      std::make_pair(std::vector<DataTypeSizeInfo>{{24, 8}, {24, 8}, {24, 8}}, 72),

                      std::make_pair(std::vector<DataTypeSizeInfo>{{0, 8}, {24, 8}, {0, 8}}, 24),

                      std::make_pair(std::vector<DataTypeSizeInfo>{{0, 8}, {0, 8}, {0, 8}}, 0),

                      std::make_pair(std::vector<DataTypeSizeInfo>{{24, 8}, {32, 16}, {24, 8}, {32, 16}}, 128)));

TEST_P(PointerArithmeticCalculateAlignedSizeOfSequenceParamaterizedFixture, ReturnsCorrectCalculatedSize)
{
    auto [data_type_size_infos, expected_size] = GetParam();

    // When calling CalculateAlignedSizeOfSequence
    const auto calculated_size = CalculateAlignedSizeOfSequence(data_type_size_infos);

    // Then the result should be equal to the expected size
    EXPECT_EQ(calculated_size, expected_size);
}

TEST(PointerArithmeticCalculateAlignedSizeOfSequenceTest,
     CallingWithTypeErasedDataInfosContainingZeroAlignmentTerminates)
{
    // When calling CalculateAlignedSizeOfSequence with only a DataTypeSizeInfo containing alignment == 0
    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = CalculateAlignedSizeOfSequence({{24, 8}, {8, 0}, {16, 16}}));
}

}  // namespace
}  // namespace score::memory::shared::test
