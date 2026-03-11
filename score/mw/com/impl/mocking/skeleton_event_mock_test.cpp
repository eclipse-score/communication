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
#include "score/mw/com/impl/mocking/skeleton_event_mock.h"
#include "score/result/result.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/mocking/test_type_utilities.h"
#include "score/mw/com/impl/skeleton_event.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

namespace score::mw::com::impl
{
namespace
{

using TestSampleType = std::uint32_t;

auto kDummyEventName = "MyDummyEvent";
const TestSampleType kDummyValueToSend{10U};

class SkeletonEventMockFixture : public ::testing::Test
{
  public:
    SkeletonEventMockFixture()
    {
        unit_.InjectMock(skeleton_event_mock_);
    }

    SkeletonEventMock<TestSampleType> skeleton_event_mock_{};
    SkeletonBase skeleton_base_{nullptr, MakeFakeInstanceIdentifier(1U)};
    SkeletonEvent<TestSampleType> unit_{skeleton_base_, kDummyEventName, nullptr};
};

TEST_F(SkeletonEventMockFixture, AllocateDispatchesToMockAfterInjectingMock)
{
    // Given a SkeletonEvent constructed with an empty binding and an injected mock

    // Expecting that Allocate will be called on the mock which returns a valid SampleAllocateePtr
    auto fake_sample_allocatee_ptr = MakeFakeSampleAllocateePtr(std::make_unique<TestSampleType>());
    EXPECT_CALL(skeleton_event_mock_, Allocate()).WillOnce(Return(ByMove(std::move(fake_sample_allocatee_ptr))));

    // When Allocate is called on the SkeletonEvent
    auto result = unit_.Allocate();

    // Then the result is valid
    ASSERT_TRUE(result.has_value());
}

TEST_F(SkeletonEventMockFixture, AllocateReturnsErrorWhenMockReturnsError)
{
    // Given a SkeletonEvent constructed with an empty binding and an injected mock

    // Expecting that Allocate will be called on the mock which returns an error
    const auto error_code = ComErrc::kNotOffered;
    EXPECT_CALL(skeleton_event_mock_, Allocate()).WillOnce(Return(ByMove(MakeUnexpected(error_code))));

    // When Allocate is called on the SkeletonEvent
    auto result = unit_.Allocate();

    // Then the result contains the same error code that was returned by the mock
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), error_code);
}

TEST_F(SkeletonEventMockFixture, CopySendDispatchesToMockAfterInjectingMock)
{
    // Given a SkeletonEvent constructed with an empty binding and an injected mock

    // Expecting that Send with copy will be called on the mock which returns a valid result
    EXPECT_CALL(skeleton_event_mock_, Send(kDummyValueToSend)).WillOnce(Return(ResultBlank{}));

    // When Send with copy is called on the SkeletonEvent
    auto result = unit_.Send(kDummyValueToSend);

    // Then the result is valid
    ASSERT_TRUE(result.has_value());
}

TEST_F(SkeletonEventMockFixture, CopySendReturnsErrorWhenMockReturnsError)
{
    // Given a SkeletonEvent constructed with an empty binding and an injected mock

    // Expecting that Send with copy will be called on the mock which returns an error
    const auto error_code = ComErrc::kNotOffered;
    EXPECT_CALL(skeleton_event_mock_, Send(kDummyValueToSend)).WillOnce(Return(MakeUnexpected(error_code)));

    // When Send with copy is called on the SkeletonEvent
    auto result = unit_.Send(kDummyValueToSend);

    // Then the result contains the same error code that was returned by the mock
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), error_code);
}

TEST_F(SkeletonEventMockFixture, ZeroCopySendDispatchesToMockAfterInjectingMock)
{
    // Given a SkeletonEvent constructed with an empty binding and an injected mock

    // Expecting that zero-copy Send will be called on the mock with a SampleAllocateePtr containing the sent value
    // which returns a valid result
    EXPECT_CALL(skeleton_event_mock_, Send(testing::Matcher<SampleAllocateePtr<TestSampleType>>(_)))
        .WillOnce(Invoke([](auto sample_allocatee_ptr) noexcept -> ResultBlank {
            EXPECT_EQ(*sample_allocatee_ptr, kDummyValueToSend);
            return ResultBlank{};
        }));

    // When zero-copy Send is called on the SkeletonEvent with a SampleAllocateePtr containing a value
    auto fake_sample_allocatee_ptr = MakeFakeSampleAllocateePtr(std::make_unique<TestSampleType>());
    *fake_sample_allocatee_ptr = kDummyValueToSend;
    auto result = unit_.Send(std::move(fake_sample_allocatee_ptr));

    // Then the result is valid
    ASSERT_TRUE(result.has_value());
}

TEST_F(SkeletonEventMockFixture, ZeroCopySendReturnsErrorWhenMockReturnsError)
{
    // Given a SkeletonEvent constructed with an empty binding and an injected mock

    // Expecting that zero-copy Send will be called on the mock which returns an error
    const auto error_code = ComErrc::kNotOffered;
    EXPECT_CALL(skeleton_event_mock_, Send(testing::Matcher<SampleAllocateePtr<TestSampleType>>(_)))
        .WillOnce(Return(MakeUnexpected(error_code)));

    // When zero-copy Send is called on the SkeletonEvent
    auto fake_sample_allocatee_ptr = MakeFakeSampleAllocateePtr(std::make_unique<TestSampleType>());
    auto result = unit_.Send(std::move(fake_sample_allocatee_ptr));

    // Then the result contains the same error code that was returned by the mock
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), error_code);
}

}  // namespace
}  // namespace score::mw::com::impl
