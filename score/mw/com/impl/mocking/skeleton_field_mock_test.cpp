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
#include "score/mw/com/impl/mocking/skeleton_field_mock.h"
#include "score/result/result.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/mocking/test_type_utilities.h"
#include "score/mw/com/impl/skeleton_field.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

namespace score::mw::com::impl
{
namespace
{

using TestSampleType = std::uint32_t;

auto kDummyFieldName = "MyDummyField";
const TestSampleType kDummyValueToUpdate{10U};

class SkeletonFieldMockFixture : public ::testing::Test
{
  public:
    SkeletonFieldMockFixture()
    {
        unit_.InjectMock(skeleton_field_mock_);
    }

    SkeletonFieldMock<TestSampleType> skeleton_field_mock_{};
    SkeletonBase skeleton_base_{nullptr, MakeFakeInstanceIdentifier(1U)};
    SkeletonField<TestSampleType> unit_{skeleton_base_, kDummyFieldName, nullptr};
};

TEST_F(SkeletonFieldMockFixture, AllocateDispatchesToMockAfterInjectingMock)
{
    // Given a SkeletonField constructed with an empty binding and an injected mock

    // Expecting that Allocate will be called on the mock which returns a valid SampleAllocateePtr
    auto fake_sample_allocatee_ptr = MakeFakeSampleAllocateePtr(std::make_unique<TestSampleType>());
    EXPECT_CALL(skeleton_field_mock_, Allocate()).WillOnce(Return(ByMove(std::move(fake_sample_allocatee_ptr))));

    // When Allocate is called on the SkeletonField
    auto result = unit_.Allocate();

    // Then the result is valid
    ASSERT_TRUE(result.has_value());
}

TEST_F(SkeletonFieldMockFixture, AllocateReturnsErrorWhenMockReturnsError)
{
    // Given a SkeletonField constructed with an empty binding and an injected mock

    // Expecting that Allocate will be called on the mock which returns an error
    const auto error_code = ComErrc::kNotOffered;
    EXPECT_CALL(skeleton_field_mock_, Allocate()).WillOnce(Return(ByMove(MakeUnexpected(error_code))));

    // When Allocate is called on the SkeletonField
    auto result = unit_.Allocate();

    // Then the result contains the same error code that was returned by the mock
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), error_code);
}

TEST_F(SkeletonFieldMockFixture, CopyUpdateDispatchesToMockAfterInjectingMock)
{
    // Given a SkeletonField constructed with an empty binding and an injected mock

    // Expecting that Update with copy will be called on the mock which returns a valid result
    EXPECT_CALL(skeleton_field_mock_, Update(kDummyValueToUpdate)).WillOnce(Return(ResultBlank{}));

    // When Update with copy is called on the SkeletonField
    auto result = unit_.Update(kDummyValueToUpdate);

    // Then the result is valid
    ASSERT_TRUE(result.has_value());
}

TEST_F(SkeletonFieldMockFixture, CopyUpdateReturnsErrorWhenMockReturnsError)
{
    // Given a SkeletonField constructed with an empty binding and an injected mock

    // Expecting that Update with copy will be called on the mock which returns an error
    const auto error_code = ComErrc::kNotOffered;
    EXPECT_CALL(skeleton_field_mock_, Update(kDummyValueToUpdate)).WillOnce(Return(MakeUnexpected(error_code)));

    // When Update with copy is called on the SkeletonField
    auto result = unit_.Update(kDummyValueToUpdate);

    // Then the result contains the same error code that was returned by the mock
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), error_code);
}

TEST_F(SkeletonFieldMockFixture, ZeroCopyUpdateDispatchesToMockAfterInjectingMock)
{
    // Given a SkeletonField constructed with an empty binding and an injected mock

    // Expecting that zero-copy Update will be called on the mock which returns a valid result
    EXPECT_CALL(skeleton_field_mock_, Update(testing::Matcher<SampleAllocateePtr<TestSampleType>>(_)))
        .WillOnce(Return(ResultBlank{}));

    // When zero-copy Update is called on the SkeletonField
    auto fake_sample_allocatee_ptr = MakeFakeSampleAllocateePtr(std::make_unique<TestSampleType>());
    auto result = unit_.Update(std::move(fake_sample_allocatee_ptr));

    // Then the result is valid
    ASSERT_TRUE(result.has_value());
}

TEST_F(SkeletonFieldMockFixture, ZeroCopyUpdateReturnsErrorWhenMockReturnsError)
{
    // Given a SkeletonField constructed with an empty binding and an injected mock

    // Expecting that zero-copy Update will be called on the mock which returns an error
    const auto error_code = ComErrc::kNotOffered;
    EXPECT_CALL(skeleton_field_mock_, Update(testing::Matcher<SampleAllocateePtr<TestSampleType>>(_)))
        .WillOnce(Return(MakeUnexpected(error_code)));

    // When zero-copy Update is called on the SkeletonField
    auto fake_sample_allocatee_ptr = MakeFakeSampleAllocateePtr(std::make_unique<TestSampleType>());
    auto result = unit_.Update(std::move(fake_sample_allocatee_ptr));

    // Then the result contains the same error code that was returned by the mock
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), error_code);
}

}  // namespace
}  // namespace score::mw::com::impl
