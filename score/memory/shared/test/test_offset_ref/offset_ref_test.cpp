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
#include "score/memory/shared/offset_ref.h"

#include "score/memory/shared/fake/my_bounded_memory_resource.h"

#include <gtest/gtest.h>
#include <cstddef>
#include <type_traits>

namespace score::memory::shared::test
{
namespace
{

class OffsetRefTest : public ::testing::Test
{
  public:
    MyBoundedMemoryResource memory_resource_{};
};

TEST_F(OffsetRefTest, ConstructionFromStackReferenceStoresAddress)
{
    // Given a plain (stack) object
    int value{5};

    // When constructing an OffsetRef referencing it
    OffsetRef<int> ref{value};

    // Then get() returns a reference to the very same object
    EXPECT_EQ(&ref.get(), &value);
    EXPECT_EQ(ref.get(), 5);
}

TEST_F(OffsetRefTest, ConstructionFromObjectInSharedMemoryStoresAddress)
{
    // Given an object constructed within a (fake) shared-memory resource
    auto* const value = memory_resource_.construct<int>(42);

    // When constructing an OffsetRef referencing it (also placed within the same shared-memory resource, so that
    // OffsetPtr's bounds-checking is active)
    auto* const ref = memory_resource_.construct<OffsetRef<int>>(*value);

    // Then get() returns a reference to the very same object
    EXPECT_EQ(&ref->get(), value);
    EXPECT_EQ(ref->get(), 42);
}

TEST_F(OffsetRefTest, ConstructionSupportsReferencingAnObjectLocatedBeforeItInMemory)
{
    // Given a standard-layout struct whose OffsetRef member references a sibling member declared (and thus located)
    // before it. This results in a negative offset_, which must be a perfectly valid (non-overlapping) case.
    struct Container
    {
        int value{42};
        OffsetRef<int> ref{value};
    };
    Container container{};

    // Then get() returns a reference to the sibling member
    EXPECT_EQ(&container.ref.get(), &container.value);
    EXPECT_EQ(container.ref.get(), 42);
}

using OffsetRefDeathTest = OffsetRefTest;

TEST_F(OffsetRefDeathTest, ConstructionTerminatesWhenReferencedObjectFullyOverlapsWithOffsetRef)
{
    // Given a buffer large enough to hold an OffsetRef<int>, with an int object placed at its very start
    alignas(alignof(OffsetRef<int>)) std::byte storage[sizeof(OffsetRef<int>)];
    auto* const referenced_object = new (static_cast<void*>(storage)) int{42};

    // When constructing an OffsetRef at the very same address (full overlap, offset_ == 0)
    // Then the program terminates, since the OffsetRef would corrupt the very object it references
    EXPECT_DEATH(new (static_cast<void*>(storage)) OffsetRef<int>(*referenced_object), ".*");
}

TEST_F(OffsetRefDeathTest, ConstructionTerminatesWhenReferencedObjectPartiallyOverlapsWithOffsetRef)
{
    // Given a buffer large enough to hold both an OffsetRef<int> and an int, where the int partially overlaps with
    // where the OffsetRef will be placed (referenced object starts before the OffsetRef's end address)
    alignas(alignof(std::max_align_t)) std::byte storage[2 * sizeof(OffsetRef<int>)];
    auto* const offset_ref_address = storage;
    auto* const overlapping_int_address = storage + (sizeof(OffsetRef<int>) / 2);
    auto* const referenced_object = new (static_cast<void*>(overlapping_int_address)) int{42};

    // When constructing an OffsetRef at offset_ref_address referencing the overlapping int
    // Then the program terminates
    EXPECT_DEATH(new (static_cast<void*>(offset_ref_address)) OffsetRef<int>(*referenced_object), ".*");
}

TEST_F(OffsetRefTest, ImplicitConversionToReferenceYieldsSameObject)
{
    // Given an OffsetRef referencing a stack object
    int value{5};
    OffsetRef<int> ref{value};

    // When implicitly converting it to a native reference
    int& converted = ref;

    // Then it references the same object
    EXPECT_EQ(&converted, &value);

    // And mutating through the converted reference is visible through the original object
    converted = 10;
    EXPECT_EQ(value, 10);
}

TEST_F(OffsetRefTest, ImplicitConversionSupportsConstT)
{
    // Given an OffsetRef<const T> referencing a const object
    const int value{5};
    OffsetRef<const int> ref{value};

    // When implicitly converting it to a native const reference
    const int& converted = ref;

    // Then it references the same object
    EXPECT_EQ(&converted, &value);
    EXPECT_EQ(converted, 5);
}

TEST_F(OffsetRefTest, GetSupportsConstT)
{
    const int value{7};
    OffsetRef<const int> ref{value};

    EXPECT_EQ(&ref.get(), &value);
    EXPECT_EQ(ref.get(), 7);
}

TEST_F(OffsetRefTest, AssignmentOperatorAssignsThroughToReferencedObject)
{
    // Given an OffsetRef referencing a stack object
    int value{5};
    OffsetRef<int> ref{value};

    // When assigning a new value via OffsetRef's assignment operator
    ref = 10;

    // Then the referenced object is updated (OffsetRef itself is not re-bound)
    EXPECT_EQ(value, 10);
    EXPECT_EQ(&ref.get(), &value);
}

TEST_F(OffsetRefTest, EqualityComparesReferencedValues)
{
    int value_a{5};
    int value_b{5};
    OffsetRef<int> ref_a{value_a};
    OffsetRef<int> ref_b{value_b};

    // Different objects but equal values compare equal
    EXPECT_TRUE(ref_a == ref_b);
    EXPECT_FALSE(ref_a != ref_b);

    value_b = 6;
    EXPECT_FALSE(ref_a == ref_b);
    EXPECT_TRUE(ref_a != ref_b);
}

TEST_F(OffsetRefTest, EqualityComparesSameObjectAsEqual)
{
    int value{5};
    OffsetRef<int> ref_a{value};
    OffsetRef<int> ref_b{value};

    EXPECT_TRUE(ref_a == ref_b);
}

// OffsetRef must behave like a native C++ reference: it can never be re-bound, default-constructed or left
// unbound. These properties are enforced purely at compile-time.
static_assert(!std::is_copy_constructible<OffsetRef<int>>::value, "OffsetRef must not be copy-constructible");
static_assert(!std::is_copy_assignable<OffsetRef<int>>::value,
              "OffsetRef must not be copy-assignable (it would imply re-binding)");
static_assert(!std::is_move_constructible<OffsetRef<int>>::value, "OffsetRef must not be move-constructible");
static_assert(!std::is_move_assignable<OffsetRef<int>>::value,
              "OffsetRef must not be move-assignable (it would imply re-binding)");
static_assert(!std::is_default_constructible<OffsetRef<int>>::value,
              "OffsetRef must not be default-constructible (it must always reference a valid object)");

}  // namespace
}  // namespace score::memory::shared::test
