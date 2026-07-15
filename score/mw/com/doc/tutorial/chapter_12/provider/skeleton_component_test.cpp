/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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
#include "score/mw/com/doc/tutorial/chapter_12/provider/skeleton_component.h"

#include "score/mw/com/com_error_domain.h"
#include "score/mw/com/test_types.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <tuple>

using testing::_;
using testing::Invoke;
using testing::Return;

class SkeletonComponentTestFixture : public ::testing::Test
{
  protected:
    using SkeletonComponent = score::mw::com::tutorial::SkeletonComponent;
    using FixedSizeString = score::mw::com::tutorial::HelloWorldProxy::FixedSizeString;
    using MessageEventMock = score::mw::com::SkeletonEventMock<FixedSizeString>;

    void SetUp() override
    {
        auto skeleton =
            score::mw::com::SkeletonWrapperClassTestView<score::mw::com::tutorial::HelloWorldSkeleton>::Create(
                skeleton_mock_, events_tuple_);
        skeleton_component_.emplace(std::move(skeleton));
    }

    MessageEventMock& message_event_mock()
    {
        return std::get<0>(events_tuple_).mock;
    }

    score::mw::com::SkeletonBaseMock skeleton_mock_{};
    std::tuple<score::mw::com::NamedSkeletonEventMock<FixedSizeString>> events_tuple_{"message"};
    std::optional<SkeletonComponent> skeleton_component_{};
};

TEST_F(SkeletonComponentTestFixture, OfferServiceReturnsTrueWhenOfferServiceSucceeds)
{
    // Expect, that OfferService is called on the mocked SkeletonBase and returns success
    EXPECT_CALL(skeleton_mock_, OfferService()).WillOnce(Return(score::Result<void>{}));

    // when we call OfferService on the unit under test
    EXPECT_TRUE(skeleton_component_->OfferService());
    // and that the result is true.
}

TEST_F(SkeletonComponentTestFixture, OfferServiceReturnsFalseWhenOfferServiceFails)
{
    // Expect, that OfferService is called on the mocked SkeletonBase and returns a binding failure
    EXPECT_CALL(skeleton_mock_, OfferService())
        .WillOnce(Return(score::MakeUnexpected(score::mw::com::ComErrc::kBindingFailure)));

    // when we call OfferService on the unit under test
    EXPECT_FALSE(skeleton_component_->OfferService());
    // and that the result is false.
}

TEST_F(SkeletonComponentTestFixture, SendSampleReturnsNoSampleAllocatedWhenAllocateFails)
{
    // Expect, that Allocate is called on the mocked SkeletonEvent and returns a binding failure
    EXPECT_CALL(message_event_mock(), Allocate())
        .WillOnce(Return(score::MakeUnexpected(score::mw::com::ComErrc::kBindingFailure)));

    // when we call SendSample on the unit under test
    EXPECT_EQ(skeleton_component_->SendSample(5U), SkeletonComponent::SendSampleResult::kNoSampleAllocated);
    // and that the result indicates that no sample slot was allocated.
}

TEST_F(SkeletonComponentTestFixture, SendSampleReturnsSendFailedWhenSendFails)
{
    // Expect, that Allocate returns a sample slot
    EXPECT_CALL(message_event_mock(), Allocate())
        .WillOnce(Invoke([]() -> score::Result<score::mw::com::SampleAllocateePtr<FixedSizeString>> {
            auto sample_unique_ptr = std::make_unique<FixedSizeString>();
            return score::mw::com::MakeFakeSampleAllocateePtr(std::move(sample_unique_ptr));
        }));
    // and expect, that Send is called and returns a binding failure
    EXPECT_CALL(message_event_mock(), Send(testing::A<score::mw::com::SampleAllocateePtr<FixedSizeString>>()))
        .WillOnce(Return(score::MakeUnexpected(score::mw::com::ComErrc::kBindingFailure)));

    // when we call SendSample on the unit under test
    EXPECT_EQ(skeleton_component_->SendSample(42U), SkeletonComponent::SendSampleResult::kSendFailed);
    // and that the result indicates a send failure.
}

TEST_F(SkeletonComponentTestFixture, SendSampleWritesPayloadAndReturnsHandledWhenSendSucceeds)
{
    // Expect, that Allocate returns a sample slot
    EXPECT_CALL(message_event_mock(), Allocate())
        .WillOnce(Invoke([]() -> score::Result<score::mw::com::SampleAllocateePtr<FixedSizeString>> {
            auto sample_unique_ptr = std::make_unique<FixedSizeString>();
            return score::mw::com::MakeFakeSampleAllocateePtr(std::move(sample_unique_ptr));
        }));
    // and expect, that Send is called with a payload containing "Hello World42"
    EXPECT_CALL(message_event_mock(), Send(testing::A<score::mw::com::SampleAllocateePtr<FixedSizeString>>()))
        .WillOnce(
            Invoke([](score::mw::com::SampleAllocateePtr<FixedSizeString> sample_allocatee_ptr) -> score::Result<void> {
                EXPECT_STREQ(sample_allocatee_ptr.Get()->data(), "Hello World42");
                return {};
            }));

    // when we call SendSample on the unit under test
    EXPECT_EQ(skeleton_component_->SendSample(42U), SkeletonComponent::SendSampleResult::kHandled);
    // and that the result indicates successful handling.
}
