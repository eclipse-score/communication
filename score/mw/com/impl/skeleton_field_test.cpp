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
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/method_type.h"
#include "score/mw/com/impl/methods/skeleton_method.h"
#include "score/mw/com/impl/methods/skeleton_method_binding.h"
#include "score/mw/com/impl/sample_allocatee_guard.h"
#include "score/mw/com/impl/test/binding_factory_resources.h"
#include "score/mw/com/impl/test/runtime_mock_guard.h"

#include "score/result/result.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

namespace score::mw::com::impl
{
namespace
{

using TestSampleType = std::uint32_t;

// SFINAE helper to check if RegisterSetHandler is available with "WithSetter" tag.
template <typename, typename = void>
struct has_register_set_handler : std::false_type
{
};
template <typename T>
struct has_register_set_handler<T,
                                std::void_t<decltype(std::declval<T&>().RegisterSetHandler(
                                    std::declval<score::cpp::callback<void(typename T::FieldType&)>>()))>>
    : std::true_type
{
};

using SkeletonEventTracingData = tracing::SkeletonEventTracingData;

using ::testing::_;
using ::testing::An;
using ::testing::ByMove;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::WithArg;

constexpr std::string_view kFieldName{"Field1"};
const TestSampleType kDummyInitialValue{42};
const TestSampleType kDummySetValue{43};

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

    // WithSetter is intentionally absent here since setter-related behaviour is tested via MySetterSkeleton below.
    SkeletonField<TestSampleType, WithGetter, WithNotifier> my_dummy_field_{*this, kFieldName};
};

class SkeletonFieldTestFixture : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        ON_CALL(skeleton_field_binding_factory_mock_guard_.factory_mock_,
                CreateEventBinding(kInstanceIdWithLolaBinding, _, kFieldName, _))
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

    RuntimeMockGuard runtime_mock_guard_{};

    SkeletonFieldBindingFactoryMockGuard<TestSampleType> skeleton_field_binding_factory_mock_guard_{};
    SkeletonMethodBindingFactoryMockGuard skeleton_method_binding_factory_mock_guard_{};

    mock_binding::SkeletonEvent<TestSampleType> skeleton_field_binding_mock_{};
    mock_binding::SkeletonMethod skeleton_field_get_binding_mock_{};
    mock_binding::SkeletonMethod skeleton_field_set_binding_mock_{};
};

TEST(SkeletonFieldTest, NotCopyable)
{
    RecordProperty("Verifies", "SCR-18221574");
    RecordProperty("Description", "Checks copy constructors for SkeletonField");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(
        !std::is_copy_constructible<SkeletonField<TestSampleType, WithGetter, WithNotifier, WithSetter>>::value,
        "Is wrongly copyable");
    static_assert(!std::is_copy_assignable<SkeletonField<TestSampleType, WithGetter, WithNotifier, WithSetter>>::value,
                  "Is wrongly copyable");
}

TEST(SkeletonFieldTest, IsMoveable)
{
    static_assert(
        std::is_move_constructible<SkeletonField<TestSampleType, WithGetter, WithNotifier, WithSetter>>::value,
        "Is not move constructible");
    static_assert(std::is_move_assignable<SkeletonField<TestSampleType, WithGetter, WithNotifier, WithSetter>>::value,
                  "Is not move assignable");
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

    static_assert(std::is_same<typename SkeletonField<TestSampleType, WithGetter, WithNotifier, WithSetter>::FieldType,
                               TestSampleType>::value,
                  "Incorrect FieldType.");
}

using SkeletonFieldCreationFixture = SkeletonFieldTestFixture;
TEST_F(SkeletonFieldCreationFixture, CreatingFieldWithNotifierCallsFactoryWithNotifier)
{
    // Expect that the factory is called with WithNotifier
    EXPECT_CALL(skeleton_field_binding_factory_mock_guard_.factory_mock_,
                CreateEventBinding(kInstanceIdWithLolaBinding, _, kFieldName, FieldTagsStore::Create<WithNotifier>()));

    // When creating a SkeletonField with WithNotifier
    SkeletonBase skeleton{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};
    SkeletonField<TestSampleType, WithNotifier> my_dummy_field{skeleton, kFieldName};
}

TEST_F(SkeletonFieldCreationFixture, CreatingFieldWithGetterCallsFactoryWithGetter)
{
    // Expect that the factory is called with WithGetter
    EXPECT_CALL(skeleton_field_binding_factory_mock_guard_.factory_mock_,
                CreateEventBinding(kInstanceIdWithLolaBinding, _, kFieldName, FieldTagsStore::Create<WithGetter>()));

    // When creating a SkeletonField with WithGetter
    SkeletonBase skeleton{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};
    SkeletonField<TestSampleType, WithGetter> my_dummy_field{skeleton, kFieldName};
}

TEST_F(SkeletonFieldCreationFixture, CreatingFieldWithNotifierAndSetterCallsFactoryWithNotifierAndSetter)
{
    // Expect that the factory is called with WithNotifier and WithSetter
    EXPECT_CALL(skeleton_field_binding_factory_mock_guard_.factory_mock_,
                CreateEventBinding(
                    kInstanceIdWithLolaBinding, _, kFieldName, FieldTagsStore::Create<WithNotifier, WithSetter>()));

    // When creating a SkeletonField with WithNotifier and WithSetter
    SkeletonBase skeleton{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};
    SkeletonField<TestSampleType, WithNotifier, WithSetter> my_dummy_field{skeleton, kFieldName};
}

TEST_F(SkeletonFieldCreationFixture, CreatingFieldWithGetterAndSetterCallsFactoryWithGetterAndSetter)
{
    // Expect that the factory is called with WithGetter and WithSetter
    EXPECT_CALL(skeleton_field_binding_factory_mock_guard_.factory_mock_,
                CreateEventBinding(
                    kInstanceIdWithLolaBinding, _, kFieldName, FieldTagsStore::Create<WithGetter, WithSetter>()));

    // When creating a SkeletonField with WithGetter and WithSetter
    SkeletonBase skeleton{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};
    SkeletonField<TestSampleType, WithGetter, WithSetter> my_dummy_field{skeleton, kFieldName};
}

TEST_F(SkeletonFieldCreationFixture, CreatingFieldWithNotifierAndGetterCallsFactoryWithNotifierAndGetter)
{
    // Expect that the factory is called with WithNotifier and WithGetter
    EXPECT_CALL(skeleton_field_binding_factory_mock_guard_.factory_mock_,
                CreateEventBinding(
                    kInstanceIdWithLolaBinding, _, kFieldName, FieldTagsStore::Create<WithNotifier, WithGetter>()));

    // When creating a SkeletonField with WithNotifier and WithGetter
    SkeletonBase skeleton{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};
    SkeletonField<TestSampleType, WithNotifier, WithGetter> my_dummy_field{skeleton, kFieldName};
}

TEST_F(SkeletonFieldCreationFixture,
       CreatingFieldWithNotifierAndGetterAndSetterCallsFactoryWithNotifierAndGetterAndSetter)
{
    // Expect that the factory is called with WithNotifier, WithGetter and WithSetter
    EXPECT_CALL(
        skeleton_field_binding_factory_mock_guard_.factory_mock_,
        CreateEventBinding(
            kInstanceIdWithLolaBinding, _, kFieldName, FieldTagsStore::Create<WithNotifier, WithGetter, WithSetter>()));

    // When creating a SkeletonField with WithNotifier, WithGetter and WithSetter
    SkeletonBase skeleton{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};
    SkeletonField<TestSampleType, WithNotifier, WithGetter, WithSetter> my_dummy_field{skeleton, kFieldName};
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
    EXPECT_CALL(skeleton_field_binding_mock_, Send(kDummyInitialValue, _, _))
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
    EXPECT_CALL(skeleton_field_binding_mock_, Send(kDummyInitialValue, _, _))
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
    EXPECT_CALL(skeleton_field_binding_mock_, Send(kDummyInitialValue, _, _)).WillOnce(Return(score::Result<void>{}));

    // and Send will be called a second time on the event binding with the updated value and returns an empty result
    EXPECT_CALL(skeleton_field_binding_mock_, Send(updated_value, _, _)).WillOnce(Return(score::Result<void>{}));

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
    EXPECT_CALL(skeleton_field_binding_mock_, Send(kDummyInitialValue, _, _)).WillOnce(Return(score::Result<void>{}));

    // and Send will be called a second time on the event binding with the updated value and returns an error
    EXPECT_CALL(skeleton_field_binding_mock_, Send(updated_value, _, _))
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
    EXPECT_CALL(skeleton_field_binding_mock_, Allocate(_)).Times(0);

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
    EXPECT_CALL(skeleton_field_binding_mock_, Send(kDummyInitialValue, _, _));

    // and Allocate will be called again which returns a valid SampleAllocateePtr
    EXPECT_CALL(skeleton_field_binding_mock_, Allocate(_))
        .WillOnce(Return(ByMove(MakeSampleAllocateePtr(std::make_unique<TestSampleType>(), SampleAllocateeGuard{}))));

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
    EXPECT_CALL(skeleton_field_binding_mock_, Send(kDummyInitialValue, _, _));

    // and Allocate will be called again which returns a nullptr
    EXPECT_CALL(skeleton_field_binding_mock_, Allocate(_))
        .WillOnce([](SampleAllocateeGuard) -> Result<SampleAllocateePtr<TestSampleType>> {
            return MakeUnexpected(ComErrc::kInvalidConfiguration);
        });

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
    EXPECT_CALL(skeleton_field_binding_mock_, Send(kDummyInitialValue, _, _));

    // and Allocate will be called again which returns a valid SampleAllocateePtr
    EXPECT_CALL(skeleton_field_binding_mock_, Allocate(_))
        .WillOnce(Return(ByMove(MakeSampleAllocateePtr(std::make_unique<TestSampleType>(), SampleAllocateeGuard{}))));

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
    EXPECT_CALL(skeleton_field_binding_mock_, Send(kDummyInitialValue, _, _));

    // and Allocate will be called again which returns a valid SampleAllocateePtr
    EXPECT_CALL(skeleton_field_binding_mock_, Allocate(_))
        .WillOnce(Return(ByMove(MakeSampleAllocateePtr(std::make_unique<TestSampleType>(), SampleAllocateeGuard{}))));

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
    EXPECT_CALL(skeleton_field_binding_mock_, Send(latest_value, _, _));

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
    EXPECT_CALL(skeleton_field_binding_mock_, Send(kDummyInitialValue, _, _));

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
    SkeletonMethodBindingFactoryMockGuard skeleton_method_binding_factory_mock_guard{};

    mock_binding::SkeletonMethod skeleton_method_mock{};
    ON_CALL(skeleton_method_binding_factory_mock_guard.factory_mock_, Create(_, _, _, _))
        .WillByDefault(InvokeWithoutArgs([&skeleton_method_mock]() {
            return std::make_unique<mock_binding::SkeletonMethodFacade>(skeleton_method_mock);
        }));

    // Expecting that a SkeletonField binding is created
    auto skeleton_field_binding_mock_ptr = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();
    auto& skeleton_field_binding_mock = *skeleton_field_binding_mock_ptr;
    EXPECT_CALL(skeleton_field_binding_factory_mock_guard.factory_mock_,
                CreateEventBinding(kInstanceIdWithLolaBinding, _, kFieldName, _))
        .WillOnce(Return(ByMove(std::move(skeleton_field_binding_mock_ptr))));

    EXPECT_CALL(skeleton_field_binding_mock, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // and that PrepareOffer() will be called on the event binding
    EXPECT_CALL(skeleton_field_binding_mock, PrepareOffer()).WillOnce(Return(score::Result<void>{}));

    // and Send will be called on the event binding with the initial value from the moved-from field
    EXPECT_CALL(skeleton_field_binding_mock, Send(kDummyInitialValue, _, _));

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
    EXPECT_CALL(skeleton_field_binding_factory_mock_guard.factory_mock_,
                CreateEventBinding(identifier2, _, kFieldName, _))
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
    const auto& field = fields.begin()->second.get().Get();

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
    const auto& field = fields.begin()->second.get().Get();

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
    const auto& field = fields.begin()->second.get().Get();

    // the name corresponds to the correct field name
    EXPECT_EQ(field_name, kFieldName);

    // and the field in the map corresponds to the correct skeleton field address
    EXPECT_EQ(&field, &unit2.my_dummy_field_);
}

using SkeletonFieldDeathTest = SkeletonFieldTestFixture;
TEST_F(SkeletonFieldDeathTest, DestroyingSkeletonFieldWhileHoldingSampleAllocateePtrTerminates)
{
    const TestSampleType initial_value{42};

    RuntimeMockGuard runtime_mock_guard{};
    ON_CALL(runtime_mock_guard.runtime_mock_, GetTracingFilterConfig()).WillByDefault(Return(nullptr));

    SkeletonFieldBindingFactoryMockGuard<TestSampleType> skeleton_field_binding_factory_mock_guard{};

    // Expecting that a SkeletonField binding is created
    auto skeleton_field_binding_mock_ptr = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();
    auto& skeleton_field_binding_mock = *skeleton_field_binding_mock_ptr;
    EXPECT_CALL(skeleton_field_binding_factory_mock_guard.factory_mock_,
                CreateEventBinding(kInstanceIdWithLolaBinding, _, kFieldName, _))
        .WillOnce(Return(ByMove(std::move(skeleton_field_binding_mock_ptr))));

    // and that PrepareOffer() is called once on the field binding
    EXPECT_CALL(skeleton_field_binding_mock, PrepareOffer()).WillOnce(Return(score::Result<void>{}));

    // and that Send() is called once on the field binding with the initial value
    EXPECT_CALL(skeleton_field_binding_mock, Send(initial_value, _, _));

    // and that Allocate() is called once on the field binding, returning a ptr backed by a real tracker guard
    EXPECT_CALL(skeleton_field_binding_mock, Allocate(_)).WillOnce([](SampleAllocateeGuard guard) {
        return MakeSampleAllocateePtr(std::make_unique<TestSampleType>(), std::move(guard));
    });

    // Given a skeleton which has a mock skeleton-binding
    auto unit =
        std::make_unique<MyDummySkeleton>(std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding);

    // and the initial field value has been set and PrepareOffer() has been called
    std::ignore = unit->my_dummy_field_.Update(initial_value);
    std::ignore = unit->my_dummy_field_.PrepareOffer();

    // and a slot has been allocated and is still held
    auto allocated_slot_result = unit->my_dummy_field_.Allocate();
    ASSERT_TRUE(allocated_slot_result.has_value());

    // When the SkeletonField is destroyed while the SampleAllocateePtr is still held
    // Then the application terminates
    EXPECT_DEATH(unit.reset(), "");
}

// Helper skeleton that holds an EnableSet=true field (setter-capable field)
class MySetterSkeleton : public SkeletonBase
{
  public:
    using SkeletonBase::SkeletonBase;

    SkeletonField<TestSampleType, WithGetter, WithSetter, WithNotifier> my_setter_field_{*this, kFieldName};
};

TEST(SkeletonFieldSetHandlerTypeTraitsTest, RegisterSetHandlerOnlyExistsWhenWithSetterTagPresent)
{
    RecordProperty("Description",
                   "RegisterSetHandler() shall only exist on SkeletonField specializations whose tag pack "
                   "contains WithSetter. Verify via SFINAE detection that it is absent without WithSetter.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    using SetterField = SkeletonField<TestSampleType, WithSetter, WithNotifier, WithGetter>;
    using NoSetterField = SkeletonField<TestSampleType, WithNotifier, WithGetter>;

    static_assert(has_register_set_handler<SetterField>::value,
                  "RegisterSetHandler must exist on a SkeletonField with WithSetter in the tag pack");
    static_assert(!has_register_set_handler<NoSetterField>::value,
                  "RegisterSetHandler must be SFINAE-removed on a SkeletonField without WithSetter");
}

TEST(ProxyFieldNotifierGatingTest, RegisterSetHandlerDoesNotExistWhenNoTagPresent)
{
    using DefaultField = SkeletonField<TestSampleType>;

    static_assert(!has_register_set_handler<DefaultField>::value,
                  "RegisterSetHandler must be SFINAE-removed on a SkeletonField without WithSetter");
}

class SkeletonFieldSetHandlerTest : public SkeletonFieldTestFixture
{
  public:
    SkeletonFieldSetHandlerTest& GivenASkeletonWithSetterEnabled()
    {
        unit_ =
            std::make_unique<MySetterSkeleton>(std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding);

        return *this;
    }

    SkeletonFieldSetHandlerTest& WhichCapturesASetHandler()
    {
        std::optional<SkeletonMethodBinding::TypeErasedHandler> captured_set_handler{};
        EXPECT_CALL(skeleton_field_set_binding_mock_, RegisterHandler(_))
            .WillOnce(Invoke([&captured_set_handler_ = captured_set_handler_](auto handler) {
                captured_set_handler_ = std::move(handler);
                return Result<void>{};
            }));
        return *this;
    }

    SkeletonFieldSetHandlerTest& WhichIsOffered()
    {
        EXPECT_TRUE(unit_->my_setter_field_.Update(kDummyInitialValue).has_value());
        EXPECT_TRUE(unit_->my_setter_field_.PrepareOffer().has_value());
        return *this;
    }

    /// \brief Returns a span pointing to storage containing the provided field value
    std::pair<score::cpp::span<std::byte>, score::cpp::span<std::byte>> CreateFieldSetterInArgAndReturnSpans(
        const TestSampleType in_arg_value,
        const score::Result<TestSampleType> return_value)
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT(!in_arg_storage_.has_value());
        SCORE_LANGUAGE_FUTURECPP_ASSERT(!return_storage_.has_value());
        score::cpp::ignore = in_arg_storage_.emplace(in_arg_value);
        score::cpp::ignore = return_storage_.emplace(return_value);

        score::cpp::span<std::byte> in_span{reinterpret_cast<std::byte*>(&(in_arg_storage_.value())),
                                            sizeof(TestSampleType)};
        score::cpp::span<std::byte> out_span{reinterpret_cast<std::byte*>(&(return_storage_.value())),
                                             sizeof(score::Result<TestSampleType>)};

        return {in_span, out_span};
    }

    std::unique_ptr<MySetterSkeleton> unit_{};
    std::optional<SkeletonMethodBinding::TypeErasedHandler> captured_set_handler_{};

    std::optional<TestSampleType> in_arg_storage_{};
    std::optional<score::Result<TestSampleType>> return_storage_{};
};

TEST_F(SkeletonFieldSetHandlerTest, RegisterSetHandlerForwardsToMethodBinding)
{
    // Expecting that RegisterHandler is called on the field set method binding which returns success
    EXPECT_CALL(skeleton_field_set_binding_mock_, RegisterHandler(_)).WillOnce(Return(Result<void>{}));

    GivenASkeletonWithSetterEnabled();

    // When RegisterSetHandler is called with a valid (no-op) handler
    const auto result = unit_->my_setter_field_.RegisterSetHandler([](TestSampleType& /*value*/) noexcept {});

    // Then the registration succeeds
    EXPECT_TRUE(result.has_value());
}

TEST_F(SkeletonFieldSetHandlerTest, RegisterSetHandlerPropagatesBindingError)
{
    // Expecting that RegisterHandler is called on the field set method binding which returns an error
    EXPECT_CALL(skeleton_field_set_binding_mock_, RegisterHandler(_))
        .WillOnce(Return(MakeUnexpected(ComErrc::kCommunicationLinkError)));

    GivenASkeletonWithSetterEnabled();

    // When RegisterSetHandler is called
    const auto result = unit_->my_setter_field_.RegisterSetHandler([](TestSampleType& /*value*/) noexcept {});

    // Then the error from the binding is propagated
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kCommunicationLinkError);
}

TEST_F(SkeletonFieldSetHandlerTest, PrepareOfferFailsWhenSetHandlerNotRegistered)
{
    GivenASkeletonWithSetterEnabled();

    // and given an initial value was set so that the initial-value check does not fail
    EXPECT_TRUE(unit_->my_setter_field_.Update(TestSampleType{42}).has_value());

    // When PrepareOffer is called without having called RegisterSetHandler
    const auto result = unit_->my_setter_field_.PrepareOffer();

    // Then it returns kSetHandlerNotSet
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kSetHandlerNotSet);
}

TEST_F(SkeletonFieldSetHandlerTest, PrepareOfferSucceedsAfterRegisterSetHandler)
{
    const TestSampleType kSetHandlerInitialValue{7U};

    GivenASkeletonWithSetterEnabled();

    // Register a valid (no-op) set handler
    ASSERT_TRUE(unit_->my_setter_field_.RegisterSetHandler([](TestSampleType& /*value*/) noexcept {}).has_value());

    // Set the initial field value
    ASSERT_TRUE(unit_->my_setter_field_.Update(kSetHandlerInitialValue).has_value());

    // When PrepareOffer is called
    const auto result = unit_->my_setter_field_.PrepareOffer();

    // Then it succeeds
    EXPECT_TRUE(result.has_value());
}

TEST_F(SkeletonFieldSetHandlerTest, PrepareOfferSucceedsWithoutHandlerWhenWithSetterTagIsAbsent)
{
    // Given a skeleton containing a field without a setter enabled
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    ASSERT_TRUE(unit.my_dummy_field_.Update(kDummyInitialValue).has_value());

    // When PrepareOffer is called without registering a set handler
    const auto result = unit.my_dummy_field_.PrepareOffer();

    // Then it succeeds
    EXPECT_TRUE(result.has_value());
}

TEST_F(SkeletonFieldSetHandlerTest, RegisterSetHandlerAcceptsStdFunction)
{
    GivenASkeletonWithSetterEnabled();

    // When registering a set handler using std::function
    std::function<void(TestSampleType&)> std_function_handler = [](TestSampleType& /*value*/) noexcept {};
    const auto result = unit_->my_setter_field_.RegisterSetHandler(std_function_handler).has_value();

    // Then the registration succeeds
    EXPECT_TRUE(result);
}

TEST_F(SkeletonFieldSetHandlerTest, RegisterSetHandlerAcceptsCppCallback)
{
    GivenASkeletonWithSetterEnabled();

    // When registering a set handler using score::cpp::callback
    score::cpp::callback<void(TestSampleType&)> cpp_callback_handler = [](TestSampleType& /*value*/) noexcept {};
    const auto result = unit_->my_setter_field_.RegisterSetHandler(std::move(cpp_callback_handler)).has_value();

    // Then the registration succeeds
    EXPECT_TRUE(result);
}

TEST_F(SkeletonFieldSetHandlerTest, CallingMethodHandlerInvokesUserCallback)
{
    testing::MockFunction<void(TestSampleType&)> user_callback_mock{};

    // Expect that the user registered callback will be called with the value provided to the set handler
    TestSampleType expected_set_value{kDummySetValue};
    EXPECT_CALL(user_callback_mock, Call(expected_set_value));

    GivenASkeletonWithSetterEnabled().WhichCapturesASetHandler();

    // and given that a set handler was registered
    ASSERT_TRUE(unit_->my_setter_field_.RegisterSetHandler(user_callback_mock.AsStdFunction()).has_value());

    WhichIsOffered();

    // When calling the set handler that was captured by the method binding
    auto [in_span, out_span] =
        CreateFieldSetterInArgAndReturnSpans(kDummySetValue, score::Result<TestSampleType>{kDummySetValue});
    captured_set_handler_.value()(in_span, out_span);
}

TEST_F(SkeletonFieldSetHandlerTest, CallingMethodHandlerInvokesLatestRegisteredUserCallback)
{
    testing::MockFunction<void(TestSampleType&)> user_callback_mock{};
    testing::MockFunction<void(TestSampleType&)> user_callback_mock_2{};

    // Expect that only the second user registered callback will be called with the value provided to the set handler
    TestSampleType expected_set_value{kDummySetValue};
    EXPECT_CALL(user_callback_mock, Call(expected_set_value)).Times(0);
    EXPECT_CALL(user_callback_mock_2, Call(expected_set_value));

    GivenASkeletonWithSetterEnabled();

    // and given that RegisterHandler is called twice on the method binding
    std::optional<SkeletonMethodBinding::TypeErasedHandler> captured_set_handler{};
    EXPECT_CALL(skeleton_field_set_binding_mock_, RegisterHandler(_))
        .Times(2)
        .WillRepeatedly(Invoke([&captured_set_handler_ = captured_set_handler_](auto handler) {
            captured_set_handler_ = std::move(handler);
            return Result<void>{};
        }));

    // and given that a set handler was registered twice
    ASSERT_TRUE(unit_->my_setter_field_.RegisterSetHandler(user_callback_mock.AsStdFunction()).has_value());
    ASSERT_TRUE(unit_->my_setter_field_.RegisterSetHandler(user_callback_mock_2.AsStdFunction()).has_value());

    WhichIsOffered();

    // When calling the set handler that was captured by the method binding
    auto [in_span, out_span] =
        CreateFieldSetterInArgAndReturnSpans(kDummySetValue, score::Result<TestSampleType>{kDummySetValue});
    captured_set_handler_.value()(in_span, out_span);
}

TEST_F(SkeletonFieldSetHandlerTest, CallingMethodHandlerCallsSend)
{
    // Expect that Send will be called on the event binding twice: (1) when the initial value of the field is set. (2)
    // with the value provided to the set handler when the handler is called
    EXPECT_CALL(skeleton_field_binding_mock_, Send(kDummyInitialValue, _, _)).WillOnce(Return(Result<void>{}));
    EXPECT_CALL(skeleton_field_binding_mock_, Send(kDummySetValue, _, _)).WillOnce(Return(Result<void>{}));

    GivenASkeletonWithSetterEnabled().WhichCapturesASetHandler();

    // and given that a set handler was registered
    ASSERT_TRUE(unit_->my_setter_field_.RegisterSetHandler([](TestSampleType& /*value*/) {}).has_value());

    WhichIsOffered();

    // When calling the set handler that was captured by the method binding
    auto [in_span, out_span] =
        CreateFieldSetterInArgAndReturnSpans(kDummySetValue, score::Result<TestSampleType>{kDummySetValue});
    captured_set_handler_.value()(in_span, out_span);
}

TEST_F(SkeletonFieldSetHandlerTest, MethodHandlerDoesNotTerminateWhenSendFails)
{
    // Expect that Send will be called on the event binding twice: (1) when the initial value of the field is set. (2)
    // with the value provided to the set handler when the handler is called which returns an error.
    EXPECT_CALL(skeleton_field_binding_mock_, Send(kDummyInitialValue, _, _)).WillOnce(Return(Result<void>{}));
    EXPECT_CALL(skeleton_field_binding_mock_, Send(kDummySetValue, _, _))
        .WillOnce(Return(MakeUnexpected(ComErrc::kCommunicationLinkError)));

    GivenASkeletonWithSetterEnabled().WhichCapturesASetHandler();

    // and given that a set handler was registered
    ASSERT_TRUE(unit_->my_setter_field_.RegisterSetHandler([](TestSampleType& /*value*/) {}).has_value());

    WhichIsOffered();

    // When calling the set handler that was captured by the method binding
    auto [in_span, out_span] =
        CreateFieldSetterInArgAndReturnSpans(kDummySetValue, score::Result<TestSampleType>{kDummySetValue});
    captured_set_handler_.value()(in_span, out_span);

    // Then the out span contains an error indicating that the binding failed
    const auto& return_value = *reinterpret_cast<score::Result<TestSampleType>*>(out_span.data());
    ASSERT_FALSE(return_value.has_value());
    EXPECT_EQ(static_cast<ComErrc>(*return_value.error()), ComErrc::kBindingFailure);
}

TEST_F(SkeletonFieldSetHandlerTest, CallingMethodHandlerCallsSendWithValueModifiedByUserCallback)
{
    testing::MockFunction<void(TestSampleType&)> user_callback_mock{};

    // Given that the user registered callback will be called which modifies the value in-place
    TestSampleType expected_set_value{kDummySetValue};
    const TestSampleType modified_value{kDummySetValue * 2};
    EXPECT_CALL(user_callback_mock, Call(expected_set_value)).WillOnce(Invoke([](TestSampleType& value) noexcept {
        value = modified_value;
    }));

    // Expect that Send will be called on the event binding twice: (1) when the initial value of the field is set. (2)
    // with the value modified by the set handler when the handler is called
    EXPECT_CALL(skeleton_field_binding_mock_, Send(kDummyInitialValue, _, _)).WillOnce(Return(Result<void>{}));
    EXPECT_CALL(skeleton_field_binding_mock_, Send(modified_value, _, _)).WillOnce(Return(Result<void>{}));

    GivenASkeletonWithSetterEnabled().WhichCapturesASetHandler();

    // and given that a set handler was registered
    ASSERT_TRUE(unit_->my_setter_field_.RegisterSetHandler(user_callback_mock.AsStdFunction()).has_value());

    WhichIsOffered();

    // When calling the set handler that was captured by the method binding
    auto [in_span, out_span] =
        CreateFieldSetterInArgAndReturnSpans(kDummySetValue, score::Result<TestSampleType>{kDummySetValue});
    captured_set_handler_.value()(in_span, out_span);
}

TEST_F(SkeletonFieldSetHandlerTest, PassingReferenceToHandlerUpdatesStateInPlace)
{
    static constexpr int kInitialValue = 42;
    static constexpr int kModifiedValue = 43;

    GivenASkeletonWithSetterEnabled().WhichCapturesASetHandler();

    // and given that a set handler was registered which modifies its internal state when called
    class DummyMethodFunctor
    {
      public:
        void operator()(TestSampleType& /*value*/)
        {
            i_ = kModifiedValue;
        }

        int i_{kInitialValue};
    };
    DummyMethodFunctor test_functor{};
    ASSERT_TRUE(unit_->my_setter_field_.RegisterSetHandler(test_functor).has_value());

    WhichIsOffered();

    // When calling the set handler that was captured by the method binding
    auto [in_span, out_span] =
        CreateFieldSetterInArgAndReturnSpans(kDummySetValue, score::Result<TestSampleType>{kDummySetValue});
    captured_set_handler_.value()(in_span, out_span);

    // Then the state of the functor is updated in place when the handler is called by the binding
    EXPECT_EQ(test_functor.i_, kModifiedValue);
}

using SkeletonFieldMoveConstructionFixture = SkeletonFieldSetHandlerTest;
TEST_F(SkeletonFieldMoveConstructionFixture, SecondRegisterSetHandlerReplacesHandler)
{
    // Note. This test verifies that moving a skeleton does not break the getter / setter methods stored within a field.
    // The getter and setter methods are owned by the field via unique_ptr, so they remain valid across skeleton moves.
    // This test verifies usability after move construction.

    // Given a skeleton containing a field with a setter enabled
    MySetterSkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // When move constructing the skeleton
    MySetterSkeleton unit2{std::move(unit)};

    // Then the method should still be usable (validated by calling RegisterSetHandler which dispatches to the method)
    unit2.my_setter_field_.RegisterSetHandler([](TestSampleType& /*value*/) noexcept {});
}

TEST_F(SkeletonFieldMoveConstructionFixture,
       CallingMethodHandlerCallsSendWithValueModifiedByUserCallbackAfterMoveConstruction)
{
    testing::MockFunction<void(TestSampleType&)> user_callback_mock{};

    // Given that the user registered callback will be called which modifies the value in-place
    TestSampleType expected_set_value{kDummySetValue};
    const TestSampleType modified_value{kDummySetValue * 2};
    EXPECT_CALL(user_callback_mock, Call(expected_set_value)).WillOnce(Invoke([](TestSampleType& value) noexcept {
        value = modified_value;
    }));

    // Expect that Send will be called on the event binding twice: (1) when the initial value of the field is set. (2)
    // with the value modified by the set handler when the handler is called
    EXPECT_CALL(skeleton_field_binding_mock_, Send(kDummyInitialValue, _, _)).WillOnce(Return(Result<void>{}));
    EXPECT_CALL(skeleton_field_binding_mock_, Send(modified_value, _, _)).WillOnce(Return(Result<void>{}));

    GivenASkeletonWithSetterEnabled().WhichCapturesASetHandler();

    // and given that a set handler was registered
    ASSERT_TRUE(unit_->my_setter_field_.RegisterSetHandler(user_callback_mock.AsStdFunction()).has_value());

    WhichIsOffered();

    // and given that the skeleton containing the field is move constructed into a new skeleton
    MySetterSkeleton unit2{std::move(*unit_)};

    // When calling the set handler that was captured by the method binding
    auto [in_span, out_span] =
        CreateFieldSetterInArgAndReturnSpans(kDummySetValue, score::Result<TestSampleType>{kDummySetValue});
    captured_set_handler_.value()(in_span, out_span);
}

}  // namespace
}  // namespace score::mw::com::impl
