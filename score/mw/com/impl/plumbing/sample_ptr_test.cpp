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
#include "score/mw/com/impl/plumbing/sample_ptr.h"

#include <gtest/gtest.h>

#include <memory>
#include <type_traits>
#include <utility>

namespace score::mw::com::impl
{
namespace
{

using TestSampleType = std::uint8_t;

/// \brief Templated test fixture for SamplePtr functionality that works for both void and non-void types
template <typename T>
class SamplePtrGenericTypeTest : public ::testing::Test
{
  protected:
    SamplePtrGenericTypeTest()
        : mock_pointer_{new TestSampleType(42), [](T* p) noexcept {
                            auto* const int_p = static_cast<TestSampleType*>(p);
                            delete int_p;
                        }}
    {
    }

    mock_binding::SamplePtr<T> mock_pointer_;
};

// Gtest will run all tests in the SamplePtrGenericTypeTest once for every type, t, in MyTypes, such that TypeParam == t
// for each run.
using MyTypes = ::testing::Types<uint8_t, void>;
TYPED_TEST_SUITE(SamplePtrGenericTypeTest, MyTypes, );

TYPED_TEST(SamplePtrGenericTypeTest, CanBeDefaultConstructed)
{
    ::testing::Test::RecordProperty("Verifies", "SCR-5878624");  // SWS_CM_00306
    ::testing::Test::RecordProperty(
        "Description",
        "Checks whether SamplePtr can be default constructed. We accept a deviation where this is not constexpr.");
    ::testing::Test::RecordProperty("TestType", "Requirements-based test");
    ::testing::Test::RecordProperty("Priority", "1");
    ::testing::Test::RecordProperty("DerivationTechnique", "Analysis of requirements");

    SamplePtr<TypeParam> unit{};
}

TYPED_TEST(SamplePtrGenericTypeTest, CanBeNullptrConstructed)
{
    ::testing::Test::RecordProperty("Verifies", "SCR-5878624");  // SWS_CM_00306
    ::testing::Test::RecordProperty(
        "Description",
        "Checks whether SamplePtr can be implicit nullptr constructed. We accept a deviation where this is "
        "not constexpr.");
    ::testing::Test::RecordProperty("TestType", "Requirements-based test");
    ::testing::Test::RecordProperty("Priority", "1");
    ::testing::Test::RecordProperty("DerivationTechnique", "Analysis of requirements");

    SamplePtr<TypeParam> unit = nullptr;
}

TYPED_TEST(SamplePtrGenericTypeTest, CannotBeCopied)
{
    ::testing::Test::RecordProperty("Verifies", "SCR-5878624");  // SWS_CM_00306
    ::testing::Test::RecordProperty("Description", "Checks whether SamplePtr can not be copied");
    ::testing::Test::RecordProperty("TestType", "Requirements-based test");
    ::testing::Test::RecordProperty("Priority", "1");
    ::testing::Test::RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(!std::is_copy_constructible<SamplePtr<TypeParam>>::value, "Should not be copyiable");
    static_assert(!std::is_copy_assignable<SamplePtr<TypeParam>>::value, "Should not be copyiable");
}

TYPED_TEST(SamplePtrGenericTypeTest, CanBeMoved)
{
    ::testing::Test::RecordProperty("Verifies", "SCR-5878624");  // SWS_CM_00306
    ::testing::Test::RecordProperty("Description", "Checks whether SamplePtr can be moved");
    ::testing::Test::RecordProperty("TestType", "Requirements-based test");
    ::testing::Test::RecordProperty("Priority", "1");
    ::testing::Test::RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(std::is_move_constructible<SamplePtr<TypeParam>>::value, "Should be moveable");
    static_assert(std::is_move_assignable<SamplePtr<TypeParam>>::value, "Should be moveable");
}

TYPED_TEST(SamplePtrGenericTypeTest, InterfaceMatchesRequirements)
{
    ::testing::Test::RecordProperty("Verifies", "SCR-21470910");
    ::testing::Test::RecordProperty(
        "Description", "Checks that SamplePtr provides at least the interface specified in the requirements");
    ::testing::Test::RecordProperty("TestType", "Requirements-based test");
    ::testing::Test::RecordProperty("Priority", "1");
    ::testing::Test::RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Default constructor
    static_assert(std::is_nothrow_default_constructible_v<SamplePtr<TypeParam>>, "Should be default constructible");

    // semantically equivalent to Default constructor
    static_assert(std::is_nothrow_constructible_v<SamplePtr<TypeParam>, std::nullptr_t>,
                  "Should be constructible with std::nullptr_t");

    // Copy constructor is deleted
    static_assert(!std::is_copy_constructible_v<SamplePtr<TypeParam>>, "Should not be copy constructible");

    // Move constructor
    static_assert(std::is_move_constructible_v<SamplePtr<TypeParam>>, "Should be move constructible");

    // Default copy assignment operator is deleted
    static_assert(!std::is_copy_assignable_v<SamplePtr<TypeParam>>, "Should not be copy assignable");

    // Assignment of nullptr_t
    static_assert(std::is_assignable_v<SamplePtr<TypeParam>, std::nullptr_t>,
                  "Should not be assignable with std::nullptr_t");

    // Move assignment operator
    static_assert(std::is_move_assignable_v<SamplePtr<TypeParam>>, "Should be move assignable");

    // Dereferences the stored pointer
    static_assert(std::is_member_function_pointer_v<decltype(&SamplePtr<TypeParam>::operator->)>,
                  "Should contain operator->");

    // Checks if the stored pointer is null
    static_assert(std::is_member_function_pointer_v<decltype(&SamplePtr<TypeParam>::operator bool)>,
                  "Should contain operator bool");

    // Swaps the managed object
    static_assert(std::is_member_function_pointer_v<decltype(&SamplePtr<TypeParam>::Swap)>, "Should contain Swap");

    // Replaces the managed object
    static_assert(std::is_member_function_pointer_v<decltype(&SamplePtr<TypeParam>::Reset)>, "Should contain Reset");

    // Returns the stored object
    static_assert(std::is_member_function_pointer_v<decltype(&SamplePtr<TypeParam>::Get)>, "Should contain Get");
}

TEST(SamplePtr, SamplePtrProvidesDereferenceOperatorWhenTypeNotVoid)
{
    RecordProperty("Verifies", "SCR-21470910");
    RecordProperty("Description",
                   "Checks that SamplePtr provides a dereference operator when the Sample type is not void");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Dereferences the stored pointer
    using SampleType = TestSampleType;
    using element_type = SamplePtr<SampleType>::element_type;
    constexpr auto dereference_operator =
        static_cast<std::add_lvalue_reference_t<element_type> (SamplePtr<SampleType>::*)() const noexcept>(
            &SamplePtr<SampleType>::operator*);
    static_assert(std::is_member_function_pointer_v<decltype(dereference_operator)>, "Should contain operator*");
}

TEST(SamplePtr, CanDereference)
{
    RecordProperty("Verifies", "SCR-5878624");  // SWS_CM_00306
    RecordProperty("Description", "Checks whether SamplePtr can be dereferenced");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    mock_binding::SamplePtr<TestSampleType> pointer = std::make_unique<TestSampleType>(42);
    SamplePtr<TestSampleType> unit{std::move(pointer), SampleReferenceGuard{}};

    EXPECT_EQ(*unit, 42);
    EXPECT_EQ(*unit.get(), 42);
    EXPECT_EQ(*unit.Get(), 42);
    EXPECT_NE(unit, nullptr);
    EXPECT_NE(nullptr, unit);

    SamplePtr<TestSampleType> empty_unit{nullptr};
    EXPECT_EQ(empty_unit, nullptr);
    EXPECT_EQ(nullptr, empty_unit);
    EXPECT_EQ(empty_unit.get(), nullptr);
    EXPECT_EQ(empty_unit.Get(), nullptr);
}

TEST(SamplePtr, CanArrowOperator)
{
    RecordProperty("Verifies", "SCR-5878624");  // SWS_CM_00306
    RecordProperty("Description", "Checks whether SamplePtr can use arrow operator");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    struct Foo
    {
        TestSampleType bar{42};
    };

    mock_binding::SamplePtr<Foo> pointer = std::make_unique<Foo>(Foo{});
    SamplePtr<Foo> unit{std::move(pointer), SampleReferenceGuard{}};

    EXPECT_EQ(unit->bar, 42);
}

TYPED_TEST(SamplePtrGenericTypeTest, ExplicitBool)
{
    ::testing::Test::RecordProperty("Verifies", "SCR-5878624");  // SWS_CM_00306
    ::testing::Test::RecordProperty("Description", "Checks whether SamplePtr can be converted to bool");
    ::testing::Test::RecordProperty("TestType", "Requirements-based test");
    ::testing::Test::RecordProperty("Priority", "1");
    ::testing::Test::RecordProperty("DerivationTechnique", "Analysis of requirements");

    SamplePtr<TypeParam> unit{std::move(SamplePtrGenericTypeTest<TypeParam>::mock_pointer_), SampleReferenceGuard{}};

    EXPECT_TRUE(unit);
    EXPECT_FALSE(SamplePtr<TypeParam>{nullptr});
}

TYPED_TEST(SamplePtrGenericTypeTest, CanSwap)
{
    ::testing::Test::RecordProperty("Verifies", "SCR-5878624");  // SWS_CM_00306
    ::testing::Test::RecordProperty("Description", "Checks whether SamplePtr can be swapped");
    ::testing::Test::RecordProperty("TestType", "Requirements-based test");
    ::testing::Test::RecordProperty("Priority", "1");
    ::testing::Test::RecordProperty("DerivationTechnique", "Analysis of requirements");

    SamplePtr<TypeParam> unit{std::move(SamplePtrGenericTypeTest<TypeParam>::mock_pointer_), SampleReferenceGuard{}};
    SamplePtr<TypeParam> other{nullptr};

    unit.Swap(other);

    EXPECT_TRUE(other);
    EXPECT_FALSE(unit);
}

TYPED_TEST(SamplePtrGenericTypeTest, CanReset)
{
    ::testing::Test::RecordProperty("Verifies", "SCR-5878624");  // SWS_CM_00306
    ::testing::Test::RecordProperty("Description", "Checks whether SamplePtr can be reseted");
    ::testing::Test::RecordProperty("TestType", "Requirements-based test");
    ::testing::Test::RecordProperty("Priority", "1");
    ::testing::Test::RecordProperty("DerivationTechnique", "Analysis of requirements");

    SamplePtr<TypeParam> unit{std::move(SamplePtrGenericTypeTest<TypeParam>::mock_pointer_), SampleReferenceGuard{}};

    unit.Reset();

    EXPECT_FALSE(unit);
}

TYPED_TEST(SamplePtrGenericTypeTest, CanAssignNullptr)
{
    SamplePtr<TypeParam> unit{std::move(SamplePtrGenericTypeTest<TypeParam>::mock_pointer_), SampleReferenceGuard{}};

    unit = nullptr;

    EXPECT_FALSE(unit);
}

template <typename T>
SamplePtr<T> CreateMockBindingSamplePtr(SampleReferenceTracker& tracker)
{
    mock_binding::SamplePtr<T> pointer(new TestSampleType, [](T* p) noexcept {
        auto* const int_p = static_cast<TestSampleType*>(p);
        delete int_p;
    });
    TrackerGuardFactory guard_factory{tracker.Allocate(1)};
    auto guard = std::move(*guard_factory.TakeGuard());
    SamplePtr<T> sample_ptr(std::move(pointer), std::move(guard));
    return sample_ptr;
}

TYPED_TEST(SamplePtrGenericTypeTest, SamplePtrWillIncrementAvailableSamplesOnDestruction)
{
    const std::size_t max_num_samples{5};

    // Given a SampleReferenceTracker
    SampleReferenceTracker tracker(max_num_samples);

    {
        // When creating a SamplePtr
        auto sample_ptr = CreateMockBindingSamplePtr<TypeParam>(tracker);

        // Then the number of available samples will be decremented by 1
        EXPECT_EQ(tracker.GetNumAvailableSamples(), max_num_samples - 1U);
    }

    // And when the SamplePtr is destroyed
    // Then the number of available samples will be re-incremented by 1
    EXPECT_EQ(tracker.GetNumAvailableSamples(), max_num_samples);
}

TYPED_TEST(SamplePtrGenericTypeTest, SamplePtrWillIncrementAvailableSamplesOnReset)
{
    const std::size_t max_num_samples{5};

    // Given a SampleReferenceTracker
    SampleReferenceTracker tracker(max_num_samples);

    // When creating a SamplePtr
    auto sample_ptr = CreateMockBindingSamplePtr<TypeParam>(tracker);

    // Then the number of available samples will be decremented by 1
    EXPECT_EQ(tracker.GetNumAvailableSamples(), max_num_samples - 1U);

    // And when the SamplePtr is reset
    sample_ptr.Reset();

    // Then the number of available samples will be re-incremented by 1
    EXPECT_EQ(tracker.GetNumAvailableSamples(), max_num_samples);
}

TYPED_TEST(SamplePtrGenericTypeTest, MovingSamplePtrWillMoveSampleReferenceGuard)
{
    const std::size_t max_num_samples{5};

    // Given a SampleReferenceTracker
    SampleReferenceTracker tracker(max_num_samples);

    // When creating 2 SamplePtrs which share the same SampleReferenceTracker.
    auto sample_ptr1 = CreateMockBindingSamplePtr<TypeParam>(tracker);
    auto sample_ptr2 = CreateMockBindingSamplePtr<TypeParam>(tracker);

    // Then the number of available samples will be decremented by 2
    EXPECT_EQ(tracker.GetNumAvailableSamples(), max_num_samples - 2U);

    // and when one SamplePtr is moved to the other SamplePtr
    sample_ptr2 = std::move(sample_ptr1);

    // Then the nubmer of available samples will not change.
    EXPECT_EQ(tracker.GetNumAvailableSamples(), max_num_samples - 1);
}

}  // namespace
}  // namespace score::mw::com::impl
