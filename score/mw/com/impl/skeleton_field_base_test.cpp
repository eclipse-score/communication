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
#include "score/mw/com/impl/skeleton_field_base.h"
#include "score/mw/com/impl/bindings/mock_binding/skeleton.h"
#include "score/mw/com/impl/bindings/mock_binding/skeleton_event.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/plumbing/skeleton_binding_factory.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/impl/runtime_mock.h"
#include "score/mw/com/impl/skeleton_base.h"
#include "score/mw/com/impl/skeleton_field.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
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

const auto kFieldName{"DummyField1"};

class MyDummyField : public SkeletonFieldBase
{
  public:
    MyDummyField()
        : SkeletonFieldBase{
              kEmptySkeleton,
              kFieldName,
              std::make_unique<SkeletonEventBase>(kEmptySkeleton,
                                                  kFieldName,
                                                  std::make_unique<StrictMock<mock_binding::SkeletonEventBase>>())}
    {
    }

    StrictMock<mock_binding::SkeletonEventBase>* GetMockEventBinding() noexcept
    {
        auto* const skeleton_field_base_binding = SkeletonFieldBaseView{*this}.GetEventBinding();
        auto* const mock_event_binding =
            dynamic_cast<StrictMock<mock_binding::SkeletonEventBase>*>(skeleton_field_base_binding);
        return mock_event_binding;
    }

    bool IsInitialValueSaved() const noexcept override
    {
        return is_initial_value_saved_;
    }

    ResultBlank DoDeferredUpdate() noexcept override
    {
        was_deferred_update_called_ = true;
        return {};
    }

    bool was_deferred_update_called_{false};
    bool is_initial_value_saved_{true};
};

class MyDummyFieldFailingDeferredUpdate final : public MyDummyField
{
  public:
    ResultBlank DoDeferredUpdate() noexcept override
    {
        return MakeUnexpected(ComErrc::kCommunicationLinkError);
    }
};

class SkeletonFieldBaseFixture : public ::testing::Test
{
  public:
    void CreateSkeletonField()
    {
        skeleton_field_ = std::make_unique<MyDummyField>();

        mock_event_binding_ = skeleton_field_->GetMockEventBinding();
        ASSERT_NE(mock_event_binding_, nullptr);
    }

    std::unique_ptr<MyDummyField> skeleton_field_{nullptr};
    StrictMock<mock_binding::SkeletonEventBase>* mock_event_binding_{nullptr};
};

TEST_F(SkeletonFieldBaseFixture, PrepareOfferDispatchesToBinding)
{
    // Given a constructed SkeletonField with a valid mock binding
    CreateSkeletonField();

    // Expecting that PrepareOffer() is called on the binding
    EXPECT_CALL(*mock_event_binding_, PrepareOffer()).WillOnce(Return(ResultBlank{}));

    // and that PrepareStopOffer() is called on destruction
    EXPECT_CALL(*mock_event_binding_, PrepareStopOffer());

    // When offering the event
    const auto result = skeleton_field_->PrepareOffer();

    // that the result is blank
    EXPECT_TRUE(result.has_value());
}

TEST_F(SkeletonFieldBaseFixture, PrepareStopOfferDispatchesToBinding)
{
    // Given a constructed SkeletonField with a valid mock binding
    CreateSkeletonField();

    // Expecting that PrepareOffer() is called on the binding
    EXPECT_CALL(*mock_event_binding_, PrepareOffer()).WillOnce(Return(ResultBlank{}));

    // and expecting that PrepareStopOffer() is called on the binding
    EXPECT_CALL(*mock_event_binding_, PrepareStopOffer());

    // When offering the event
    const auto result = skeleton_field_->PrepareOffer();
    EXPECT_TRUE(result.has_value());

    // When stopping the event offering
    skeleton_field_->PrepareStopOffer();
}

TEST_F(SkeletonFieldBaseFixture, PrepareOfferCallsInitialUpdateCallback)
{
    // Given a constructed SkeletonField with a valid mock binding
    CreateSkeletonField();

    // Expecting that PrepareOffer() is called on the binding
    EXPECT_CALL(*mock_event_binding_, PrepareOffer()).WillOnce(Return(ResultBlank{}));

    // and that PrepareStopOffer() is called on destruction
    EXPECT_CALL(*mock_event_binding_, PrepareStopOffer());

    // When offering the event
    EXPECT_FALSE(skeleton_field_->was_deferred_update_called_);
    const auto result = skeleton_field_->PrepareOffer();

    // that the result is blank
    EXPECT_TRUE(result.has_value());

    // and that the callback to set the initial value is called
    EXPECT_TRUE(skeleton_field_->was_deferred_update_called_);
}

TEST_F(SkeletonFieldBaseFixture, PrepareOfferPropagatesErrorFromBinding)
{
    // Given a constructed SkeletonField with a valid mock binding
    CreateSkeletonField();

    // When PrepareOffer() is called on the binding and returns an error
    EXPECT_CALL(*mock_event_binding_, PrepareOffer())
        .WillOnce(Return(score::MakeUnexpected(score::mw::com::impl::ComErrc::kInvalidBindingInformation)));

    // Expecting that when offering the event
    const auto result = skeleton_field_->PrepareOffer();

    // That the error is propagated
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), score::mw::com::impl::ComErrc::kInvalidBindingInformation);
}

TEST_F(SkeletonFieldBaseFixture, CallingPrepareOfferBeforeSettingInitialValueUpdateCallbackReturnsError)
{
    // Given a constructed SkeletonField with a valid mock binding
    CreateSkeletonField();

    // and the initial value is not set
    skeleton_field_->is_initial_value_saved_ = false;

    // Expecting that PrepareOffer() will not be called on the binding
    EXPECT_CALL(*mock_event_binding_, PrepareOffer()).Times(0);

    // When offering the event
    const auto result = skeleton_field_->PrepareOffer();

    // and the PrepareOffer() call should return an error message.
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kFieldValueIsNotValid);

    // and that PrepareStopOffer() is not called on destruction as the field was never offered
    EXPECT_CALL(*mock_event_binding_, PrepareStopOffer()).Times(0);
}

TEST_F(SkeletonFieldBaseFixture, PrepareStopOfferIsNotCalledIfFieldWasNotOffered)
{
    // Given a constructed SkeletonField with a valid mock binding
    CreateSkeletonField();

    // Expecting that PrepareStopOffer() is not called on the binding
    EXPECT_CALL(*mock_event_binding_, PrepareStopOffer()).Times(0);

    // When calling PrepareStopOffer
    skeleton_field_->PrepareStopOffer();

    // Or when destroying the event
}

TEST(SkeletonFieldBaseTest, PrepareOfferPropagatesErrorFromInitialValueUpdateCallback)
{
    // Given a constructed SkeletonField with a valid mock binding which has a DoDeferredUpdate function which will
    // return an error
    MyDummyFieldFailingDeferredUpdate skeleton_field;
    StrictMock<mock_binding::SkeletonEventBase>* mock_event_binding{skeleton_field.GetMockEventBinding()};
    ASSERT_NE(mock_event_binding, nullptr);

    // Expecting that PrepareOffer() is called on the binding
    EXPECT_CALL(*mock_event_binding, PrepareOffer()).WillOnce(Return(ResultBlank{}));

    // and that PrepareStopOffer() is called on destruction
    EXPECT_CALL(*mock_event_binding, PrepareStopOffer());

    // When offering the event
    const auto result = skeleton_field.PrepareOffer();

    // that the result contains an error
    EXPECT_FALSE(result.has_value());

    // and that error is the same error that was returned by the initial update callback
    EXPECT_EQ(result.error(), ComErrc::kCommunicationLinkError);
}

TEST(SkeletonFieldBaseTests, NotCopyable)
{
    RecordProperty("Verifies", "SCR-18221574");
    RecordProperty("Description", "Checks copy semantics for SkeletonFieldBase");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(!std::is_copy_constructible<MyDummyField>::value, "Is wrongly copyable");
    static_assert(!std::is_copy_assignable<MyDummyField>::value, "Is wrongly copyable");
}

TEST(SkeletonFieldBaseTests, IsMoveable)
{
    static_assert(std::is_move_constructible<MyDummyField>::value, "Is not move constructible");
    static_assert(std::is_move_assignable<MyDummyField>::value, "Is not move assignable");
}

TEST(SkeletonFieldBaseTests, StopOfferCalledDuringDestruction)
{
    StrictMock<mock_binding::SkeletonEventBase>* mock_event_binding{nullptr};
    bool is_offer_stopped{false};
    {
        // Given a skeleton with a field
        MyDummyField skeleton_field{};

        mock_event_binding = skeleton_field.GetMockEventBinding();
        ASSERT_NE(mock_event_binding, nullptr);

        // Expecting that PrepareOffer() is called on the binding
        EXPECT_CALL(*mock_event_binding, PrepareOffer()).WillOnce(Return(ResultBlank{}));

        // and that PrepareStopOffer() is called on the binding when the field goes out of scope
        EXPECT_CALL(*mock_event_binding, PrepareStopOffer()).WillOnce(Invoke([&is_offer_stopped]() -> ResultBlank {
            is_offer_stopped = true;
            return {};
        }));

        // When offering the event
        skeleton_field.PrepareOffer();

        // Then the offer is stopped on destruction
        EXPECT_FALSE(is_offer_stopped);
    }
    EXPECT_TRUE(is_offer_stopped);
}

TEST(SkeletonFieldBaseTests, MovingConstructingSkeletonFieldBaseDoesNotCallPrepareStopOffer)
{
    StrictMock<mock_binding::SkeletonEventBase>* mock_event_binding{nullptr};
    bool is_prepare_stop_offer_called{false};
    {
        // Given a constructed SkeletonField with a valid mock binding
        MyDummyField skeleton_field{};
        mock_event_binding = skeleton_field.GetMockEventBinding();
        ASSERT_NE(mock_event_binding, nullptr);

        // Expecting that PrepareOffer() is called on the binding
        EXPECT_CALL(*mock_event_binding, PrepareOffer());

        // Expecting that PrepareStopOffer() is called on the binding
        EXPECT_CALL(*mock_event_binding, PrepareStopOffer())
            .WillOnce(Invoke([&is_prepare_stop_offer_called]() -> ResultBlank {
                is_prepare_stop_offer_called = true;
                return {};
            }));

        // When offering the event
        skeleton_field.PrepareOffer();

        // When move constructing the second event from the first event
        MyDummyField skeleton_field_2{std::move(skeleton_field)};

        // not when the move constructor is called
        EXPECT_FALSE(is_prepare_stop_offer_called);
    }
    // but only when the skeleton is destroyed
    EXPECT_TRUE(is_prepare_stop_offer_called);
}

TEST(SkeletonFieldBaseTests, MovingAssigningOfferedSkeletonFieldBaseCallsPrepareStopOffer)
{
    StrictMock<mock_binding::SkeletonEventBase>* mock_event_binding{nullptr};
    StrictMock<mock_binding::SkeletonEventBase>* mock_event_binding2{nullptr};

    bool is_prepare_stop_offer_called{false};
    bool is_prepare_stop_offer_called2{false};

    {
        // Given a constructed SkeletonField with a valid mock binding
        MyDummyField skeleton_field{};
        mock_event_binding = skeleton_field.GetMockEventBinding();

        // Expecting that PrepareOffer() is called on the binding
        EXPECT_CALL(*mock_event_binding, PrepareOffer());

        // When offering the event
        skeleton_field.PrepareOffer();

        // and given a second SkeletonField with a valid mock binding
        MyDummyField skeleton_field2{};
        mock_event_binding2 = skeleton_field2.GetMockEventBinding();

        // Expecting that PrepareOffer() is called on the second binding
        EXPECT_CALL(*mock_event_binding2, PrepareOffer());

        // When offering the second event
        skeleton_field2.PrepareOffer();

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
        skeleton_field = std::move(skeleton_field2);

        // Then PrepareStopOffer() is called on the first event's binding
        EXPECT_TRUE(is_prepare_stop_offer_called);
        EXPECT_FALSE(is_prepare_stop_offer_called2);
    }
    // and PrepareStopOffer() is called on the second event's binding on destruction
    EXPECT_TRUE(is_prepare_stop_offer_called);
    EXPECT_TRUE(is_prepare_stop_offer_called2);
}

TEST(SkeletonFieldBaseTests, MovingAssigningUnOfferedSkeletonFieldBaseDoesNotCallPrepareStopOffer)
{
    StrictMock<mock_binding::SkeletonEventBase>* mock_event_binding{nullptr};
    StrictMock<mock_binding::SkeletonEventBase>* mock_event_binding2{nullptr};

    bool is_prepare_stop_offer_called2{false};

    {
        // Given a constructed SkeletonField with a valid mock binding
        MyDummyField skeleton_field{};
        mock_event_binding = skeleton_field.GetMockEventBinding();

        // and given a second SkeletonField with a valid mock binding
        MyDummyField skeleton_field2{};
        mock_event_binding2 = skeleton_field2.GetMockEventBinding();

        // Expecting that PrepareOffer() is called on the second binding
        EXPECT_CALL(*mock_event_binding2, PrepareOffer());

        // When offering the second event
        skeleton_field2.PrepareOffer();

        // and expecting that PrepareStopOffer() is only called on the offered event's binding
        EXPECT_CALL(*mock_event_binding, PrepareStopOffer()).Times(0);
        EXPECT_CALL(*mock_event_binding2, PrepareStopOffer())
            .WillOnce(Invoke([&is_prepare_stop_offer_called2]() -> ResultBlank {
                is_prepare_stop_offer_called2 = true;
                return {};
            }));

        // When move assigning the second event to the first event
        skeleton_field = std::move(skeleton_field2);

        // Then PrepareStopOffer() is not called on the first event's binding
    }
    // and PrepareStopOffer() is called on the second event's binding on destruction
    EXPECT_TRUE(is_prepare_stop_offer_called2);
}

}  // namespace
}  // namespace score::mw::com::impl
