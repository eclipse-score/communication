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
#include "score/mw/com/impl/plumbing/sample_allocatee_ptr.h"

#include "score/mw/com/impl/bindings/lola/control_slot_types.h"
#include "score/mw/com/impl/bindings/lola/test_doubles/fake_memory_resource.h"

#include "score/mw/com/impl/bindings/mock_binding/sample_allocatee_ptr.h"

#include <score/assert.hpp>

#include <score/blank.hpp>
#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <utility>

namespace score::mw::com::impl
{
namespace
{

class ObjectDestructionNotifier
{
  public:
    ObjectDestructionNotifier(bool& is_destructed) : is_destructed_{is_destructed}
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(is_destructed == false, "Is destructed flag must be false.");
    };
    ~ObjectDestructionNotifier()
    {
        is_destructed_ = true;
    }

  private:
    bool& is_destructed_;
};

class SampleAllocateePtrFixture : public ::testing::Test
{
  public:
    std::uint8_t value_{0x42};
    lola::FakeMemoryResource fake_memory_resource_{};
    lola::EventDataControl event_data_ctrl_qm_{0, fake_memory_resource_.getMemoryResourceProxy(), 1U};
    lola::EventDataControlComposite event_data_ctrl_{&event_data_ctrl_qm_};
    lola::SlotIndexType event_data_slot_index_{std::numeric_limits<lola::SlotIndexType>::max()};
    lola::SampleAllocateePtr<std::uint8_t> lola_allocatee_ptr_{&value_, event_data_ctrl_, {}};
    SampleAllocateePtr<std::uint8_t> valid_unit_{MakeSampleAllocateePtr(std::move(lola_allocatee_ptr_))};
    

    SampleAllocateePtr<std::uint8_t> unit_with_unique_ptr_{
        MakeSampleAllocateePtr(mock_binding::SampleAllocateePtr<std::uint8_t>(new std::uint8_t(42), [](std::uint8_t* p) { delete p; } ))};
};

TEST_F(SampleAllocateePtrFixture, ConstructFromNullptr)
{
    RecordProperty("Verifies", "SCR-5878642");  // SWS_CM_00308
    RecordProperty("Description", "Checks whether SampleAllocateePtr can be constructed from nullptr");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const auto unit = SampleAllocateePtr<std::uint8_t>{nullptr};
}

TEST_F(SampleAllocateePtrFixture, CanReset)
{
    RecordProperty("Verifies", "SCR-5878642");  // SWS_CM_00308
    RecordProperty("Description", "Checks whether SampleAllocateePtr can be reseted");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");
    // Given a valid unit

    // When calling reset
    valid_unit_.reset();

    // Then the pointer is no longer valid
    EXPECT_EQ(nullptr, valid_unit_.Get());
}

TEST_F(SampleAllocateePtrFixture, CanSwap)
{
    RecordProperty("Verifies", "SCR-5878642");  // SWS_CM_00308
    RecordProperty("Description", "Checks whether SampleAllocateePtr can be swaped");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");
    // Given a valid_unit and a second SampleAllocateePtr
    std::uint8_t value{0x43};
    lola::SampleAllocateePtr<std::uint8_t> foo{&value, event_data_ctrl_, {}};
    auto unit = MakeSampleAllocateePtr(std::move(foo));

    // When swapping both classes
    unit.Swap(valid_unit_);

    // Then the one contains the value of the other
    EXPECT_EQ(&value_, unit.Get());
}

TEST_F(SampleAllocateePtrFixture, CanGetUnderlyingPointer)
{
    RecordProperty("Verifies", "SCR-5878642");  // SWS_CM_00308
    RecordProperty("Description", "Checks whether SampleAllocateePtr can retrieve underlying pointer");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");
    // Given a valid unit

    // When getting the underlying pointer
    auto* const value = valid_unit_.Get();

    // Then it points to the expected value
    EXPECT_EQ(&value_, value);
}

TEST_F(SampleAllocateePtrFixture, ValidConvertsToTrue)
{
    RecordProperty("Verifies", "SCR-5878642");  // SWS_CM_00308
    RecordProperty("Description", "Checks whether SampleAllocateePtr can implicit converted to bool");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");
    // Given a valid unit
    // When using it in an boolean expression
    // Then it converts to true
    EXPECT_TRUE(valid_unit_);
}

TEST_F(SampleAllocateePtrFixture, InvalidConvertstoFalse)
{
    RecordProperty("Verifies", "SCR-5878642");  // SWS_CM_00308
    RecordProperty("Description", "Checks whether SampleAllocateePtr can be implicit converted to bool (positive)");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");
    // Given an invalid (empty) unit
    const auto unit = SampleAllocateePtr<std::uint8_t>{};

    // When using it in an boolean expression
    // Then it converts to false
    EXPECT_FALSE(unit);
}

TEST_F(SampleAllocateePtrFixture, SampleAllocateeInitialisedWithNullptrConvertstoFalse)
{
    RecordProperty("Verifies", "SCR-5878642");  // SWS_CM_00308
    RecordProperty("Description", "Checks whether SampleAllocateePtr can be implicit converted to bool (positive)");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given an SampleAllocateePtr initialised with a nullptr
    const auto unit = SampleAllocateePtr<std::uint8_t>{nullptr};

    // When using it in a boolean expression
    // Then it converts to false
    EXPECT_FALSE(unit);
}

TEST(SampleAllocateePtrTest, InterfaceMatchesRequirements)
{
    RecordProperty("Verifies", "SCR-21470937");
    RecordProperty("Description",
                   "Checks that SampleAllocateePtr provides at least the interface specified in the requirements");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    using SampleType = std::uint8_t;

    // Default constructor
    static_assert(std::is_nothrow_default_constructible_v<SampleAllocateePtr<SampleType>>,
                  "Should be default constructible");

    // semantically equivalent to Default constructor
    static_assert(std::is_nothrow_constructible_v<SampleAllocateePtr<SampleType>, std::nullptr_t>,
                  "Should be constructible with std::nullptr_t");

    // Copy constructor is deleted
    static_assert(!std::is_copy_constructible_v<SampleAllocateePtr<SampleType>>, "Should not be copy constructible");

    // Move constructor
    static_assert(std::is_move_constructible_v<SampleAllocateePtr<SampleType>>, "Should be move constructible");

    // Default copy assignment operator is deleted
    static_assert(!std::is_copy_assignable_v<SampleAllocateePtr<SampleType>>, "Should not be copy assignable");

    // Assignment of nullptr_t
    static_assert(std::is_assignable_v<SampleAllocateePtr<SampleType>, std::nullptr_t>,
                  "Should not be assignable with std::nullptr_t");

    // Move assignment operator
    static_assert(std::is_move_assignable_v<SampleAllocateePtr<SampleType>>, "Should be move assignable");

    // Checks if the stored pointer is null
    static_assert(std::is_member_function_pointer_v<decltype(&SampleAllocateePtr<SampleType>::operator bool)>,
                  "Should contain operator bool");

    // Swaps the managed object
    static_assert(std::is_member_function_pointer_v<decltype(&SampleAllocateePtr<SampleType>::Swap)>,
                  "Should contain Swap");

    // Returns the stored object
    static_assert(std::is_member_function_pointer_v<decltype(&SampleAllocateePtr<SampleType>::Get)>,
                  "Should contain Get");
}

TEST(SampleAllocateePtrTest, NullLolaSampleAllocateePtrConvertsToFalse)
{
    // Given a lola::SampleAllocateePtr which holds a nullptr
    lola::SampleAllocateePtr<std::uint8_t> null_lola_ptr{nullptr};

    // When creating an impl::SampleAllocateePtr from the lola::SampleAllocateePtr
    auto ptr = MakeSampleAllocateePtr(std::move(null_lola_ptr));

    // and using it in a boolean expression
    // Then it converts to false
    EXPECT_FALSE(ptr);
}

TEST(SampleAllocateePtrTest, NullUniquePtrConvertsToFalse)
{
    // Given a unique_ptr which holds a nullptr

    mock_binding::SampleAllocateePtr<std::uint8_t> null_unique_ptr{nullptr};

    // When creating an impl::SampleAllocateePtr from the unique_ptr
    auto ptr = MakeSampleAllocateePtr(std::move(null_unique_ptr));

    // and using it in a boolean expression
    // Then it converts to false
    EXPECT_FALSE(ptr);
}

TEST_F(SampleAllocateePtrFixture, ValidLolaSampleAllocateePtrConvertsToTrue)
{
    struct Foo
    {
        std::uint8_t bar{};
    } value;

    // Given a valid lola::SampleAllocateePtr
    lola::SampleAllocateePtr<Foo> valid_lola_ptr{&value, event_data_ctrl_, {}};

    // When creating an impl::SampleAllocateePtr from the lola::SampleAllocateePtr
    const auto ptr = MakeSampleAllocateePtr(std::move(valid_lola_ptr));

    // and using it in a boolean expression
    // Then it converts to true
    EXPECT_TRUE(ptr);
}

TEST_F(SampleAllocateePtrFixture, ValidUniquePtrConvertsToTrue)
{
    // Given a valid unique_ptr

    mock_binding::SampleAllocateePtr<std::uint8_t> valid_unique_ptr(new std::uint8_t(10),[](std::uint8_t* p) { delete p; } );    
    EXPECT_TRUE(valid_unique_ptr);

    // When creating an impl::SampleAllocateePtr from the unique_ptr
    const auto ptr = MakeSampleAllocateePtr(std::move(valid_unique_ptr));

    // and using it in a boolean expression
    // Then it converts to true
    EXPECT_TRUE(ptr);
}

TEST_F(SampleAllocateePtrFixture, CanDereferenceToUnderlyingValue)
{
    RecordProperty("Verifies", "SCR-5878642");  // SWS_CM_00308
    RecordProperty("Description", "Checks whether SampleAllocateePtr can be dereferenced.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");
    // Given an valid unit
    // When dereferencing
    // Then the underlying value is returned
    EXPECT_EQ(value_, *valid_unit_);
}

TEST_F(SampleAllocateePtrFixture, CanDereferenceUsingArrow)
{
    RecordProperty("Verifies", "SCR-5878642");  // SWS_CM_00308
    RecordProperty("Description", "Checks whether SampleAllocateePtr can be pointer dereference.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");
    struct Foo
    {
        std::uint8_t bar{};
    } value;

    value.bar = 0x42;

    lola::SampleAllocateePtr<Foo> foo{&value, event_data_ctrl_, {}};
    const auto unit = MakeSampleAllocateePtr(std::move(foo));

    EXPECT_EQ(value.bar, unit->bar);
}

TEST_F(SampleAllocateePtrFixture, CanAccessUnderlyingSlot)
{
    // Given a view on a SampleAllocateePtr
    struct Foo
    {
        std::uint8_t bar{};
    } value;
    lola::SampleAllocateePtr<Foo> foo{&value, event_data_ctrl_, {}};
    const auto ptr = MakeSampleAllocateePtr(std::move(foo));
    const auto unit = SampleAllocateePtrView<Foo>{ptr};

    // When trying to read its underlying implementation
    const auto* underlying_impl = unit.As<lola::SampleAllocateePtr<Foo>>();

    // This is possible and we can interact with it
    ASSERT_NE(underlying_impl, nullptr);
}

TEST_F(SampleAllocateePtrFixture, CanGetUnderlyingPointerUsingUniquePtr)
{
    RecordProperty("Verifies", "SCR-5878642");  // SWS_CM_00308
    RecordProperty("Description", "Checks whether SampleAllocateePtr can get underlying pointer");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");
    // Given a valid unit

    // When getting the underlying pointer
    auto* const value = unit_with_unique_ptr_.Get();

    // Then it points to the expected value
    EXPECT_EQ(42, *value);
}

TEST_F(SampleAllocateePtrFixture, CanGetUnderlyingBlankPointer)
{
    // Given an invalid (empty) unit
    const auto unit = SampleAllocateePtr<std::uint8_t>{};

    // When getting the underlying pointer
    auto* const value = unit.Get();

    // Then it is a nullptr
    EXPECT_EQ(value, nullptr);
}

TEST_F(SampleAllocateePtrFixture, CanResetUnderlyingPointerUsingUniquePtr)
{
    // Given a SampleAllocateePtr with an underlying unique_ptr
    bool is_destructed{false};
    SampleAllocateePtr<ObjectDestructionNotifier> unit_with_unique_ptr{
        MakeSampleAllocateePtr(mock_binding::SampleAllocateePtr<ObjectDestructionNotifier>(new ObjectDestructionNotifier(is_destructed),[](ObjectDestructionNotifier* p) { delete p; }))};

    // When calling Reset
    unit_with_unique_ptr.reset();

    // Then the underlying unique_ptr will be destroyed
    EXPECT_TRUE(is_destructed);
}

TEST_F(SampleAllocateePtrFixture, ResettingWithUnderlyingBlankPointerDoesNotCrash)
{
    // Given an invalid (empty) unit
    auto unit = SampleAllocateePtr<std::uint8_t>{};

    // When calling reset
    unit.reset();

    // Then the process will not crash
}

TEST_F(SampleAllocateePtrFixture, CanDereferenceToUnderlyingValueUsingUniquePtr)
{
    RecordProperty("Verifies", "SCR-5878642");  // SWS_CM_00308
    RecordProperty("Description", "Checks whether SampleAllocateePtr can dereference");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");
    // Given an valid unit
    // When dereferencing
    // Then the underlying value is returned
    EXPECT_EQ(*unit_with_unique_ptr_, 42);
}

TEST_F(SampleAllocateePtrFixture, CanDereferenceUsingArrowUsingUniquePtr)
{
    RecordProperty("Verifies", "SCR-5878642");  // SWS_CM_00308
    RecordProperty("Description", "Checks whether SampleAllocateePtr can dereference using arrow");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");
    struct Foo
    {
        std::uint8_t bar{};
    };


    auto value = mock_binding::SampleAllocateePtr<Foo>(new Foo(),[](Foo* p) { delete p; });
    value->bar = 42;
    const auto unit = MakeSampleAllocateePtr(std::move(value));

    EXPECT_EQ(42, unit->bar);
}

TEST_F(SampleAllocateePtrFixture, CanWrapUniquePtr)
{
    // Given a SampleAllocateePtr with an underlying unique_ptr

    const auto ptr = MakeSampleAllocateePtr(mock_binding::SampleAllocateePtr<std::uint8_t>(new std::uint8_t(),[](std::uint8_t* p) { delete p; }));
    const auto unit = SampleAllocateePtrView<std::uint8_t>{ptr};

    // When trying to read its underlying implementation

    const auto* underlying_impl = unit.As<mock_binding::SampleAllocateePtr<std::uint8_t>>();

    // This is possible and we can interact with it
    ASSERT_NE(underlying_impl, nullptr);
}

TEST_F(SampleAllocateePtrFixture, CanCompareTwoUnequalPtrs)
{
    // Given a valid_unit and a second SampleAllocateePtr pointing to a different value
    std::uint8_t value{0x43};

    SampleAllocateePtr<std::uint8_t> unit2{MakeSampleAllocateePtr(mock_binding::SampleAllocateePtr<std::uint8_t>(new std::uint8_t(value),[](std::uint8_t* p) { delete p; }))};

    // When testing equality
    // Then the pointers are considered unequal
    EXPECT_TRUE(valid_unit_ != unit2);
    EXPECT_FALSE(valid_unit_ == unit2);
}

using SampleAllocateePtrFixtureDeathTest = SampleAllocateePtrFixture;

TEST_F(SampleAllocateePtrFixtureDeathTest, CannotDereferenceBlankPointer)
{
    // Given an invalid (empty) unit
    const auto unit = SampleAllocateePtr<std::uint8_t>{};

    // When dereferencing the underlying pointer
    // Then the program will terminate
    EXPECT_DEATH(*unit, ".*");
}

TEST_F(SampleAllocateePtrFixtureDeathTest, CannotUseArrowOperatorOnBlankPointer)
{
    struct A
    {
        int value;
    };

    // Given an invalid (empty) unit
    const auto unit = SampleAllocateePtr<A>{};

    // When dereferencing the underlying pointer
    // Then the program will terminate
    EXPECT_DEATH(score::cpp::ignore = unit->value, ".*");
}

TEST(SampleAllocateePtrTest, UnderlyingUniquePtrIsFreedOnDestruction)
{
    RecordProperty("Verifies", "SCR-6244646");
    RecordProperty("Description",
                   "SampleAllocateePtr shall free resources only on destruction. Note. the underlying memory of the "
                   "pointed-to object is not deleted. The underlying lola::SampleAllocateePtr merely marks the slot as "
                   "invalid and stops pointing to the object.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    bool is_destructed{false};
    {
        // Given a SampleAllocateePtr with an underlying unique_ptr

        SampleAllocateePtr<ObjectDestructionNotifier> unit_with_unique_ptr{
            MakeSampleAllocateePtr(mock_binding::SampleAllocateePtr<ObjectDestructionNotifier>(new ObjectDestructionNotifier(is_destructed),[](ObjectDestructionNotifier* p) { delete p; }))};

        // The underlying object will not be freed while the SampleAllocateePtr has not been destructed
        EXPECT_FALSE(is_destructed);
    }

    // And will be freed once the SampleAllocateePtr has been destructed
    EXPECT_TRUE(is_destructed);
}

TEST_F(SampleAllocateePtrFixture, UnderlyingLolaPtrIsFreedOnDestruction)
{
    RecordProperty("Verifies", "SCR-6244646");
    RecordProperty("Description",
                   "SampleAllocateePtr shall free resources only on destruction. Note. the underlying memory of the "
                   "pointed-to object is not deleted. The underlying lola::SampleAllocateePtr merely marks the slot as "
                   "invalid and stops pointing to the object.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a SampleAllocateePtr with an underlying lola SampleAllocateePtr
    lola::EventDataControlComposite event_data_ctrl{&event_data_ctrl_qm_};

    bool is_destructed{false};
    ObjectDestructionNotifier object_destruction_notifier(is_destructed);
    lola::SampleAllocateePtr<ObjectDestructionNotifier> lola_allocatee_ptr{
        &object_destruction_notifier, event_data_ctrl, {}};
    {
        SampleAllocateePtr<ObjectDestructionNotifier> unit_with_lola_sample_allocatee_ptr{
            MakeSampleAllocateePtr(std::move(lola_allocatee_ptr))};

        // The underlying object will not be freed while the SampleAllocateePtr has not been destructed
        EXPECT_FALSE(is_destructed);
    }

    // The underlying object will still not be freed when the SampleAllocateePtr is destructed. However, the underlying
    // lola::SampleAllocateePtr will no longer point to the object.
    EXPECT_FALSE(is_destructed);
    EXPECT_EQ(lola_allocatee_ptr.get(), nullptr);
}

}  // namespace
}  // namespace score::mw::com::impl