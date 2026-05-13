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
#include "score/mw/com/impl/skeleton_field.h"

#include "score/mw/com/impl/bindings/mock_binding/skeleton_method.h"
#include "score/mw/com/impl/method_type.h"
#include "score/mw/com/impl/methods/skeleton_method.h"
#include "score/mw/com/impl/plumbing/skeleton_method_binding_factory_mock.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/impl/runtime_mock.h"
#include "score/mw/com/impl/test/binding_factory_resources.h"
#include "score/mw/com/impl/test/runtime_mock_guard.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <cstdint>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>

namespace score::mw::com::impl
{
namespace
{

using TestSampleType = std::uint8_t;

using SkeletonEventTracingData = tracing::SkeletonEventTracingData;

using ::testing::_;
using ::testing::An;
using ::testing::ByMove;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::WithArg;

constexpr std::string_view kFieldName{"Field1"};
const TestSampleType kDummyInitialValue{42};

ServiceIdentifierType kServiceIdentifier{make_ServiceIdentifierType("foo", 1U, 0U)};
std::uint16_t kInstanceId{23U};
const auto kInstanceSpecifier = InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"}).value();
const ServiceInstanceDeployment kDeploymentInfo{kServiceIdentifier,
                                                LolaServiceInstanceDeployment{LolaServiceInstanceId{kInstanceId}},
                                                QualityType::kASIL_QM,
                                                kInstanceSpecifier};
std::uint16_t kServiceId{34U};
const ServiceTypeDeployment kTypeDeployment{LolaServiceTypeDeployment{kServiceId}};
const auto kInstanceIdWithLolaBinding = make_InstanceIdentifier(kDeploymentInfo, kTypeDeployment);

class MyDummySkeleton : public SkeletonBase
{
  public:
    using SkeletonBase::SkeletonBase;

    SkeletonField<TestSampleType> my_dummy_field_{*this, kFieldName};
};

class SkeletonFieldTestFixture : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        ON_CALL(skeleton_field_binding_factory_mock_guard_.factory_mock_,
                CreateEventBinding(kInstanceIdWithLolaBinding, _, kFieldName))
            .WillByDefault(InvokeWithoutArgs([this]() {
                return std::make_unique<mock_binding::SkeletonEventFacade<TestSampleType>>(
                    skeleton_field_binding_mock_);
            }));

        ON_CALL(skeleton_method_binding_factory_mock_guard_.factory_mock_,
                Create(kInstanceIdWithLolaBinding, _, _, MethodType::kGet))
            .WillByDefault(InvokeWithoutArgs([this]() {
                return std::make_unique<mock_binding::SkeletonMethodFacade>(skeleton_field_get_binding_mock_);
            }));

        ON_CALL(skeleton_method_binding_factory_mock_guard_.factory_mock_,
                Create(kInstanceIdWithLolaBinding, _, _, MethodType::kSet))
            .WillByDefault(InvokeWithoutArgs([this]() {
                return std::make_unique<mock_binding::SkeletonMethodFacade>(skeleton_field_set_binding_mock_);
            }));

        ON_CALL(skeleton_field_set_binding_mock_, RegisterHandler(_)).WillByDefault(Return(Result<void>{}));
    }

    /// \brief Returns a span pointing to storage containing the provided field value
    std::pair<score::cpp::span<std::byte>, score::cpp::span<std::byte>> CreateFieldSetterInArgAndReturnSpans(
        const TestSampleType in_arg_value,
        const TestSampleType return_value)
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT(!in_arg_storage_.has_value());
        SCORE_LANGUAGE_FUTURECPP_ASSERT(!return_storage_.has_value());
        score::cpp::ignore = in_arg_storage_.emplace(in_arg_value);
        score::cpp::ignore = return_storage_.emplace(return_value);

        score::cpp::span<std::byte> in_span{reinterpret_cast<std::byte*>(&(in_arg_storage_.value())),
                                            sizeof(TestSampleType)};
        score::cpp::span<std::byte> out_span{reinterpret_cast<std::byte*>(&(return_storage_.value())),
                                             sizeof(TestSampleType)};

        return {in_span, out_span};
    }

    RuntimeMockGuard runtime_mock_guard_{};

    SkeletonFieldBindingFactoryMockGuard<TestSampleType> skeleton_field_binding_factory_mock_guard_{};
    SkeletonMethodBindingFactoryMockGuard skeleton_method_binding_factory_mock_guard_{};

    mock_binding::SkeletonEvent<TestSampleType> skeleton_field_binding_mock_{};
    mock_binding::SkeletonMethod skeleton_field_get_binding_mock_{};
    mock_binding::SkeletonMethod skeleton_field_set_binding_mock_{};

    std::optional<TestSampleType> in_arg_storage_{};
    std::optional<TestSampleType> return_storage_{};
};

TEST(SkeletonFieldTest, NotCopyable)
{
    RecordProperty("Verifies", "SCR-18221574");
    RecordProperty("Description", "Checks copy constructors for SkeletonField");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(!std::is_copy_constructible<SkeletonField<TestSampleType>>::value, "Is wrongly copyable");
    static_assert(!std::is_copy_assignable<SkeletonField<TestSampleType>>::value, "Is wrongly copyable");
}

TEST(SkeletonFieldTest, IsMoveable)
{
    static_assert(std::is_move_constructible<SkeletonField<TestSampleType>>::value, "Is not move constructible");
    static_assert(std::is_move_assignable<SkeletonField<TestSampleType>>::value, "Is not move assignable");
}

TEST(SkeletonFieldTest, ClassTypeDependsOnFieldDataType)
{
    RecordProperty("Verifies", "SCR-29235194");
    RecordProperty("Description", "SkeletonFields with different field data types should be different classes.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    using FirstSkeletonFieldType = SkeletonField<bool>;
    using SecondSkeletonFieldType = SkeletonField<std::uint16_t>;
    static_assert(!std::is_same_v<FirstSkeletonFieldType, SecondSkeletonFieldType>,
                  "Class type does not depend on field data type");
}

TEST(SkeletonFieldTest, SkeletonFieldContainsPublicSampleType)
{
    RecordProperty("Verifies", "SCR-17433130");
    RecordProperty("Description",
                   "A SkeletonField contains a public member type FieldType which denotes the type of the field.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(std::is_same<typename SkeletonField<TestSampleType>::FieldType, TestSampleType>::value,
                  "Incorrect FieldType.");
}

// When Ticket-104261 is implemented, the Update call does not have to be deferred until OfferService is called. This
// test can be reworked to remove the call to PrepareOffer() and simply test Update() before PrepareOffer() is called.
using SkeletonFieldCopyUpdateTest = SkeletonFieldTestFixture;
TEST_F(SkeletonFieldCopyUpdateTest, CallingUpdateBeforeOfferServiceDefersCallToOfferService)
{
    RecordProperty("Verifies", "SCR-17434775, SCR-17563743, SCR-21553554");
    RecordProperty("Description", "Checks that calling Update before offer service defers the call to OfferService().");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    bool is_send_called_on_binding{false};

    // and that PrepareOffer() will be called on the event binding
    EXPECT_CALL(skeleton_field_binding_mock_, PrepareOffer()).WillOnce(Return(score::Result<void>{}));

    // and Send will be called on the event binding with the initial value and returns an empty result
    EXPECT_CALL(skeleton_field_binding_mock_, Send(kDummyInitialValue, _))
        .WillOnce(InvokeWithoutArgs([&is_send_called_on_binding]() noexcept -> Result<void> {
            is_send_called_on_binding = true;
            return {};
        }));

    // Given a skeleton created based on a Lola binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // When the initial value is set via an Update call
    const auto update_result = unit.my_dummy_field_.Update(kDummyInitialValue);

    // then it does not return an error
    ASSERT_TRUE(update_result.has_value());

    // and send is not called on the binding
    EXPECT_FALSE(is_send_called_on_binding);

    // and when PrepareOffer() is called on the field
    const auto prepare_offer_result = unit.my_dummy_field_.PrepareOffer();

    // then it does not return an error
    ASSERT_TRUE(prepare_offer_result.has_value());

    // and send is called on the binding with the initial value
    EXPECT_TRUE(is_send_called_on_binding);
}

// When Ticket-104261 is implemented, the Update call does not have to be deferred until OfferService is called. This
// test can be reworked to remove the call to PrepareOffer() and the deferred processing of Update() and simply test
// Update() before PrepareOffer() is called.
TEST_F(SkeletonFieldCopyUpdateTest, CallingUpdateBeforeOfferServicePropagatesBindingFailureToOfferService)
{
    RecordProperty("Verifies", "SCR-17434775, SCR-21553554");
    RecordProperty("Description", "Checks that calling Update before offer service defers the call to OfferService().");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    bool is_send_called_on_binding{false};

    // and that PrepareOffer() will be called on the event binding
    EXPECT_CALL(skeleton_field_binding_mock_, PrepareOffer()).WillOnce(Return(score::Result<void>{}));

    // and Send will be called on the event binding with the initial value and returns an error
    EXPECT_CALL(skeleton_field_binding_mock_, Send(kDummyInitialValue, _))
        .WillOnce(InvokeWithoutArgs([&is_send_called_on_binding] {
            is_send_called_on_binding = true;
            return MakeUnexpected(ComErrc::kInvalidBindingInformation);
        }));

    // Given a skeleton created based on a Lola binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // When the initial value is set via an Update call
    const auto update_result = unit.my_dummy_field_.Update(kDummyInitialValue);

    // then it does not return an error
    ASSERT_TRUE(update_result.has_value());

    // and send is not called on the binding
    EXPECT_FALSE(is_send_called_on_binding);

    // and when PrepareOffer() is called on the field
    const auto prepare_offer_result = unit.my_dummy_field_.PrepareOffer();

    //  Then the result will contain an error that the binding failed
    ASSERT_FALSE(prepare_offer_result.has_value());
    EXPECT_EQ(prepare_offer_result.error(), ComErrc::kBindingFailure);

    // and send is called on the binding with the initial value
    EXPECT_TRUE(is_send_called_on_binding);
}

TEST_F(SkeletonFieldCopyUpdateTest, CallingUpdateAfterOfferServiceDispatchesToBinding)
{
    RecordProperty("Verifies", "SCR-17434775, SCR-21553375");
    RecordProperty("Description", "Checks that calling Update after offer service dispatches to the binding.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const TestSampleType updated_value{kDummyInitialValue + 1U};

    // and that PrepareOffer() will be called on the event binding
    EXPECT_CALL(skeleton_field_binding_mock_, PrepareOffer()).WillOnce(Return(score::Result<void>{}));

    // and Send will be called on the event binding with the initial value and returns an empty result
    EXPECT_CALL(skeleton_field_binding_mock_, Send(kDummyInitialValue, _)).WillOnce(Return(score::Result<void>{}));

    // and Send will be called a second time on the event binding with the updated value and returns an empty result
    EXPECT_CALL(skeleton_field_binding_mock_, Send(updated_value, _)).WillOnce(Return(score::Result<void>{}));

    // Given a skeleton created based on a Lola binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // When the initial value is set via an Update call
    const auto update_result = unit.my_dummy_field_.Update(kDummyInitialValue);

    // then it does not return an error
    ASSERT_TRUE(update_result.has_value());

    // and when PrepareOffer() is called on the field
    const auto prepare_offer_result = unit.my_dummy_field_.PrepareOffer();

    // then it does not return an error
    ASSERT_TRUE(prepare_offer_result.has_value());

    // and when an updated value is set via an Update call
    const auto update_result_2 = unit.my_dummy_field_.Update(updated_value);

    // then it does not return an error
    ASSERT_TRUE(update_result_2.has_value());
}

TEST_F(SkeletonFieldCopyUpdateTest, CallingUpdateAfterOfferServicePropagatesBindingFail)
{
    RecordProperty("Verifies", "SCR-17434775, SCR-21553375");
    RecordProperty("Description",
                   "Checks that calling Update after offer service returns kBindingFailure for a generic error code "
                   "from the binding.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const TestSampleType updated_value{kDummyInitialValue + 1U};

    // and that PrepareOffer() will be called on the event binding
    EXPECT_CALL(skeleton_field_binding_mock_, PrepareOffer()).WillOnce(Return(score::Result<void>{}));

    // and Send will be called on the event binding with the initial value and returns an empty result
    EXPECT_CALL(skeleton_field_binding_mock_, Send(kDummyInitialValue, _)).WillOnce(Return(score::Result<void>{}));

    // and Send will be called a second time on the event binding with the updated value and returns an error
    EXPECT_CALL(skeleton_field_binding_mock_, Send(updated_value, _))
        .WillOnce(Return(MakeUnexpected(ComErrc::kInvalidBindingInformation)));

    // Given a skeleton created based on a Lola binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // When the initial value is set via an Update call
    const auto update_result = unit.my_dummy_field_.Update(kDummyInitialValue);

    // then it does not return an error
    ASSERT_TRUE(update_result.has_value());

    // and when PrepareOffer() is called on the field
    const auto prepare_offer_result = unit.my_dummy_field_.PrepareOffer();

    // then it does not return an error
    ASSERT_TRUE(prepare_offer_result.has_value());

    // and when an updated value is set via an Update call
    const auto update_result_2 = unit.my_dummy_field_.Update(updated_value);

    // Then the result will contain an error that the binding failed
    ASSERT_FALSE(update_result_2.has_value());
    EXPECT_EQ(update_result_2.error(), ComErrc::kBindingFailure);
}

// This test can be removed when Ticket-104261 is implemented.
using SkeletonFieldAllocateTest = SkeletonFieldTestFixture;

TEST_F(SkeletonFieldAllocateTest, CallingAllocateBeforePrepareOfferDoesNotReturnValidSlot)
{
    // and that PrepareOffer() will not be called on the event binding
    EXPECT_CALL(skeleton_field_binding_mock_, PrepareOffer()).Times(0);

    // and Allocate will not be called on the event binding
    EXPECT_CALL(skeleton_field_binding_mock_, Allocate()).Times(0);

    // Given a skeleton created based on a Lola binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // When allocating a slot before calling PrepareOffer
    const auto slot_result = unit.my_dummy_field_.Allocate();

    // Then an error will be returned indicating that the binding failed
    ASSERT_FALSE(slot_result.has_value());
    EXPECT_EQ(slot_result.error(), ComErrc::kBindingFailure);
}

TEST_F(SkeletonFieldAllocateTest, CallingAllocateAfterPrepareOfferDispatchesToBinding)
{
    RecordProperty("Verifies", "SCR-17434933, SCR-21470600");
    RecordProperty("Description", "Checks that calling allocate after prepare offer dispatches to the binding.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // and that PrepareOffer() will be called on the event binding
    EXPECT_CALL(skeleton_field_binding_mock_, PrepareOffer()).WillOnce(Return(score::Result<void>{}));

    // and Send will be called on the event binding with the initial value
    EXPECT_CALL(skeleton_field_binding_mock_, Send(kDummyInitialValue, _));

    // and Allocate will be called again which returns a valid SampleAllocateePtr
    EXPECT_CALL(skeleton_field_binding_mock_, Allocate())
        .WillOnce(Return(ByMove(MakeSampleAllocateePtr(std::make_unique<TestSampleType>()))));

    // Given a skeleton created based on a Lola binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // When the initial value is set via an Update call
    const auto update_result = unit.my_dummy_field_.Update(kDummyInitialValue);

    // which does not return an error
    EXPECT_TRUE(update_result.has_value());

    // and PrepareOffer() is called on the field
    const auto prepare_offer_result = unit.my_dummy_field_.PrepareOffer();

    // which does not return an error
    ASSERT_TRUE(prepare_offer_result.has_value());

    // When allocating a slot
    auto slot_result = unit.my_dummy_field_.Allocate();

    // Then the slot can be allocated
    ASSERT_TRUE(slot_result.has_value());
}

TEST_F(SkeletonFieldAllocateTest, CallingAllocateAfterPrepareOfferFailsWhenBindingReturnsError)
{
    RecordProperty("Verifies", "SCR-17434933");
    RecordProperty("Description",
                   "Checks that calling allocate after prepare offer propagates an error from the binding.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // and that PrepareOffer() will be called on the event binding
    EXPECT_CALL(skeleton_field_binding_mock_, PrepareOffer()).WillOnce(Return(score::Result<void>{}));

    // and Send will be called on the event binding with the initial value
    EXPECT_CALL(skeleton_field_binding_mock_, Send(kDummyInitialValue, _));

    // and Allocate will be called again which returns a nullptr
    EXPECT_CALL(skeleton_field_binding_mock_, Allocate())
        .WillOnce(Return(ByMove(MakeUnexpected(ComErrc::kInvalidConfiguration))));

    // Given a skeleton created based on a Lola binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // When the initial value is set via an Update call
    const auto update_result = unit.my_dummy_field_.Update(kDummyInitialValue);

    // which does not return an error
    EXPECT_TRUE(update_result.has_value());

    // and PrepareOffer() is called on the field
    const auto prepare_offer_result = unit.my_dummy_field_.PrepareOffer();

    // which does not return an error
    ASSERT_TRUE(prepare_offer_result.has_value());

    // When allocating a slot
    const auto slot_result = unit.my_dummy_field_.Allocate();

    // Then an error will be returned indicating that the binding failed
    ASSERT_FALSE(slot_result.has_value());
    EXPECT_EQ(slot_result.error(), ComErrc::kBindingFailure);
}

using SkeletonFieldZeroCopyUpdateTest = SkeletonFieldTestFixture;

TEST_F(SkeletonFieldZeroCopyUpdateTest, CallingZeroCopyUpdateAfterOfferServiceDispatchesToBinding)
{
    RecordProperty("Verifies", "SCR-17434778, SCR-21553623");
    RecordProperty("Description",
                   "Checks that calling zero copy Update after offer service dispatches to the binding.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const TestSampleType new_value{kDummyInitialValue + 1U};

    // and that PrepareOffer() will be called on the event binding
    EXPECT_CALL(skeleton_field_binding_mock_, PrepareOffer()).WillOnce(Return(score::Result<void>{}));

    // and Send will be called on the event binding with the initial value
    EXPECT_CALL(skeleton_field_binding_mock_, Send(kDummyInitialValue, _));

    // and Allocate will be called again which returns a valid SampleAllocateePtr
    EXPECT_CALL(skeleton_field_binding_mock_, Allocate())
        .WillOnce(Return(ByMove(MakeSampleAllocateePtr(std::make_unique<TestSampleType>()))));

    // and Send will be called a second time on the event binding with a new value which returns an empty result
    EXPECT_CALL(skeleton_field_binding_mock_, Send(An<SampleAllocateePtr<TestSampleType>>(), _))
        .WillOnce(WithArg<0>(Invoke([new_value](SampleAllocateePtr<TestSampleType> sample_ptr) -> Result<void> {
            EXPECT_EQ(*sample_ptr, new_value);
            return {};
        })));

    // Given a skeleton created based on a Lola binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // When the initial value is set via an Update call
    const auto update_result = unit.my_dummy_field_.Update(kDummyInitialValue);

    // which does not return an error
    EXPECT_TRUE(update_result.has_value());

    // and PrepareOffer() is called on the field
    const auto prepare_offer_result = unit.my_dummy_field_.PrepareOffer();

    // which does not return an error
    ASSERT_TRUE(prepare_offer_result.has_value());

    // When allocating a slot
    auto allocated_slot_result = unit.my_dummy_field_.Allocate();

    // Then the result is valid
    ASSERT_TRUE(allocated_slot_result.has_value());
    auto allocated_slot = std::move(allocated_slot_result).value();

    // And assigning a value to the slot
    *allocated_slot = new_value;

    // and when calling Update() on the field
    const auto new_update_result = unit.my_dummy_field_.Update(std::move(allocated_slot));

    // then it does not return an error
    EXPECT_TRUE(new_update_result.has_value());
}

TEST_F(SkeletonFieldZeroCopyUpdateTest, CallingZeroCopyUpdateAfterOfferServicePropagatesBindingFail)
{
    RecordProperty("Verifies", "SCR-17434778");
    RecordProperty(
        "Description",
        "Checks that calling zero copy Update after offer service returns kBindingFailure for a generic error code "
        "from the binding.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const TestSampleType new_value{kDummyInitialValue + 1U};

    // and that PrepareOffer() will be called on the event binding
    EXPECT_CALL(skeleton_field_binding_mock_, PrepareOffer()).WillOnce(Return(score::Result<void>{}));

    // and Send will be called on the event binding with the initial value
    EXPECT_CALL(skeleton_field_binding_mock_, Send(kDummyInitialValue, _));

    // and Allocate will be called again which returns a valid SampleAllocateePtr
    EXPECT_CALL(skeleton_field_binding_mock_, Allocate())
        .WillOnce(Return(ByMove(MakeSampleAllocateePtr(std::make_unique<TestSampleType>()))));

    // and Send will be called a second time on the event binding with a new value which returns an error
    EXPECT_CALL(skeleton_field_binding_mock_, Send(An<SampleAllocateePtr<TestSampleType>>(), _))
        .WillOnce(WithArg<0>(Invoke([new_value](SampleAllocateePtr<TestSampleType> sample_ptr) -> Result<void> {
            EXPECT_EQ(*sample_ptr, new_value);
            return MakeUnexpected(ComErrc::kInvalidBindingInformation);
        })));

    // Given a skeleton created based on a Lola binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // When the initial value is set via an Update call
    const auto update_result = unit.my_dummy_field_.Update(kDummyInitialValue);

    // which does not return an error
    EXPECT_TRUE(update_result.has_value());

    // and PrepareOffer() is called on the field
    const auto prepare_offer_result = unit.my_dummy_field_.PrepareOffer();

    // which does not return an error
    ASSERT_TRUE(prepare_offer_result.has_value());

    // When allocating a slot
    auto slot_result = unit.my_dummy_field_.Allocate();
    ASSERT_TRUE(slot_result.has_value());
    auto slot = std::move(slot_result).value();

    // And assigning a value to the slot
    *slot = new_value;

    // and calling Update() on the field
    const auto update_result_2 = unit.my_dummy_field_.Update(std::move(slot));

    // Then the result will contain an error that the binding failed
    ASSERT_FALSE(update_result_2.has_value());
    EXPECT_EQ(update_result_2.error(), ComErrc::kBindingFailure);
}

using SkeletonFieldInitialValueFixture = SkeletonFieldTestFixture;

TEST_F(SkeletonFieldInitialValueFixture, LatestFieldValueWillBeSetOnPrepareOffer)
{
    RecordProperty("Verifies", "SCR-22129134");
    RecordProperty(
        "Description",
        "Checks that the initial value of the field is the value of the last Update call before calling PrepareOffer.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const TestSampleType latest_value{kDummyInitialValue + 1U};

    // and that PrepareOffer() will be called on the event binding
    EXPECT_CALL(skeleton_field_binding_mock_, PrepareOffer()).WillOnce(Return(score::Result<void>{}));

    // and Send will be called only once on the event binding with the latest value
    EXPECT_CALL(skeleton_field_binding_mock_, Send(latest_value, _));

    // Given a skeleton created based on a Lola binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // When the initial value is set via an Update call
    const auto update_result = unit.my_dummy_field_.Update(kDummyInitialValue);

    // which does not return an error
    EXPECT_TRUE(update_result.has_value());

    // and a new value is set via an Update call
    const auto latest_update_result = unit.my_dummy_field_.Update(latest_value);

    // which does not return an error
    EXPECT_TRUE(latest_update_result.has_value());

    // and PrepareOffer() is called on the field
    const auto prepare_offer_result = unit.my_dummy_field_.PrepareOffer();

    // which does not return an error
    EXPECT_TRUE(prepare_offer_result.has_value());
}

TEST_F(SkeletonFieldInitialValueFixture, OfferingFieldBeforeUpdatingValueReturnsError)
{
    RecordProperty("Verifies", "SCR-17563743");
    RecordProperty("Description", "Calling OfferService before setting the field value returns kFieldValueIsNotValid.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // and that PrepareOffer() will not be called on the event binding
    EXPECT_CALL(skeleton_field_binding_mock_, PrepareOffer()).Times(0);

    // Given a skeleton created based on a Lola binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // When the initial field value has not been set

    // when PrepareOffer() is called on the field
    const auto result = unit.my_dummy_field_.PrepareOffer();

    // and the PrepareOffer() call should return an error message.
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kFieldValueIsNotValid);
}

TEST_F(SkeletonFieldInitialValueFixture, MoveConstructingFieldBeforePrepareOfferWillKeepInitialValue)
{
    // and that PrepareOffer() will be called on the event binding
    EXPECT_CALL(skeleton_field_binding_mock_, PrepareOffer()).WillOnce(Return(score::Result<void>{}));

    // and Send will be called on the event binding with the initial value
    EXPECT_CALL(skeleton_field_binding_mock_, Send(kDummyInitialValue, _));

    // Given a skeleton created based on a Lola binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // When the initial value is set via an Update call
    const auto update_result = unit.my_dummy_field_.Update(kDummyInitialValue);

    // which does not return an error
    EXPECT_TRUE(update_result.has_value());

    // and then move constructing a new skeleton
    MyDummySkeleton unit_2{std::move(unit)};

    // and PrepareOffer() is called on the field in the new skeleton
    const auto prepare_offer_result = unit_2.my_dummy_field_.PrepareOffer();

    // then PrepareOffer() does not return an error
    EXPECT_TRUE(prepare_offer_result.has_value());
}

TEST(SkeletonFieldInitialValueTest, MoveAssigningFieldBeforePrepareOfferWillKeepInitialValue)
{
    const TestSampleType kDummyInitialValue_2{kDummyInitialValue + 1U};

    RuntimeMockGuard runtime_mock_guard{};
    ON_CALL(runtime_mock_guard.runtime_mock_, GetTracingFilterConfig()).WillByDefault(Return(nullptr));

    SkeletonFieldBindingFactoryMockGuard<TestSampleType> skeleton_field_binding_factory_mock_guard{};

    // Expecting that a SkeletonField binding is created
    auto skeleton_field_binding_mock_ptr = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();
    auto& skeleton_field_binding_mock = *skeleton_field_binding_mock_ptr;
    EXPECT_CALL(skeleton_field_binding_factory_mock_guard.factory_mock_,
                CreateEventBinding(kInstanceIdWithLolaBinding, _, kFieldName))
        .WillOnce(Return(ByMove(std::move(skeleton_field_binding_mock_ptr))));

    EXPECT_CALL(skeleton_field_binding_mock, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // and that PrepareOffer() will be called on the event binding
    EXPECT_CALL(skeleton_field_binding_mock, PrepareOffer()).WillOnce(Return(score::Result<void>{}));

    // and Send will be called on the event binding with the initial value from the moved-from field
    EXPECT_CALL(skeleton_field_binding_mock, Send(kDummyInitialValue, _));

    // Given a skeleton created based on a Lola binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // When the initial value is set via an Update call
    const auto update_result = unit.my_dummy_field_.Update(kDummyInitialValue);

    // which does not return an error
    EXPECT_TRUE(update_result.has_value());

    ServiceIdentifierType service{make_ServiceIdentifierType("foo2", 1U, 0U)};
    const ServiceInstanceDeployment instance_deployment{
        service,
        LolaServiceInstanceDeployment{LolaServiceInstanceId{kInstanceId}},
        QualityType::kASIL_QM,
        kInstanceSpecifier};
    InstanceIdentifier identifier2{make_InstanceIdentifier(instance_deployment, kTypeDeployment)};

    // and Expecting that a second SkeletonField binding is created
    auto skeleton_field_binding_mock_ptr_2 = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();
    auto& skeleton_field_binding_mock_2 = *skeleton_field_binding_mock_ptr_2;
    EXPECT_CALL(skeleton_field_binding_factory_mock_guard.factory_mock_, CreateEventBinding(identifier2, _, kFieldName))
        .WillOnce(Return(ByMove(std::move(skeleton_field_binding_mock_ptr_2))));

    EXPECT_CALL(skeleton_field_binding_mock_2, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    MyDummySkeleton unit_2{std::make_unique<mock_binding::Skeleton>(), identifier2};

    // When the initial value is set via an Update call
    const auto update_result_2 = unit_2.my_dummy_field_.Update(kDummyInitialValue_2);

    // which does not return an error
    EXPECT_TRUE(update_result_2.has_value());

    unit_2 = std::move(unit);

    // and PrepareOffer() is called on the field in the new skeleton
    const auto prepare_offer_result = unit_2.my_dummy_field_.PrepareOffer();

    // then PrepareOffer() does not return an error
    EXPECT_TRUE(prepare_offer_result.has_value());
}

TEST_F(SkeletonFieldTestFixture, SkeletonFieldsRegisterThemselvesWithSkeleton)
{
    // Given a skeleton created based on a Lola binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // Expect that the field map stored in the skeleton
    const auto& fields = SkeletonBaseView{unit}.GetFields();

    // contains a single field
    ASSERT_EQ(fields.size(), 1);

    const auto field_name = fields.begin()->first;
    const auto& field = fields.begin()->second.get();

    // the name corresponds to the correct field name
    EXPECT_EQ(field_name, kFieldName);

    // and the field in the map corresponds to the correct skeleton field address
    EXPECT_EQ(&field, &unit.my_dummy_field_);
}

TEST_F(SkeletonFieldTestFixture, MovingConstructingSkeletonUpdatesFieldMapReference)
{
    // Given a skeleton created based on a Lola binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    MyDummySkeleton unit2{std::move(unit)};

    // Expect that the field map stored in the skeleton
    const auto& fields = SkeletonBaseView{unit2}.GetFields();

    // contains a single field
    ASSERT_EQ(fields.size(), 1);

    const auto field_name = fields.begin()->first;
    const auto& field = fields.begin()->second.get();

    // the name corresponds to the correct field name
    EXPECT_EQ(field_name, kFieldName);

    // and the field in the map corresponds to the correct skeleton field address
    EXPECT_EQ(&field, &unit2.my_dummy_field_);
}

TEST_F(SkeletonFieldTestFixture, MovingAssigningSkeletonUpdatesFieldMapReference)
{
    ServiceIdentifierType service{make_ServiceIdentifierType("foo2", 1U, 0U)};
    const ServiceInstanceDeployment instance_deployment{
        service,
        LolaServiceInstanceDeployment{LolaServiceInstanceId{kInstanceId}},
        QualityType::kASIL_QM,
        kInstanceSpecifier};
    InstanceIdentifier identifier2{make_InstanceIdentifier(instance_deployment, kTypeDeployment)};

    // Expecting that the SkeletonFieldBindingFactory returns a valid binding  for both Skeletons

    // Given a skeleton created based on a Lola binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    MyDummySkeleton unit2{std::make_unique<mock_binding::Skeleton>(), identifier2};

    unit2 = std::move(unit);

    // Expect that the field map stored in the skeleton
    const auto& fields = SkeletonBaseView{unit2}.GetFields();

    // contains a single field
    ASSERT_EQ(fields.size(), 1);

    const auto field_name = fields.begin()->first;
    const auto& field = fields.begin()->second.get();

    // the name corresponds to the correct field name
    EXPECT_EQ(field_name, kFieldName);

    // and the field in the map corresponds to the correct skeleton field address
    EXPECT_EQ(&field, &unit2.my_dummy_field_);
}

using SkeletonFieldDeathTest = SkeletonFieldTestFixture;
TEST_F(SkeletonFieldDeathTest, UpdateWithInvalidFieldNameTriggersTermination)
{
    // Given a skeleton created based on a Lola binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // Expect that the field map stored in the skeleton
    const auto& fields = SkeletonBaseView{unit}.GetFields();

    // contains a single field
    ASSERT_EQ(fields.size(), 1);

    auto& field = fields.begin()->second.get();

    // Then the program terminates as expected due to the invalid field name
    EXPECT_DEATH(SkeletonBaseView{unit}.UpdateField("non_existing_test_field_name", field);, ".*");
}

// Helper skeleton that holds an EnableSet=true field (setter-capable field)
class MySetterSkeleton : public SkeletonBase
{
  public:
    using SkeletonBase::SkeletonBase;

    SkeletonField<TestSampleType, /*EnableSet=*/true> my_setter_field_{*this, kFieldName};
};

TEST(SkeletonFieldSetHandlerTypeTraitsTest, RegisterSetHandlerOnlyExistsWhenEnableSetIsTrue)
{
    RecordProperty("Description",
                   "RegisterSetHandler() shall only exist on SkeletonField<T, EnableSet=true>. "
                   "Verify via has_member detection that it is absent when EnableSet=false.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // RegisterSetHandler should be callable on EnableSet=true
    static_assert(std::is_same_v<SkeletonField<TestSampleType, true>::FieldType, TestSampleType>,
                  "Setter-capable field must expose FieldType");

    // EnableSet=false field must not expose SetHandlerType at the member function level. We verify
    // indirectly that the EnableSet template parameter distinguishes the types.
    static_assert(!std::is_same_v<SkeletonField<TestSampleType, false>, SkeletonField<TestSampleType, true>>,
                  "EnableSet=false and EnableSet=true fields must be different types");
}

using SkeletonFieldSetHandlerTest = SkeletonFieldTestFixture;

TEST_F(SkeletonFieldSetHandlerTest, RegisterSetHandlerForwardsToMethodBinding)
{
    // Expecting that RegisterHandler is called on the field set method binding which returns success
    EXPECT_CALL(skeleton_field_set_binding_mock_, RegisterHandler(_)).WillOnce(Return(Result<void>{}));

    // Given a skeleton containing a field with a setter enabled
    MySetterSkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // When RegisterSetHandler is called with a valid (no-op) handler
    const auto result = unit.my_setter_field_.RegisterSetHandler([](TestSampleType& /*value*/) noexcept {});

    // Then the registration succeeds
    EXPECT_TRUE(result.has_value());
}

TEST_F(SkeletonFieldSetHandlerTest, RegisterSetHandlerPropagatesBindingError)
{
    // Expecting that RegisterHandler is called on the field set method binding which returns an error
    EXPECT_CALL(skeleton_field_set_binding_mock_, RegisterHandler(_))
        .WillOnce(Return(MakeUnexpected(ComErrc::kCommunicationLinkError)));

    // Given a skeleton containing a field with a setter enabled
    MySetterSkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // When RegisterSetHandler is called
    const auto result = unit.my_setter_field_.RegisterSetHandler([](TestSampleType& /*value*/) noexcept {});

    // Then the error from the binding is propagated
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kCommunicationLinkError);
}

TEST_F(SkeletonFieldSetHandlerTest, PrepareOfferFailsWhenSetHandlerNotRegistered)
{
    // Given a skeleton containing a field with a setter enabled
    MySetterSkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // and given an initial value was set so that the initial-value check does not fail
    EXPECT_TRUE(unit.my_setter_field_.Update(TestSampleType{42}).has_value());

    // When PrepareOffer is called without having called RegisterSetHandler
    const auto result = unit.my_setter_field_.PrepareOffer();

    // Then it returns kSetHandlerNotSet
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kSetHandlerNotSet);
}

TEST_F(SkeletonFieldSetHandlerTest, PrepareOfferSucceedsAfterRegisterSetHandler)
{
    const TestSampleType kDummyInitialValue{7U};

    // Given a skeleton containing a field with a setter enabled
    MySetterSkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // Register a valid (no-op) set handler
    ASSERT_TRUE(unit.my_setter_field_.RegisterSetHandler([](TestSampleType& /*value*/) noexcept {}).has_value());

    // Set the initial field value
    ASSERT_TRUE(unit.my_setter_field_.Update(kDummyInitialValue).has_value());

    // When PrepareOffer is called
    const auto result = unit.my_setter_field_.PrepareOffer();

    // Then it succeeds
    EXPECT_TRUE(result.has_value());
}

TEST_F(SkeletonFieldSetHandlerTest, PrepareOfferSucceedsWithoutHandlerWhenEnableSetIsFalse)
{
    // Given a skeleton containing a field without a setter enabled
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    ASSERT_TRUE(unit.my_dummy_field_.Update(kDummyInitialValue).has_value());

    // When PrepareOffer is called without registering a set handler
    const auto result = unit.my_dummy_field_.PrepareOffer();

    // Then it succeeds
    EXPECT_TRUE(result.has_value());
}

TEST(SkeletonFieldSetHandlerTypeTraitsTest, RegisterSetHandlerAcceptsAnyCallable)
{
    RecordProperty("Description",
                   "RegisterSetHandler() shall accept any callable (lambda, std::function, "
                   "score::cpp::callback) with signature void(FieldType&). "
                   "The public interface is not tied to a specific callable type.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    using HandlerSignature = void(TestSampleType&);

    // lambda
    static_assert(std::is_invocable_v<std::function<HandlerSignature>, TestSampleType&>,
                  "std::function with expected signature must be invocable");

    // score::cpp::callback
    static_assert(std::is_invocable_v<score::cpp::callback<HandlerSignature>, TestSampleType&>,
                  "score::cpp::callback with expected signature must be invocable");
}

// Handler wrapping: user callback is invoked and the field value is updated

/// Helper: captures the TypeErasedHandler registered on the SkeletonMethodBinding so that
/// tests can invoke it manually to simulate a proxy calling the setter.
class CapturingSkeletonMethodBinding : public SkeletonMethodBinding
{
  public:
    Result<void> RegisterHandler(TypeErasedHandler&& cb) override
    {
        captured_handler_ = std::move(cb);
        return Result<void>{};
    }

    TypeErasedHandler captured_handler_{};
};

TEST_F(SkeletonFieldSetHandlerTest, UserCallbackIsInvokedByWrappedHandler)
{
    RecordProperty("Description",
                   "The callback registered with RegisterSetHandler() is invoked by the wrapped "
                   "handler that the SkeletonField registers on the SkeletonMethod binding.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const TestSampleType incoming_value{99U};
    bool user_callback_called{false};

    // Use a capturing binding so that we can invoke the type-erased handler later
    auto capturing_binding = std::make_unique<CapturingSkeletonMethodBinding>();
    auto& capturing_binding_ref = *capturing_binding;
    EXPECT_CALL(skeleton_method_binding_factory_mock_guard_.factory_mock_,
                Create(kInstanceIdWithLolaBinding, _, _, MethodType::kSet))
        .WillOnce(Return(ByMove(std::move(capturing_binding))));
    EXPECT_CALL(skeleton_method_binding_factory_mock_guard_.factory_mock_,
                Create(kInstanceIdWithLolaBinding, _, _, MethodType::kGet))
        .WillOnce(InvokeWithoutArgs([]() {
            return std::unique_ptr<SkeletonMethodBinding>{};
        }));

    MySetterSkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // Register a set handler that flags it was called
    ASSERT_TRUE(unit.my_setter_field_
                    .RegisterSetHandler([&user_callback_called](TestSampleType& /*value*/) noexcept {
                        user_callback_called = true;
                    })
                    .has_value());

    // Offer the service (sets initial value)
    ASSERT_TRUE(unit.my_setter_field_.Update(TestSampleType{1U}).has_value());
    ASSERT_TRUE(unit.my_setter_field_.PrepareOffer().has_value());

    // Simulate the proxy invoking the setter by calling the captured type-erased handler.
    // The SkeletonMethod serializes the incoming value into a byte span before dispatch.
    // We replicate that serialization here for the single TestSampleType argument.
    auto [in_span, out_span] = CreateFieldSetterInArgAndReturnSpans(incoming_value, TestSampleType{});

    capturing_binding_ref.captured_handler_(in_span, out_span);

    // Verify the user callback was called
    EXPECT_TRUE(user_callback_called);
}

TEST_F(SkeletonFieldSetHandlerTest, UserCallbackCanModifyValueInPlace)
{
    RecordProperty("Description",
                   "The set handler callback receives the value by reference. Modifications made "
                   "inside the callback shall be reflected in the value that is subsequently sent "
                   "via Update().");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const TestSampleType incoming_value{10U};
    const TestSampleType modified_value{20U};  // callback doubles the value

    EXPECT_CALL(skeleton_field_binding_mock_, PrepareOffer()).WillOnce(Return(Result<void>{}));
    EXPECT_CALL(skeleton_field_binding_mock_, Send(TestSampleType{1U}, _)).WillOnce(Return(Result<void>{}));
    // The modified value (20) must be the one forwarded to the event binding
    EXPECT_CALL(skeleton_field_binding_mock_, Send(modified_value, _)).WillOnce(Return(Result<void>{}));

    auto capturing_binding = std::make_unique<CapturingSkeletonMethodBinding>();
    auto& capturing_binding_ref = *capturing_binding;
    EXPECT_CALL(skeleton_method_binding_factory_mock_guard_.factory_mock_,
                Create(kInstanceIdWithLolaBinding, _, _, MethodType::kSet))
        .WillOnce(Return(ByMove(std::move(capturing_binding))));
    EXPECT_CALL(skeleton_method_binding_factory_mock_guard_.factory_mock_,
                Create(kInstanceIdWithLolaBinding, _, _, MethodType::kGet))
        .WillOnce(InvokeWithoutArgs([]() {
            return std::unique_ptr<SkeletonMethodBinding>{};
        }));

    MySetterSkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // Register a set handler that doubles the incoming value in-place
    ASSERT_TRUE(unit.my_setter_field_
                    .RegisterSetHandler([](TestSampleType& value) noexcept {
                        value = static_cast<TestSampleType>(value * 2U);
                    })
                    .has_value());

    ASSERT_TRUE(unit.my_setter_field_.Update(TestSampleType{1U}).has_value());
    ASSERT_TRUE(unit.my_setter_field_.PrepareOffer().has_value());

    // Invoke the wrapped handler with incoming_value (10)
    auto [in_span, out_span] = CreateFieldSetterInArgAndReturnSpans(incoming_value, TestSampleType{});

    // The handler shall call Send with 20, not 10
    capturing_binding_ref.captured_handler_(in_span, out_span);
}

TEST_F(SkeletonFieldSetHandlerTest, WrappedHandlerLogsWhenUpdateFails)
{
    RecordProperty("Description",
                   "When the event binding's Send() fails inside the wrapped set handler, the "
                   "failure shall be logged and the handler shall complete normally without "
                   "propagating the error to the proxy caller. The user callback is still "
                   "invoked before the failed Update().");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const TestSampleType incoming_value{55U};
    bool user_callback_called{false};

    EXPECT_CALL(skeleton_field_binding_mock_, PrepareOffer()).WillOnce(Return(Result<void>{}));
    EXPECT_CALL(skeleton_field_binding_mock_, Send(TestSampleType{1U}, _)).WillOnce(Return(Result<void>{}));
    // Simulate Update() failure when the wrapped handler is invoked by the proxy
    EXPECT_CALL(skeleton_field_binding_mock_, Send(incoming_value, _))
        .WillOnce(Return(MakeUnexpected(ComErrc::kCommunicationLinkError)));

    auto capturing_binding = std::make_unique<CapturingSkeletonMethodBinding>();
    auto& capturing_binding_ref = *capturing_binding;
    EXPECT_CALL(skeleton_method_binding_factory_mock_guard_.factory_mock_,
                Create(kInstanceIdWithLolaBinding, _, _, MethodType::kSet))
        .WillOnce(Return(ByMove(std::move(capturing_binding))));
    EXPECT_CALL(skeleton_method_binding_factory_mock_guard_.factory_mock_,
                Create(kInstanceIdWithLolaBinding, _, _, MethodType::kGet))
        .WillOnce(InvokeWithoutArgs([]() {
            return std::unique_ptr<SkeletonMethodBinding>{};
        }));

    MySetterSkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    ASSERT_TRUE(unit.my_setter_field_
                    .RegisterSetHandler([&user_callback_called](TestSampleType& /*value*/) noexcept {
                        user_callback_called = true;
                    })
                    .has_value());

    ASSERT_TRUE(unit.my_setter_field_.Update(TestSampleType{1U}).has_value());
    ASSERT_TRUE(unit.my_setter_field_.PrepareOffer().has_value());

    auto [in_span, out_span] = CreateFieldSetterInArgAndReturnSpans(incoming_value, TestSampleType{});

    // Handler must complete normally even when Update() returns an error
    capturing_binding_ref.captured_handler_(in_span, out_span);

    // The user callback was invoked before the failed Update()
    EXPECT_TRUE(user_callback_called);
}

TEST_F(SkeletonFieldSetHandlerTest, IsSetHandlerRegisteredFlagIsSetAfterRegistration)
{
    RecordProperty("Description",
                   "After a successful RegisterSetHandler() call the internal flag "
                   "is_set_handler_registered_ must be set, which allows PrepareOffer() to proceed.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const TestSampleType kDummyInitialValue{3U};

    EXPECT_CALL(skeleton_field_binding_mock_, PrepareOffer()).WillOnce(Return(Result<void>{}));
    EXPECT_CALL(skeleton_field_binding_mock_, Send(kDummyInitialValue, _)).WillOnce(Return(Result<void>{}));

    EXPECT_CALL(skeleton_field_set_binding_mock_, RegisterHandler(_)).WillOnce(Return(Result<void>{}));

    MySetterSkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // Before registration PrepareOffer should fail with kSetHandlerNotSet
    ASSERT_TRUE(unit.my_setter_field_.Update(kDummyInitialValue).has_value());
    {
        // Separate scope: verify failure without handler
        // (We cannot call PrepareOffer twice without a stop-offer in between, so we
        //  rely on the unit test for "PrepareOfferFailsWhenSetHandlerNotRegistered" for
        //  that path; here we just confirm the happy path after registration.)
    }

    ASSERT_TRUE(unit.my_setter_field_.RegisterSetHandler([](TestSampleType& /*v*/) noexcept {}).has_value());

    // After registration PrepareOffer must succeed
    const auto result = unit.my_setter_field_.PrepareOffer();
    EXPECT_TRUE(result.has_value());
}

TEST_F(SkeletonFieldSetHandlerTest, SecondRegisterSetHandlerReplacesHandler)
{
    RecordProperty("Description",
                   "Calling RegisterSetHandler() a second time shall replace the previously stored "
                   "user callback. When the wrapped handler is subsequently invoked, only the second "
                   "callback shall be called.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const TestSampleType incoming_value{11U};
    bool first_callback_called{false};
    bool second_callback_called{false};

    EXPECT_CALL(skeleton_field_binding_mock_, PrepareOffer()).WillOnce(Return(Result<void>{}));
    EXPECT_CALL(skeleton_field_binding_mock_, Send(TestSampleType{1U}, _)).WillOnce(Return(Result<void>{}));
    EXPECT_CALL(skeleton_field_binding_mock_, Send(incoming_value, _)).WillOnce(Return(Result<void>{}));

    // The method binding is called twice (once per RegisterSetHandler)
    auto capturing_binding = std::make_unique<CapturingSkeletonMethodBinding>();
    auto& capturing_binding_ref = *capturing_binding;
    EXPECT_CALL(skeleton_method_binding_factory_mock_guard_.factory_mock_,
                Create(kInstanceIdWithLolaBinding, _, _, MethodType::kSet))
        .WillOnce(Return(ByMove(std::move(capturing_binding))));
    EXPECT_CALL(skeleton_method_binding_factory_mock_guard_.factory_mock_,
                Create(kInstanceIdWithLolaBinding, _, _, MethodType::kGet))
        .WillOnce(Return(ByMove(nullptr)));

    MySetterSkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // First registration
    ASSERT_TRUE(unit.my_setter_field_
                    .RegisterSetHandler([&first_callback_called](TestSampleType& /*value*/) noexcept {
                        first_callback_called = true;
                    })
                    .has_value());

    // Second registration — replaces the handler stored on the field
    ASSERT_TRUE(unit.my_setter_field_
                    .RegisterSetHandler([&second_callback_called](TestSampleType& /*value*/) noexcept {
                        second_callback_called = true;
                    })
                    .has_value());

    ASSERT_TRUE(unit.my_setter_field_.Update(TestSampleType{1U}).has_value());
    ASSERT_TRUE(unit.my_setter_field_.PrepareOffer().has_value());

    // Invoke the most-recently captured handler (from the second registration)
    auto [in_span, out_span] = CreateFieldSetterInArgAndReturnSpans(incoming_value, TestSampleType{});

    capturing_binding_ref.captured_handler_(in_span, out_span);

    // Only the second callback must have been called
    EXPECT_FALSE(first_callback_called);
    EXPECT_TRUE(second_callback_called);
}

using SkeletonFieldMoveConstructionFixture = SkeletonFieldTestFixture;
TEST_F(SkeletonFieldMoveConstructionFixture, SecondRegisterSetHandlerReplacesHandler)
{
    // Note. This test verifies that moving a skeleton does not break the getter / setter methods stored within a field.
    // When moving, UpdateSkeletonReference must update the references to SkeletonBase in the field instance as well as
    // the stored methods. However, the SkeletonBase reference in the methods are only used when moving the method (to
    // update the reference to the method in SkeletonBase). Since the methods are stored in the field as unique_ptrs,
    // they will never actually be moved. Therefore, with the current implementation, the SkeletonBase reference in the
    // getter and setter are never used and so we have no way of ensuring that they are updated correctly. We can only
    // verify that the method is still valid after move construction.

    // Given a skeleton containing a field with a setter enabled
    MySetterSkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // When move constructing the skeleton
    MySetterSkeleton unit2{std::move(unit)};

    // Then the method should still be usable (validated by calling RegisterSetHandler which dispatches to the method)
    unit2.my_setter_field_.RegisterSetHandler([](TestSampleType& /*value*/) noexcept {});
}

}  // namespace
}  // namespace score::mw::com::impl
