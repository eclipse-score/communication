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
#include "score/mw/com/impl/reference_to_moveable.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl
{

namespace
{

TEST(ReferenceToMoveableTest, NotCopyableOrMoveable)
{
    static_assert(!std::is_copy_constructible_v<ReferenceToMoveable<int>::Reference>, "Is wrongly copyable");
    static_assert(!std::is_copy_assignable_v<ReferenceToMoveable<int>::Reference>, "Is wrongly copyable");
    static_assert(!std::is_move_constructible_v<ReferenceToMoveable<int>::Reference>, "Is wrongly move constructible");
    static_assert(!std::is_move_assignable_v<ReferenceToMoveable<int>::Reference>, "Is wrongly move assignable");
}

TEST(ReferenceToMoveableTest, NotCopyable)
{
    static_assert(!std::is_copy_constructible_v<ReferenceToMoveable<int>>, "Is wrongly copyable");
    static_assert(!std::is_copy_assignable_v<ReferenceToMoveable<int>>, "Is wrongly copyable");
}

TEST(ReferenceToMoveableTest, IsMoveable)
{
    static_assert(std::is_move_constructible_v<ReferenceToMoveable<int>>, "Is not move constructible");
    static_assert(std::is_move_assignable_v<ReferenceToMoveable<int>>, "Is not move assignable");
}

TEST(ReferenceToMoveableTest, GetReturnsReferenceToObject)
{
    // Given a ReferenceToMoveable pointing to an int
    const int value = 42;
    ReferenceToMoveable<const int> wrapper{value};

    // When Get is called
    auto& reference_to_moveable = wrapper.Get();

    // Then the reference points to the original int
    EXPECT_EQ(reference_to_moveable.Get(), value);
    EXPECT_EQ(&reference_to_moveable.Get(), &value);
}

TEST(ReferenceToMoveableTest, GetReturnsReferenceToUpdatedObject)
{
    // Given a ReferenceToMoveable pointing to an int
    const int value = 42;
    ReferenceToMoveable<const int> wrapper{value};

    // When calling Update with a new int
    const int new_value = 100;
    wrapper.Update(new_value);

    // Then Get returns a reference to the ned int
    auto& reference_to_moveable = wrapper.Get();
    EXPECT_EQ(reference_to_moveable.Get(), new_value);
    EXPECT_EQ(&reference_to_moveable.Get(), &new_value);
}

// Note. This class and the test below is added as a demonstration of how ReferenceToMoveable and
// ReferenceToMoveable::Reference can be used together to allow moveable classes to hand out references to themselves
// while allowing these references to be updated when the moveable class is moved.
class DummyClass
{
  public:
    DummyClass() : ptr_wrapper_{*this} {}

    ~DummyClass() = default;

    DummyClass(const DummyClass&) = delete;
    DummyClass& operator=(const DummyClass&) & = delete;

    DummyClass(DummyClass&& other) noexcept : ptr_wrapper_{std::move(other.ptr_wrapper_)}
    {
        ptr_wrapper_.Update(*this);
    }
    DummyClass& operator=(DummyClass&& other) & noexcept
    {
        if (this != &other)
        {
            ptr_wrapper_ = std::move(other.ptr_wrapper_);
            ptr_wrapper_.Update(*this);
        }
        return *this;
    }

    ReferenceToMoveable<DummyClass>::Reference& Get()
    {
        return ptr_wrapper_.Get();
    }

  private:
    ReferenceToMoveable<DummyClass> ptr_wrapper_;
};

TEST(ReferenceToMoveableDummyClassTest, GetReturnsReferenceToUpdatedObjectWhenMoveConstructingDummyClass)
{
    // Given an DummyClass with a ReferenceToMoveable pointing to itself
    DummyClass owning_class{};

    // and given that we want to store a reference to DummyClass which should be updated when DummyClass is moved
    auto& owning_class_reference = owning_class.Get();

    // When the DummyClass is move constructed
    DummyClass moved_owning_class{std::move(owning_class)};

    // Then the ReferenceToMoveable retrieved from the original ReferenceToMoveable::Reference should point to the
    // moved-to DummyClass
    auto& reference_wrapper = owning_class_reference.Get();
    EXPECT_EQ(&reference_wrapper, &moved_owning_class);
}

}  // namespace
}  // namespace score::mw::com::impl
