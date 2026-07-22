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
#include "score/mw/com/impl/methods/method_signature_element_ptr.h"

#include <gtest/gtest.h>
#include <memory>

namespace score::mw::com::impl
{
namespace
{

static constexpr int kTestElementValue = 42;
static constexpr std::size_t kDefaultQueuePosition = 2;

struct TestElementType
{
    int value;
    explicit TestElementType(int v) : value(v) {}
};

struct TestElementWrapperType
{
    TestElementType wrapped_element;
    explicit TestElementWrapperType(int v) : wrapped_element(v) {}
};

class MethodSignatureElementPtrTestFixture : public ::testing::Test
{
  public:
    MethodSignatureElementPtrTestFixture& GivenAValidMethodSignatureElementPtr()
    {
        unit_ = std::make_unique<MethodSignatureElementPtr<TestElementType>>(
            test_element_, active_flag_, kDefaultQueuePosition);
        return *this;
    }

    bool active_flag_{false};
    TestElementType test_element_{kTestElementValue};
    std::unique_ptr<MethodSignatureElementPtr<TestElementType>> unit_{nullptr};
};

TEST_F(MethodSignatureElementPtrTestFixture, Construction_SetsActiveFlag)
{
    // When constructing a MethodSignatureElementPtr with active flag reference which is initially false
    ASSERT_FALSE(active_flag_);
    MethodSignatureElementPtr<TestElementType> unit{test_element_, active_flag_, kDefaultQueuePosition};

    // Then the active flag is set to true after construction
    EXPECT_TRUE(active_flag_);
}

TEST_F(MethodSignatureElementPtrTestFixture, Construction_PointsToElement)
{
    // Given a MethodSignatureElementPtr constructed with a pointer to test_element_
    MethodSignatureElementPtr<TestElementType> unit{test_element_, active_flag_, kDefaultQueuePosition};

    // When calling get(), then the internal pointer points to the given TestElementType instance, the unit was
    // constructed with
    // Then the returned pointer is the same as the pointer provided to the constructor
    ASSERT_TRUE(unit.get() != nullptr);
    EXPECT_EQ(unit.get(), &test_element_);
}

TEST_F(MethodSignatureElementPtrTestFixture, Construction_CorrectQueuePosition)
{
    GivenAValidMethodSignatureElementPtr();

    // When calling GetQueuePosition()
    // Then the internal queue position is set correctly
    EXPECT_EQ(unit_->GetQueuePosition(), kDefaultQueuePosition);
}

TEST_F(MethodSignatureElementPtrTestFixture, Destruction_ClearsActiveFlag)
{
    GivenAValidMethodSignatureElementPtr();
    ASSERT_TRUE(active_flag_);

    // When the MethodSignatureElementPtr is destroyed
    unit_.reset();

    // then the active flag is set to false after destruction
    EXPECT_FALSE(active_flag_);
}

TEST_F(MethodSignatureElementPtrTestFixture, MoveConstruction_ActiveFlagSet_BeforeMovedFromInstanceDestroyed)
{
    GivenAValidMethodSignatureElementPtr();

    // When move-constructing a new MethodSignatureElementPtr from another MethodSignatureElementPtr
    MethodSignatureElementPtr<TestElementType> move_constructed_unit(std::move(*unit_));

    // then the active flag is (still) set to true before the moved-from instance is destroyed
    EXPECT_TRUE(active_flag_);
}

TEST_F(MethodSignatureElementPtrTestFixture, MoveConstruction_ActiveFlagSet_AfterMovedFromInstanceDestroyed)
{
    GivenAValidMethodSignatureElementPtr();

    // Given that a new MethodSignatureElementPtr was move-constructed from another MethodSignatureElementPtr
    MethodSignatureElementPtr<TestElementType> move_constructed_unit(std::move(*unit_));

    // when destroying the moved-from instance
    unit_.reset();

    // then the active flag is (still) set to true after the moved-from instance is destroyed
    EXPECT_TRUE(active_flag_);
}

TEST_F(MethodSignatureElementPtrTestFixture, MoveConstruction_ActiveFlagCleared_AfterMoveConstructedInstanceDestroyed)
{
    GivenAValidMethodSignatureElementPtr();

    {
        // and given a new MethodSignatureElementPtr which is move constructed from the original one
        MethodSignatureElementPtr<TestElementType> moved_constructed_unit(std::move(*unit_));

        // when the move-constructed instance is destroyed and the moved-from instance still exists
    }

    // Then the active flag is false
    EXPECT_FALSE(active_flag_);
}

TEST_F(MethodSignatureElementPtrTestFixture, MoveConstruction_CorrectQueuePosition)
{
    GivenAValidMethodSignatureElementPtr();

    // and given a new MethodSignatureElementPtr which is move constructed from the original one
    MethodSignatureElementPtr<TestElementType> moved_constructed_unit(std::move(*unit_));

    // When calling GetQueuePosition
    // Then the internal queue position is set correctly
    EXPECT_EQ(unit_->GetQueuePosition(), kDefaultQueuePosition);
}

TEST_F(MethodSignatureElementPtrTestFixture, MoveConstruction_CorrectElementValue)
{
    GivenAValidMethodSignatureElementPtr();

    // and given a new MethodSignatureElementPtr which is move constructed from the original one
    MethodSignatureElementPtr<TestElementType> moved_constructed_unit(std::move(*unit_));

    // when calling get()
    // Then the returned pointer points to the correct TestElementType instance
    EXPECT_EQ(moved_constructed_unit.get(), &test_element_);
}

TEST_F(MethodSignatureElementPtrTestFixture, DereferenceOperator_WorksCorrectly)
{
    GivenAValidMethodSignatureElementPtr();

    // When dereferencing the MethodSignatureElementPtr
    MethodSignatureElementPtr<TestElementType>& unit_ref{*unit_};
    TestElementType& element_ref = *unit_ref;

    // then the returned reference points to the correct TestElementType instance
    EXPECT_EQ(element_ref.value, kTestElementValue);
}

TEST_F(MethodSignatureElementPtrTestFixture, ArrowOperator_WorksCorrectly)
{
    GivenAValidMethodSignatureElementPtr();

    // When using the arrow operator on the MethodSignatureElementPtr
    // Then it returns the correct value
    MethodSignatureElementPtr<TestElementType>& unit_ref{*unit_};
    EXPECT_EQ(unit_ref->value, kTestElementValue);
}

TEST_F(MethodSignatureElementPtrTestFixture,
       ConvertingMoveConstruction_PointsToConvertedElementAndKeepsOwnershipAfterMovedFromDestroyed)
{
    // Given a wrapper element and an active MethodSignatureElementPtr to that wrapper element
    TestElementWrapperType wrapped_element{kTestElementValue};
    auto wrapped_ptr = std::make_unique<MethodSignatureElementPtr<TestElementWrapperType>>(
        wrapped_element, active_flag_, kDefaultQueuePosition);
    ASSERT_TRUE(active_flag_);

    // When move-converting to a MethodSignatureElementPtr that points to the wrapped element
    MethodSignatureElementPtr<TestElementType> converted_ptr{wrapped_element.wrapped_element, std::move(*wrapped_ptr)};

    // and when destroying the moved-from instance
    wrapped_ptr.reset();

    // Then ownership of the active flag remains with the converted instance
    EXPECT_TRUE(active_flag_);
    // and the converted instance points to the expected element with preserved metadata
    EXPECT_EQ(converted_ptr.get(), &wrapped_element.wrapped_element);
    EXPECT_EQ(converted_ptr->value, kTestElementValue);
    EXPECT_EQ(converted_ptr.GetQueuePosition(), kDefaultQueuePosition);
}

TEST_F(MethodSignatureElementPtrTestFixture,
       ConvertingMoveConstruction_ClearsActiveFlag_AfterConvertedInstanceDestroyed)
{
    // Given a wrapper element and an active MethodSignatureElementPtr to that wrapper element
    TestElementWrapperType wrapped_element{kTestElementValue};
    auto wrapped_ptr = std::make_unique<MethodSignatureElementPtr<TestElementWrapperType>>(
        wrapped_element, active_flag_, kDefaultQueuePosition);
    ASSERT_TRUE(active_flag_);

    // and given a move-converted MethodSignatureElementPtr that points to the wrapped element
    auto converted_ptr = std::make_unique<MethodSignatureElementPtr<TestElementType>>(wrapped_element.wrapped_element,
                                                                                      std::move(*wrapped_ptr));

    // and given that the moved-from instance was destroyed
    wrapped_ptr.reset();
    ASSERT_TRUE(active_flag_);

    // When destroying the converted instance
    converted_ptr.reset();

    // Then the active flag is set to false
    EXPECT_FALSE(active_flag_);
}

}  // namespace

}  // namespace score::mw::com::impl
