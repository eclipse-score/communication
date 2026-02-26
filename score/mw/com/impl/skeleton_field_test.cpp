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

// When Ticket-104261 is implemented, the Update call does not have to be deferred until OfferService is called. This test
// can be reworked to remove the call to PrepareOffer() and simply test Update() before PrepareOffer() is called.
TEST(SkeletonFieldCopyUpdateTest, CallingUpdateBeforeOfferServiceDefersCallToOfferService)
{
    RecordProperty("Verifies", "SCR-17434775, SCR-17563743, SCR-21553554");
    RecordProperty("Description", "Checks that calling Update before offer service defers the call to OfferService().");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const TestSampleType initial_value{42};
    bool is_send_called_on_binding{false};

    RuntimeMockGuard runtime_mock_guard{};
    ON_CALL(runtime_mock_guard.runtime_mock_, GetTracingFilterConfig()).WillByDefault(Return(nullptr));

    SkeletonFieldBindingFactoryMockGuard<TestSampleType> skeleton_field_binding_factory_mock_guard{};

    // Expecting that a SkeletonField binding is created
    auto skeleton_field_binding_mock_ptr = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();
    auto& skeleton_field_binding_mock = *skeleton_field_binding_mock_ptr;
    EXPECT_CALL(skeleton_field_binding_factory_mock_guard.factory_mock_,
                CreateEventBinding(kInstanceIdWithLolaBinding, _, kFieldName))
        .WillOnce(Return(ByMove(std::move(skeleton_field_binding_mock_ptr))));

    // and that PrepareOffer() will be called on the event binding
    EXPECT_CALL(skeleton_field_binding_mock, PrepareOffer()).WillOnce(Return(score::ResultBlank{}));

    // and Send will be called on the event binding with the initial value and returns an empty result
    EXPECT_CALL(skeleton_field_binding_mock, Send(initial_value, _))
        .WillOnce(InvokeWithoutArgs([&is_send_called_on_binding]() noexcept -> ResultBlank {
            is_send_called_on_binding = true;
            return {};
        }));

    // Given a skeleton created based on a Lola binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // When the initial value is set via an Update call
    const auto update_result = unit.my_dummy_field_.Update(initial_value);

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

// When Ticket-104261 is implemented, the Update call does not have to be deferred until OfferService is called. This test
// can be reworked to remove the call to PrepareOffer() and the deferred processing of Update() and simply test Update()
// before PrepareOffer() is called.
TEST(SkeletonFieldCopyUpdateTest, CallingUpdateBeforeOfferServicePropagatesBindingFailureToOfferService)
{
    RecordProperty("Verifies", "SCR-17434775, SCR-21553554");
    RecordProperty("Description", "Checks that calling Update before offer service defers the call to OfferService().");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const TestSampleType initial_value{42};
    bool is_send_called_on_binding{false};

    RuntimeMockGuard runtime_mock_guard{};
    ON_CALL(runtime_mock_guard.runtime_mock_, GetTracingFilterConfig()).WillByDefault(Return(nullptr));

    SkeletonFieldBindingFactoryMockGuard<TestSampleType> skeleton_field_binding_factory_mock_guard{};

    // Expecting that a SkeletonField binding is created
    auto skeleton_field_binding_mock_ptr = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();
    auto& skeleton_field_binding_mock = *skeleton_field_binding_mock_ptr;
    EXPECT_CALL(skeleton_field_binding_factory_mock_guard.factory_mock_,
                CreateEventBinding(kInstanceIdWithLolaBinding, _, kFieldName))
        .WillOnce(Return(ByMove(std::move(skeleton_field_binding_mock_ptr))));

    // and that PrepareOffer() will be called on the event binding
    EXPECT_CALL(skeleton_field_binding_mock, PrepareOffer()).WillOnce(Return(score::ResultBlank{}));

    // and Send will be called on the event binding with the initial value and returns an error
    EXPECT_CALL(skeleton_field_binding_mock, Send(initial_value, _))
        .WillOnce(InvokeWithoutArgs([&is_send_called_on_binding] {
            is_send_called_on_binding = true;
            return MakeUnexpected(ComErrc::kInvalidBindingInformation);
        }));

    // Given a skeleton created based on a Lola binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // When the initial value is set via an Update call
    const auto update_result = unit.my_dummy_field_.Update(initial_value);

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

TEST(SkeletonFieldCopyUpdateTest, CallingUpdateAfterOfferServiceDispatchesToBinding)
{
    RecordProperty("Verifies", "SCR-17434775, SCR-21553375");
    RecordProperty("Description", "Checks that calling Update after offer service dispatches to the binding.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const TestSampleType initial_value{42};
    const TestSampleType updated_value{43};

    RuntimeMockGuard runtime_mock_guard{};
    ON_CALL(runtime_mock_guard.runtime_mock_, GetTracingFilterConfig()).WillByDefault(Return(nullptr));

    SkeletonFieldBindingFactoryMockGuard<TestSampleType> skeleton_field_binding_factory_mock_guard{};

    // Expecting that a SkeletonField binding is created
    auto skeleton_field_binding_mock_ptr = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();
    auto& skeleton_field_binding_mock = *skeleton_field_binding_mock_ptr;
    EXPECT_CALL(skeleton_field_binding_factory_mock_guard.factory_mock_,
                CreateEventBinding(kInstanceIdWithLolaBinding, _, kFieldName))
        .WillOnce(Return(ByMove(std::move(skeleton_field_binding_mock_ptr))));

    // and that PrepareOffer() will be called on the event binding
    EXPECT_CALL(skeleton_field_binding_mock, PrepareOffer()).WillOnce(Return(score::ResultBlank{}));

    // and Send will be called on the event binding with the initial value and returns an empty result
    EXPECT_CALL(skeleton_field_binding_mock, Send(initial_value, _)).WillOnce(Return(score::ResultBlank{}));

    // and Send will be called a second time on the event binding with the updated value and returns an empty result
    EXPECT_CALL(skeleton_field_binding_mock, Send(updated_value, _)).WillOnce(Return(score::ResultBlank{}));

    // Given a skeleton created based on a Lola binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // When the initial value is set via an Update call
    const auto update_result = unit.my_dummy_field_.Update(initial_value);

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

TEST(SkeletonFieldCopyUpdateTest, CallingUpdateAfterOfferServicePropagatesBindingFail)
{
    RecordProperty("Verifies", "SCR-17434775, SCR-21553375");
    RecordProperty("Description",
                   "Checks that calling Update after offer service returns kBindingFailure for a generic error code "
                   "from the binding.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const TestSampleType initial_value{42};
    const TestSampleType updated_value{43};

    RuntimeMockGuard runtime_mock_guard{};
    ON_CALL(runtime_mock_guard.runtime_mock_, GetTracingFilterConfig()).WillByDefault(Return(nullptr));

    SkeletonFieldBindingFactoryMockGuard<TestSampleType> skeleton_field_binding_factory_mock_guard{};

    // Expecting that a SkeletonField binding is created
    auto skeleton_field_binding_mock_ptr = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();
    auto& skeleton_field_binding_mock = *skeleton_field_binding_mock_ptr;
    EXPECT_CALL(skeleton_field_binding_factory_mock_guard.factory_mock_,
                CreateEventBinding(kInstanceIdWithLolaBinding, _, kFieldName))
        .WillOnce(Return(ByMove(std::move(skeleton_field_binding_mock_ptr))));

    // and that PrepareOffer() will be called on the event binding
    EXPECT_CALL(skeleton_field_binding_mock, PrepareOffer()).WillOnce(Return(score::ResultBlank{}));

    // and Send will be called on the event binding with the initial value and returns an empty result
    EXPECT_CALL(skeleton_field_binding_mock, Send(initial_value, _)).WillOnce(Return(score::ResultBlank{}));

    // and Send will be called a second time on the event binding with the updated value and returns an error
    EXPECT_CALL(skeleton_field_binding_mock, Send(updated_value, _))
        .WillOnce(Return(MakeUnexpected(ComErrc::kInvalidBindingInformation)));

    // Given a skeleton created based on a Lola binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // When the initial value is set via an Update call
    const auto update_result = unit.my_dummy_field_.Update(initial_value);

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
TEST(SkeletonFieldAllocateTest, CallingAllocateBeforePrepareOfferDoesNotReturnValidSlot)
{
    RuntimeMockGuard runtime_mock_guard{};
    ON_CALL(runtime_mock_guard.runtime_mock_, GetTracingFilterConfig()).WillByDefault(Return(nullptr));

    SkeletonFieldBindingFactoryMockGuard<TestSampleType> skeleton_field_binding_factory_mock_guard{};

    // Expecting that a SkeletonField binding is created
    auto skeleton_field_binding_mock_ptr = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();
    auto& skeleton_field_binding_mock = *skeleton_field_binding_mock_ptr;
    EXPECT_CALL(skeleton_field_binding_factory_mock_guard.factory_mock_,
                CreateEventBinding(kInstanceIdWithLolaBinding, _, kFieldName))
        .WillOnce(Return(ByMove(std::move(skeleton_field_binding_mock_ptr))));

    // and that PrepareOffer() will be called on the event binding
    EXPECT_CALL(skeleton_field_binding_mock, PrepareOffer()).Times(0);

    // and Allocate will not be called on the event binding
    EXPECT_CALL(skeleton_field_binding_mock, Allocate()).Times(0);

    // Given a skeleton created based on a Lola binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // When allocating a slot before calling PrepareOffer
    const auto slot_result = unit.my_dummy_field_.Allocate();

    // Then an error will be returned indicating that the binding failed
    ASSERT_FALSE(slot_result.has_value());
    EXPECT_EQ(slot_result.error(), ComErrc::kBindingFailure);
}

TEST(SkeletonFieldAllocateTest, CallingAllocateAfterPrepareOfferDispatchesToBinding)
{
    RecordProperty("Verifies", "SCR-17434933, SCR-21470600");
    RecordProperty("Description", "Checks that calling allocate after prepare offer dispatches to the binding.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const TestSampleType initial_value{42};

    RuntimeMockGuard runtime_mock_guard{};
    ON_CALL(runtime_mock_guard.runtime_mock_, GetTracingFilterConfig()).WillByDefault(Return(nullptr));

    SkeletonFieldBindingFactoryMockGuard<TestSampleType> skeleton_field_binding_factory_mock_guard{};

    // Expecting that a SkeletonField binding is created
    auto skeleton_field_binding_mock_ptr = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();
    auto& skeleton_field_binding_mock = *skeleton_field_binding_mock_ptr;
    EXPECT_CALL(skeleton_field_binding_factory_mock_guard.factory_mock_,
                CreateEventBinding(kInstanceIdWithLolaBinding, _, kFieldName))
        .WillOnce(Return(ByMove(std::move(skeleton_field_binding_mock_ptr))));

    // and that PrepareOffer() will be called on the event binding
    EXPECT_CALL(skeleton_field_binding_mock, PrepareOffer()).WillOnce(Return(score::ResultBlank{}));

    // and Send will be called on the event binding with the initial value
    EXPECT_CALL(skeleton_field_binding_mock, Send(initial_value, _));

    // and Allocate will be called again which returns a valid SampleAllocateePtr
    EXPECT_CALL(skeleton_field_binding_mock, Allocate())
        .WillOnce(Return(ByMove(MakeSampleAllocateePtr(std::make_unique<TestSampleType>()))));

    // Given a skeleton created based on a Lola binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // When the initial value is set via an Update call
    const auto update_result = unit.my_dummy_field_.Update(initial_value);

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

TEST(SkeletonFieldAllocateTest, CallingAllocateAfterPrepareOfferFailsWhenBindingReturnsError)
{
    RecordProperty("Verifies", "SCR-17434933");
    RecordProperty("Description",
                   "Checks that calling allocate after prepare offer propagates an error from the binding.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const TestSampleType initial_value{42};

    RuntimeMockGuard runtime_mock_guard{};
    ON_CALL(runtime_mock_guard.runtime_mock_, GetTracingFilterConfig()).WillByDefault(Return(nullptr));

    SkeletonFieldBindingFactoryMockGuard<TestSampleType> skeleton_field_binding_factory_mock_guard{};

    // Expecting that a SkeletonField binding is created
    auto skeleton_field_binding_mock_ptr = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();
    auto& skeleton_field_binding_mock = *skeleton_field_binding_mock_ptr;
    EXPECT_CALL(skeleton_field_binding_factory_mock_guard.factory_mock_,
                CreateEventBinding(kInstanceIdWithLolaBinding, _, kFieldName))
        .WillOnce(Return(ByMove(std::move(skeleton_field_binding_mock_ptr))));

    // and that PrepareOffer() will be called on the event binding
    EXPECT_CALL(skeleton_field_binding_mock, PrepareOffer()).WillOnce(Return(score::ResultBlank{}));

    // and Send will be called on the event binding with the initial value
    EXPECT_CALL(skeleton_field_binding_mock, Send(initial_value, _));

    // and Allocate will be called again which returns a nullptr
    EXPECT_CALL(skeleton_field_binding_mock, Allocate())
        .WillOnce(Return(ByMove(MakeUnexpected(ComErrc::kInvalidConfiguration))));

    // Given a skeleton created based on a Lola binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // When the initial value is set via an Update call
    const auto update_result = unit.my_dummy_field_.Update(initial_value);

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

TEST(SkeletonFieldZeroCopyUpdateTest, CallingZeroCopyUpdateAfterOfferServiceDispatchesToBinding)
{
    RecordProperty("Verifies", "SCR-17434778, SCR-21553623");
    RecordProperty("Description",
                   "Checks that calling zero copy Update after offer service dispatches to the binding.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const TestSampleType initial_value{42};
    const TestSampleType new_value{52};

    RuntimeMockGuard runtime_mock_guard{};
    ON_CALL(runtime_mock_guard.runtime_mock_, GetTracingFilterConfig()).WillByDefault(Return(nullptr));

    SkeletonFieldBindingFactoryMockGuard<TestSampleType> skeleton_field_binding_factory_mock_guard{};

    // Expecting that a SkeletonField binding is created
    auto skeleton_field_binding_mock_ptr = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();
    auto& skeleton_field_binding_mock = *skeleton_field_binding_mock_ptr;
    EXPECT_CALL(skeleton_field_binding_factory_mock_guard.factory_mock_,
                CreateEventBinding(kInstanceIdWithLolaBinding, _, kFieldName))
        .WillOnce(Return(ByMove(std::move(skeleton_field_binding_mock_ptr))));

    // and that PrepareOffer() will be called on the event binding
    EXPECT_CALL(skeleton_field_binding_mock, PrepareOffer()).WillOnce(Return(score::ResultBlank{}));

    // and Send will be called on the event binding with the initial value
    EXPECT_CALL(skeleton_field_binding_mock, Send(initial_value, _));

    // and Allocate will be called again which returns a valid SampleAllocateePtr
    EXPECT_CALL(skeleton_field_binding_mock, Allocate())
        .WillOnce(Return(ByMove(MakeSampleAllocateePtr(std::make_unique<TestSampleType>()))));

    // and Send will be called a second time on the event binding with a new value which returns an empty result
    EXPECT_CALL(skeleton_field_binding_mock, Send(An<SampleAllocateePtr<TestSampleType>>(), _))
        .WillOnce(WithArg<0>(Invoke([new_value](SampleAllocateePtr<TestSampleType> sample_ptr) -> ResultBlank {
            EXPECT_EQ(*sample_ptr, new_value);
            return {};
        })));

    // Given a skeleton created based on a Lola binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // When the initial value is set via an Update call
    const auto update_result = unit.my_dummy_field_.Update(initial_value);

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

TEST(SkeletonFieldZeroCopyUpdateTest, CallingZeroCopyUpdateAfterOfferServicePropagatesBindingFail)
{
    RecordProperty("Verifies", "SCR-17434778");
    RecordProperty(
        "Description",
        "Checks that calling zero copy Update after offer service returns kBindingFailure for a generic error code "
        "from the binding.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const TestSampleType initial_value{42};
    const TestSampleType new_value{52};

    RuntimeMockGuard runtime_mock_guard{};
    ON_CALL(runtime_mock_guard.runtime_mock_, GetTracingFilterConfig()).WillByDefault(Return(nullptr));

    SkeletonFieldBindingFactoryMockGuard<TestSampleType> skeleton_field_binding_factory_mock_guard{};

    // Expecting that a SkeletonField binding is created
    auto skeleton_field_binding_mock_ptr = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();
    auto& skeleton_field_binding_mock = *skeleton_field_binding_mock_ptr;
    EXPECT_CALL(skeleton_field_binding_factory_mock_guard.factory_mock_,
                CreateEventBinding(kInstanceIdWithLolaBinding, _, kFieldName))
        .WillOnce(Return(ByMove(std::move(skeleton_field_binding_mock_ptr))));

    // and that PrepareOffer() will be called on the event binding
    EXPECT_CALL(skeleton_field_binding_mock, PrepareOffer()).WillOnce(Return(score::ResultBlank{}));

    // and Send will be called on the event binding with the initial value
    EXPECT_CALL(skeleton_field_binding_mock, Send(initial_value, _));

    // and Allocate will be called again which returns a valid SampleAllocateePtr
    EXPECT_CALL(skeleton_field_binding_mock, Allocate())
        .WillOnce(Return(ByMove(MakeSampleAllocateePtr(std::make_unique<TestSampleType>()))));

    // and Send will be called a second time on the event binding with a new value which returns an error
    EXPECT_CALL(skeleton_field_binding_mock, Send(An<SampleAllocateePtr<TestSampleType>>(), _))
        .WillOnce(WithArg<0>(Invoke([new_value](SampleAllocateePtr<TestSampleType> sample_ptr) -> ResultBlank {
            EXPECT_EQ(*sample_ptr, new_value);
            return MakeUnexpected(ComErrc::kInvalidBindingInformation);
        })));

    // Given a skeleton created based on a Lola binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // When the initial value is set via an Update call
    const auto update_result = unit.my_dummy_field_.Update(initial_value);

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

TEST(SkeletonFieldInitialValueFixture, LatestFieldValueWillBeSetOnPrepareOffer)
{
    RecordProperty("Verifies", "SCR-22129134");
    RecordProperty(
        "Description",
        "Checks that the initial value of the field is the value of the last Update call before calling PrepareOffer.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const TestSampleType initial_value{42};
    const TestSampleType latest_value{43};

    RuntimeMockGuard runtime_mock_guard{};
    ON_CALL(runtime_mock_guard.runtime_mock_, GetTracingFilterConfig()).WillByDefault(Return(nullptr));

    SkeletonFieldBindingFactoryMockGuard<TestSampleType> skeleton_field_binding_factory_mock_guard{};

    // Expecting that a SkeletonField binding is created
    auto skeleton_field_binding_mock_ptr = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();
    auto& skeleton_field_binding_mock = *skeleton_field_binding_mock_ptr;
    EXPECT_CALL(skeleton_field_binding_factory_mock_guard.factory_mock_,
                CreateEventBinding(kInstanceIdWithLolaBinding, _, kFieldName))
        .WillOnce(Return(ByMove(std::move(skeleton_field_binding_mock_ptr))));

    // and that PrepareOffer() will be called on the event binding
    EXPECT_CALL(skeleton_field_binding_mock, PrepareOffer()).WillOnce(Return(score::ResultBlank{}));

    // and Send will be called only once on the event binding with the latest value
    EXPECT_CALL(skeleton_field_binding_mock, Send(latest_value, _));

    // Given a skeleton created based on a Lola binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // When the initial value is set via an Update call
    const auto update_result = unit.my_dummy_field_.Update(initial_value);

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

TEST(SkeletonFieldInitialValueFixture, OfferingFieldBeforeUpdatingValueReturnsError)
{
    RecordProperty("Verifies", "SCR-17563743");
    RecordProperty("Description", "Calling OfferService before setting the field value returns kFieldValueIsNotValid.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    RuntimeMockGuard runtime_mock_guard{};
    ON_CALL(runtime_mock_guard.runtime_mock_, GetTracingFilterConfig()).WillByDefault(Return(nullptr));

    SkeletonFieldBindingFactoryMockGuard<TestSampleType> skeleton_field_binding_factory_mock_guard{};

    // Expecting that a SkeletonField binding is created
    auto skeleton_field_binding_mock_ptr = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();
    auto& skeleton_field_binding_mock = *skeleton_field_binding_mock_ptr;
    EXPECT_CALL(skeleton_field_binding_factory_mock_guard.factory_mock_,
                CreateEventBinding(kInstanceIdWithLolaBinding, _, kFieldName))
        .WillOnce(Return(ByMove(std::move(skeleton_field_binding_mock_ptr))));

    // and that PrepareOffer() will not be called on the event binding
    EXPECT_CALL(skeleton_field_binding_mock, PrepareOffer()).Times(0);

    // Given a skeleton created based on a Lola binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // When the initial field value has not been set

    // when PrepareOffer() is called on the field
    const auto result = unit.my_dummy_field_.PrepareOffer();

    // and the PrepareOffer() call should return an error message.
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kFieldValueIsNotValid);
}

TEST(SkeletonFieldInitialValueFixture, MoveConstructingFieldBeforePrepareOfferWillKeepInitialValue)
{
    const TestSampleType initial_value{42};

    RuntimeMockGuard runtime_mock_guard{};
    ON_CALL(runtime_mock_guard.runtime_mock_, GetTracingFilterConfig()).WillByDefault(Return(nullptr));

    SkeletonFieldBindingFactoryMockGuard<TestSampleType> skeleton_field_binding_factory_mock_guard{};

    // Expecting that a SkeletonField binding is created
    auto skeleton_field_binding_mock_ptr = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();
    auto& skeleton_field_binding_mock = *skeleton_field_binding_mock_ptr;
    EXPECT_CALL(skeleton_field_binding_factory_mock_guard.factory_mock_,
                CreateEventBinding(kInstanceIdWithLolaBinding, _, kFieldName))
        .WillOnce(Return(ByMove(std::move(skeleton_field_binding_mock_ptr))));

    // and that PrepareOffer() will be called on the event binding
    EXPECT_CALL(skeleton_field_binding_mock, PrepareOffer()).WillOnce(Return(score::ResultBlank{}));

    // and Send will be called on the event binding with the initial value
    EXPECT_CALL(skeleton_field_binding_mock, Send(initial_value, _));

    // Given a skeleton created based on a Lola binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // When the initial value is set via an Update call
    const auto update_result = unit.my_dummy_field_.Update(initial_value);

    // which does not return an error
    EXPECT_TRUE(update_result.has_value());

    // and then move constructing a new skeleton
    MyDummySkeleton unit_2{std::move(unit)};

    // and PrepareOffer() is called on the field in the new skeleton
    const auto prepare_offer_result = unit_2.my_dummy_field_.PrepareOffer();

    // then PrepareOffer() does not return an error
    EXPECT_TRUE(prepare_offer_result.has_value());
}

TEST(SkeletonFieldInitialValueFixture, MoveAssigningFieldBeforePrepareOfferWillKeepInitialValue)
{
    const TestSampleType initial_value{42};
    const TestSampleType initial_value_2{43};

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
    EXPECT_CALL(skeleton_field_binding_mock, PrepareOffer()).WillOnce(Return(score::ResultBlank{}));

    // and Send will be called on the event binding with the initial value from the moved-from field
    EXPECT_CALL(skeleton_field_binding_mock, Send(initial_value, _));

    // Given a skeleton created based on a Lola binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // When the initial value is set via an Update call
    const auto update_result = unit.my_dummy_field_.Update(initial_value);

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
    const auto update_result_2 = unit_2.my_dummy_field_.Update(initial_value_2);

    // which does not return an error
    EXPECT_TRUE(update_result_2.has_value());

    unit_2 = std::move(unit);

    // and PrepareOffer() is called on the field in the new skeleton
    const auto prepare_offer_result = unit_2.my_dummy_field_.PrepareOffer();

    // then PrepareOffer() does not return an error
    EXPECT_TRUE(prepare_offer_result.has_value());
}

TEST(SkeletonFieldTest, SkeletonFieldsRegisterThemselvesWithSkeleton)
{
    RuntimeMockGuard runtime_mock_guard{};
    ON_CALL(runtime_mock_guard.runtime_mock_, GetTracingFilterConfig()).WillByDefault(Return(nullptr));

    // Expecting that the SkeletonFieldBindingFactory returns a valid binding
    SkeletonFieldBindingFactoryMockGuard<TestSampleType> skeleton_field_binding_factory_mock_guard{};
    EXPECT_CALL(skeleton_field_binding_factory_mock_guard.factory_mock_,
                CreateEventBinding(kInstanceIdWithLolaBinding, _, kFieldName))
        .WillOnce(Return(ByMove(std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>())));

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

TEST(SkeletonFieldTest, MovingConstructingSkeletonUpdatesFieldMapReference)
{
    RuntimeMockGuard runtime_mock_guard{};
    ON_CALL(runtime_mock_guard.runtime_mock_, GetTracingFilterConfig()).WillByDefault(Return(nullptr));

    // Expecting that the SkeletonFieldBindingFactory returns a valid binding
    SkeletonFieldBindingFactoryMockGuard<TestSampleType> skeleton_field_binding_factory_mock_guard{};
    EXPECT_CALL(skeleton_field_binding_factory_mock_guard.factory_mock_,
                CreateEventBinding(kInstanceIdWithLolaBinding, _, kFieldName))
        .WillOnce(Return(ByMove(std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>())));

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

TEST(SkeletonFieldTest, MovingAssigningSkeletonUpdatesFieldMapReference)
{
    RuntimeMockGuard runtime_mock_guard{};
    ON_CALL(runtime_mock_guard.runtime_mock_, GetTracingFilterConfig()).WillByDefault(Return(nullptr));

    ServiceIdentifierType service{make_ServiceIdentifierType("foo2", 1U, 0U)};
    const ServiceInstanceDeployment instance_deployment{
        service,
        LolaServiceInstanceDeployment{LolaServiceInstanceId{kInstanceId}},
        QualityType::kASIL_QM,
        kInstanceSpecifier};
    InstanceIdentifier identifier2{make_InstanceIdentifier(instance_deployment, kTypeDeployment)};

    // Expecting that the SkeletonFieldBindingFactory returns a valid binding  for both Skeletons
    SkeletonFieldBindingFactoryMockGuard<TestSampleType> skeleton_field_binding_factory_mock_guard{};
    EXPECT_CALL(skeleton_field_binding_factory_mock_guard.factory_mock_,
                CreateEventBinding(kInstanceIdWithLolaBinding, _, kFieldName))
        .WillOnce(Return(ByMove(std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>())));
    EXPECT_CALL(skeleton_field_binding_factory_mock_guard.factory_mock_, CreateEventBinding(identifier2, _, kFieldName))
        .WillOnce(Return(ByMove(std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>())));

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

TEST(SkeletonFieldDeathTest, UpdateWithInvalidFieldNameTriggersTermination)
{
    RuntimeMockGuard runtime_mock_guard{};
    ON_CALL(runtime_mock_guard.runtime_mock_, GetTracingFilterConfig()).WillByDefault(Return(nullptr));

    // Expecting that the SkeletonFieldBindingFactory returns a valid binding
    SkeletonFieldBindingFactoryMockGuard<TestSampleType> skeleton_field_binding_factory_mock_guard{};
    EXPECT_CALL(skeleton_field_binding_factory_mock_guard.factory_mock_,
                CreateEventBinding(kInstanceIdWithLolaBinding, _, kFieldName))
        .WillOnce(Return(ByMove(std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>())));

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

}  // namespace
}  // namespace score::mw::com::impl
