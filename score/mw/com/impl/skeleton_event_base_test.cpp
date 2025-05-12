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
#include "score/mw/com/impl/skeleton_event_base.h"
#include "score/mw/com/impl/bindings/mock_binding/skeleton.h"
#include "score/mw/com/impl/bindings/mock_binding/skeleton_event.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/plumbing/skeleton_binding_factory.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/impl/runtime_mock.h"
#include "score/mw/com/impl/skeleton_base.h"
#include "score/mw/com/impl/skeleton_event.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <type_traits>
#include <utility>
#include <vector>

namespace score::mw::com::impl
{
namespace
{

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrictMock;

const ServiceTypeDeployment kEmptyTypeDeployment{score::cpp::blank{}};
const ServiceIdentifierType kFooservice{make_ServiceIdentifierType("foo")};
const auto kInstanceSpecifier = InstanceSpecifier::Create("/dummy_instance_specifier").value();
const ServiceInstanceDeployment kEmptyInstanceDeployment{kFooservice,
                                                         score::cpp::blank{},
                                                         QualityType::kASIL_QM,
                                                         kInstanceSpecifier};

SkeletonBase kEmptySkeleton(std::make_unique<mock_binding::Skeleton>(),
                            make_InstanceIdentifier(kEmptyInstanceDeployment, kEmptyTypeDeployment));

const auto kEventName{"DummyEvent1"};

class MyDummyEvent final : public SkeletonEventBase
{
  public:
    MyDummyEvent()
        : SkeletonEventBase{kEmptySkeleton, kEventName, std::make_unique<StrictMock<mock_binding::SkeletonEventBase>>()}
    {
    }

    StrictMock<mock_binding::SkeletonEventBase>* GetMockBinding() noexcept
    {
        auto* const mock_event_binding = dynamic_cast<StrictMock<mock_binding::SkeletonEventBase>*>(binding_.get());
        return mock_event_binding;
    }
};

class SkeletonEventBaseFixture : public ::testing::Test
{
  public:
    void CreateSkeletonEvent()
    {
        skeleton_event_ = std::make_unique<MyDummyEvent>();

        mock_event_binding_ = skeleton_event_->GetMockBinding();
        ASSERT_NE(mock_event_binding_, nullptr);
    }

    std::unique_ptr<MyDummyEvent> skeleton_event_{nullptr};
    StrictMock<mock_binding::SkeletonEventBase>* mock_event_binding_{nullptr};
};

TEST(SkeletonEventBaseTests, NotCopyable)
{
    static_assert(!std::is_copy_constructible<MyDummyEvent>::value, "Is wrongly copyable");
    static_assert(!std::is_copy_assignable<MyDummyEvent>::value, "Is wrongly copyable");
}

TEST(SkeletonEventBaseTests, IsMoveable)
{
    static_assert(std::is_move_constructible<MyDummyEvent>::value, "Is not move constructible");
    static_assert(std::is_move_assignable<MyDummyEvent>::value, "Is not move assignable");
}

TEST_F(SkeletonEventBaseFixture, PrepareOfferDispatchesToBinding)
{
    // Given a constructed SkeletonEvent with a valid mock binding
    CreateSkeletonEvent();

    // Expecting that PrepareOffer() is called on the binding
    EXPECT_CALL(*mock_event_binding_, PrepareOffer()).WillOnce(Return(ResultBlank{}));

    // When offering the event
    const auto result = skeleton_event_->PrepareOffer();

    // and that PrepareStopOffer() is called on destruction
    EXPECT_CALL(*mock_event_binding_, PrepareStopOffer());

    // and that the result is blank
    EXPECT_TRUE(result.has_value());
}

TEST_F(SkeletonEventBaseFixture, PrepareStopOfferDispatchesToBinding)
{
    // Given a constructed SkeletonEvent with a valid mock binding
    CreateSkeletonEvent();

    // Expecting that PrepareOffer() is called on the binding
    EXPECT_CALL(*mock_event_binding_, PrepareOffer()).WillOnce(Return(ResultBlank{}));

    // When offering the event
    const auto result = skeleton_event_->PrepareOffer();
    EXPECT_TRUE(result.has_value());

    // and expecting that PrepareStopOffer() is called on the binding
    EXPECT_CALL(*mock_event_binding_, PrepareStopOffer());

    // When stopping the event offering
    skeleton_event_->PrepareStopOffer();
}

TEST_F(SkeletonEventBaseFixture, PrepareOfferPropagatesErrorFromBinding)
{
    // Given a constructed SkeletonEvent with a valid mock binding
    CreateSkeletonEvent();

    // When PrepareOffer() is called on the binding and returns an error
    EXPECT_CALL(*mock_event_binding_, PrepareOffer())
        .WillOnce(Return(score::MakeUnexpected(score::mw::com::impl::ComErrc::kInvalidBindingInformation)));

    // Expecting that when offering the event
    const auto result = skeleton_event_->PrepareOffer();

    // That the error is propagated
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), score::mw::com::impl::ComErrc::kInvalidBindingInformation);
}

TEST_F(SkeletonEventBaseFixture, PrepareStopOfferIsNotCalledIfEventWasNotOffered)
{
    // Given a constructed SkeletonEvent with a valid mock binding
    CreateSkeletonEvent();

    // Expecting that PrepareStopOffer() is not called on the binding
    EXPECT_CALL(*mock_event_binding_, PrepareStopOffer()).Times(0);

    // When calling PrepareStopOffer
    skeleton_event_->PrepareStopOffer();

    // Or when destroying the event
}

TEST_F(SkeletonEventBaseFixture, MovingConstructingSkeletonEventBaseDoesNotCallPrepareStopOffer)
{
    bool is_prepare_stop_offer_called{false};
    {
        // Given a constructed SkeletonEvent with a valid mock binding
        MyDummyEvent skeleton_event{};
        mock_event_binding_ = skeleton_event.GetMockBinding();

        // Expecting that PrepareOffer() is called on the binding
        EXPECT_CALL(*mock_event_binding_, PrepareOffer());

        // When offering the event
        skeleton_event.PrepareOffer();

        // Expecting that PrepareStopOffer() is called on the binding
        EXPECT_CALL(*mock_event_binding_, PrepareStopOffer())
            .WillOnce(Invoke([&is_prepare_stop_offer_called]() -> ResultBlank {
                is_prepare_stop_offer_called = true;
                return {};
            }));

        // When move constructing the second event from the first event
        MyDummyEvent skeleton_event_2{std::move(skeleton_event)};

        // not when the move constructor is called
        EXPECT_FALSE(is_prepare_stop_offer_called);
    }
    // but only when the skeleton is destroyed
    EXPECT_TRUE(is_prepare_stop_offer_called);
}

TEST(SkeletonEventBaseTests, MovingAssigningOfferedSkeletonEventBaseCallsPrepareStopOffer)
{
    StrictMock<mock_binding::SkeletonEventBase>* mock_event_binding{nullptr};
    StrictMock<mock_binding::SkeletonEventBase>* mock_event_binding2{nullptr};

    bool is_prepare_stop_offer_called{false};
    bool is_prepare_stop_offer_called2{false};

    {
        // Given a constructed SkeletonEvent with a valid mock binding
        MyDummyEvent skeleton_event{};
        mock_event_binding = skeleton_event.GetMockBinding();

        // Expecting that PrepareOffer() is called on the binding
        EXPECT_CALL(*mock_event_binding, PrepareOffer());

        // When offering the event
        skeleton_event.PrepareOffer();

        // and given a second SkeletonEvent with a valid mock binding
        MyDummyEvent skeleton_event2{};
        mock_event_binding2 = skeleton_event2.GetMockBinding();

        // Expecting that PrepareOffer() is called on the second binding
        EXPECT_CALL(*mock_event_binding2, PrepareOffer());

        // When offering the second event
        skeleton_event2.PrepareOffer();

        // and expecting that PrepareStopOffer() is called on both bindings
        EXPECT_CALL(*mock_event_binding, PrepareStopOffer())
            .WillOnce(Invoke([&is_prepare_stop_offer_called]() -> ResultBlank {
                is_prepare_stop_offer_called = true;
                return {};
            }));
        EXPECT_CALL(*mock_event_binding2, PrepareStopOffer())
            .WillOnce(Invoke([&is_prepare_stop_offer_called2]() -> ResultBlank {
                is_prepare_stop_offer_called2 = true;
                return {};
            }));

        // When move assigning the second event to the first event
        skeleton_event = std::move(skeleton_event2);

        // Then PrepareStopOffer() is called on the first event's binding
        EXPECT_TRUE(is_prepare_stop_offer_called);
        EXPECT_FALSE(is_prepare_stop_offer_called2);
    }
    // and PrepareStopOffer() is called on the second event's binding on destruction
    EXPECT_TRUE(is_prepare_stop_offer_called);
    EXPECT_TRUE(is_prepare_stop_offer_called2);
}

TEST(SkeletonEventBaseTests, MovingAssigningUnOfferedSkeletonEventBaseDoesNotCallPrepareStopOffer)
{
    StrictMock<mock_binding::SkeletonEventBase>* mock_event_binding{nullptr};
    StrictMock<mock_binding::SkeletonEventBase>* mock_event_binding2{nullptr};

    bool is_prepare_stop_offer_called2{false};

    {
        // Given a constructed SkeletonEvent with a valid mock binding
        MyDummyEvent skeleton_event{};
        mock_event_binding = skeleton_event.GetMockBinding();

        // and given a second SkeletonEvent with a valid mock binding
        MyDummyEvent skeleton_event2{};
        mock_event_binding2 = skeleton_event2.GetMockBinding();

        // Expecting that PrepareOffer() is called on the second binding
        EXPECT_CALL(*mock_event_binding2, PrepareOffer());

        // When offering the second event
        skeleton_event2.PrepareOffer();

        // and expecting that PrepareStopOffer() is only called on the offered event's binding
        EXPECT_CALL(*mock_event_binding, PrepareStopOffer()).Times(0);
        EXPECT_CALL(*mock_event_binding2, PrepareStopOffer())
            .WillOnce(Invoke([&is_prepare_stop_offer_called2]() -> ResultBlank {
                is_prepare_stop_offer_called2 = true;
                return {};
            }));

        // When move assigning the second event to the first event
        skeleton_event = std::move(skeleton_event2);

        // Then PrepareStopOffer() is not called on the first event's binding
    }
    // and PrepareStopOffer() is called on the second event's binding on destruction
    EXPECT_TRUE(is_prepare_stop_offer_called2);
}

}  // namespace
}  // namespace score::mw::com::impl
