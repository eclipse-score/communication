/*******************************************************************************
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
#include "score/mw/com/impl/enable_reference_to_moveable_from_this.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl
{

namespace
{

class DummyClass : public EnableReferenceToMoveableFromThis<DummyClass>
{
  public:
    explicit DummyClass(int value) : value_{value} {}

    int value_;
};

TEST(EnableReferenceToMoveableFromThisTest, GetReferenceToMoveableReturnsReferenceToObject)
{
    // Given a DummyClass which inherits from EnableReferenceToMoveableFromThis
    const int value = 42;
    DummyClass unit{value};

    // When GetReferenceToMoveable is called
    auto& reference_to_moveable = unit.GetReferenceToMoveable();

    // Then the extracted reference points to the original int
    EXPECT_EQ(reference_to_moveable.Get().value_, value);
    EXPECT_EQ(&reference_to_moveable.Get(), &unit);
}

TEST(EnableReferenceToMoveableFromThisTest, GetReturnsReferenceToUpdatedObjectWhenMoveConstructingDummyClass)
{
    // Given an DummyClass which inherits from EnableReferenceToMoveableFromThis
    const int value = 42;
    DummyClass owning_class{value};

    // and given that we want to store a reference to DummyClass which should be updated when DummyClass is moved
    auto& owning_class_reference = owning_class.GetReferenceToMoveable();

    // When the DummyClass is move constructed
    DummyClass moved_owning_class{std::move(owning_class)};

    // Then the ReferenceToMoveable retrieved from the original ReferenceToMoveable::Reference should point to the
    // moved-to DummyClass
    auto& reference_wrapper = owning_class_reference.Get();
    EXPECT_EQ(&reference_wrapper, &moved_owning_class);
    EXPECT_EQ(reference_wrapper.value_, value);
}

TEST(EnableReferenceToMoveableFromThisTest, GetReturnsReferenceToUpdatedObjectWhenMoveAssigningDummyClass)
{
    // Given an DummyClass with a ReferenceToMoveable pointing to itself
    const int moved_from_value = 42;
    const int moved_to_value = 100;
    DummyClass moved_from_owning_class{moved_from_value};
    DummyClass moved_to_owning_class{moved_to_value};

    // and given that we want to store a reference to DummyClass which should be updated when DummyClass is moved
    auto& owning_class_reference = moved_from_owning_class.GetReferenceToMoveable();

    // When the DummyClass is move constructed
    moved_to_owning_class = std::move(moved_from_owning_class);

    // Then the ReferenceToMoveable retrieved from the original ReferenceToMoveable::Reference should point to the
    // moved-to DummyClass
    auto& reference_wrapper = owning_class_reference.Get();
    EXPECT_EQ(&reference_wrapper, &moved_to_owning_class);
    EXPECT_EQ(reference_wrapper.value_, moved_from_value);
}

}  // namespace
}  // namespace score::mw::com::impl
