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
#include "score/mw/com/impl/tracing/skeleton_event_tracing_data.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl::tracing
{
namespace
{

const ServiceElementInstanceIdentifierView kDummyServiceElementInstanceIdentifierView1{
    {"same_type", "same_element", ServiceElementType::EVENT},
    "same_specifier"};
const ServiceElementInstanceIdentifierView kDummyServiceElementInstanceIdentifierView2{
    {"different_type", "different_element", ServiceElementType::FIELD},
    "different_specifier"};

const ServiceElementTracingData kDummyServiceElementTracingData1{2U, 5U};
const ServiceElementTracingData kDummyServiceElementTracingData2{23U, 6U};

TEST(SkeletonEventTracingDataTest, CallingDisableAllTracePointsWillSetAllTracePointsToFalse)
{
    // Given a SkeletonEventTracingData object with all trace points set to true
    SkeletonEventTracingData skeleton_event_tracing_data{};
    skeleton_event_tracing_data.enable_send = true;
    skeleton_event_tracing_data.enable_send_with_allocate = true;

    // When calling DisableAllTracePoints
    DisableAllTracePoints(skeleton_event_tracing_data);

    // Then all trace points will be set to false
    EXPECT_FALSE(skeleton_event_tracing_data.enable_send);
    EXPECT_FALSE(skeleton_event_tracing_data.enable_send_with_allocate);
}

TEST(SkeletonEventTracingDataEqualityOperatorTest,
     ComparingTwoSkeletonEventTracingDatasContainingTheSameValuesReturnsTrue)
{
    const bool enable_send{true};
    const bool enable_send_with_allocate{true};

    // Given 2 SkeletonEventTracingDatas with containing the same values
    const SkeletonEventTracingData skeleton_event_tracing_data_1{kDummyServiceElementInstanceIdentifierView1,
                                                                 kDummyServiceElementTracingData1,
                                                                 enable_send,
                                                                 enable_send_with_allocate};
    const SkeletonEventTracingData skeleton_event_tracing_data_2{kDummyServiceElementInstanceIdentifierView1,
                                                                 kDummyServiceElementTracingData1,
                                                                 enable_send,
                                                                 enable_send_with_allocate};

    // When comparing the two SkeletonEventTracingDatas
    const auto result = skeleton_event_tracing_data_1 == skeleton_event_tracing_data_2;

    // Then the result should be true
    EXPECT_TRUE(result);
}

class SkeletonEventTracingDataEqualityOperationParamaterisedFixture
    : public ::testing::TestWithParam<std::pair<SkeletonEventTracingData, SkeletonEventTracingData>>
{
};

TEST_P(SkeletonEventTracingDataEqualityOperationParamaterisedFixture, DifferentSkeletonEventTracingDatasAreNotEqual)
{
    // Given 2 SkeletonEventTracingDatas containing different values
    const auto [skeleton_event_tracing_data_1, skeleton_event_tracing_data_2] = GetParam();

    // When comparing the two SkeletonEventTracingDatas
    const auto result = skeleton_event_tracing_data_1 == skeleton_event_tracing_data_2;

    // Then the result should be false
    EXPECT_FALSE(result);
}

// Test that each element that should be used in the equality operator is used by changing them one at a time.
INSTANTIATE_TEST_CASE_P(
    SkeletonEventTracingDataEqualityOperationParamaterisedFixture,
    SkeletonEventTracingDataEqualityOperationParamaterisedFixture,
    ::testing::Values(std::make_pair(SkeletonEventTracingData{kDummyServiceElementInstanceIdentifierView1,
                                                              kDummyServiceElementTracingData1,
                                                              true,
                                                              true},
                                     SkeletonEventTracingData{kDummyServiceElementInstanceIdentifierView2,
                                                              kDummyServiceElementTracingData1,
                                                              true,
                                                              true}),

                      std::make_pair(SkeletonEventTracingData{kDummyServiceElementInstanceIdentifierView1,
                                                              kDummyServiceElementTracingData1,
                                                              true,
                                                              true},
                                     SkeletonEventTracingData{kDummyServiceElementInstanceIdentifierView1,
                                                              kDummyServiceElementTracingData2,
                                                              true,
                                                              true}),

                      std::make_pair(SkeletonEventTracingData{kDummyServiceElementInstanceIdentifierView1,
                                                              kDummyServiceElementTracingData1,
                                                              true,
                                                              true},
                                     SkeletonEventTracingData{kDummyServiceElementInstanceIdentifierView1,
                                                              kDummyServiceElementTracingData1,
                                                              false,
                                                              true}),

                      std::make_pair(SkeletonEventTracingData{kDummyServiceElementInstanceIdentifierView1,
                                                              kDummyServiceElementTracingData1,
                                                              true,
                                                              true},
                                     SkeletonEventTracingData{kDummyServiceElementInstanceIdentifierView1,
                                                              kDummyServiceElementTracingData1,
                                                              true,
                                                              false})));

}  // namespace
}  // namespace score::mw::com::impl::tracing
