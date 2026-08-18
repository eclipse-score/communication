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

/// \file
/// \brief These tests verify that score::containers::DynamicArray and score::containers::NonRelocatableVector
/// remain compatible with the production score::memory::shared memory-resource / allocator types
/// (ManagedMemoryResource-derived resources and PolymorphicOffsetPtrAllocator), even though the containers'
/// own unit tests use generic test doubles instead of these production types. If any of these tests
/// break, it indicates that a change made to score/containers has broken the ability of the containers to
/// be used together with real, shared-memory-capable allocators.

#include "score/containers/dynamic_array.h"
#include "score/containers/non_relocatable_vector.h"

#include "score/memory/shared/fake/my_bounded_memory_resource.h"
#include "score/memory/shared/fake/my_memory_resource.h"
#include "score/memory/shared/polymorphic_offset_ptr_allocator.h"

#include "gtest/gtest.h"

#include <score/utility.hpp>

#include <cstdint>
#include <type_traits>
#include <utility>

namespace score::memory::shared::test
{
namespace
{

using score::containers::DynamicArray;
using score::containers::NonRelocatableVector;

TEST(ContainersCompatibility, DynamicArrayCanBeConstructedWithPolymorphicOffsetPtrAllocator)
{
    // Given a memory resource and a PolymorphicOffsetPtrAllocator built on top of it
    MyMemoryResource resource{};
    PolymorphicOffsetPtrAllocator<std::uint64_t> allocator{resource};

    // When constructing a DynamicArray with that allocator and assigning values into it
    DynamicArray<std::uint64_t, PolymorphicOffsetPtrAllocator<std::uint64_t>> unit{5U, allocator};
    ASSERT_EQ(unit.size(), 5U);
    for (std::size_t i = 0U; i < unit.size(); ++i)
    {
        unit[i] = i;
    }

    // Then the array holds the assigned values and memory was allocated on the provided resource
    for (std::size_t i = 0U; i < unit.size(); ++i)
    {
        EXPECT_EQ(unit[i], i);
    }
    EXPECT_GT(resource.getAllocatedMemory(), 0U);
}

TEST(ContainersCompatibility, DynamicArrayCanBeConstructedWithInitialValueAndPolymorphicOffsetPtrAllocator)
{
    // Given a memory resource and a PolymorphicOffsetPtrAllocator built on top of it
    MyMemoryResource resource{};
    PolymorphicOffsetPtrAllocator<std::uint64_t> allocator{resource};

    // When constructing a DynamicArray with that allocator and an initial value
    DynamicArray<std::uint64_t, PolymorphicOffsetPtrAllocator<std::uint64_t>> unit{3U, 42U, allocator};

    // Then every element of the array is initialized to the given value and memory was allocated on the provided
    // resource
    ASSERT_EQ(unit.size(), 3U);
    for (const auto& value : unit)
    {
        EXPECT_EQ(value, 42U);
    }
    EXPECT_GT(resource.getAllocatedMemory(), 0U);
}

TEST(ContainersCompatibility, NonRelocatableVectorCanBeConstructedWithPolymorphicOffsetPtrAllocator)
{
    // Given a memory resource and a PolymorphicOffsetPtrAllocator built on top of it
    MyMemoryResource resource{};
    PolymorphicOffsetPtrAllocator<std::uint64_t> allocator{resource};
    NonRelocatableVector<std::uint64_t, PolymorphicOffsetPtrAllocator<std::uint64_t>> unit{5U, allocator};

    // When emplacing elements into the NonRelocatableVector
    unit.emplace_back(42U);
    unit.emplace_back(43U);

    // Then the vector holds the emplaced elements, its capacity remains as constructed, and memory was allocated on
    // the provided resource
    ASSERT_EQ(unit.size(), 2U);
    EXPECT_EQ(unit.capacity(), 5U);
    EXPECT_EQ(unit[0], 42U);
    EXPECT_EQ(unit[1], 43U);
    EXPECT_GT(resource.getAllocatedMemory(), 0U);
}

TEST(ContainersCompatibility, NonRelocatableVectorCanBeCopyConstructedWithPolymorphicOffsetPtrAllocator)
{
    // Given a NonRelocatableVector, using a PolymorphicOffsetPtrAllocator, populated with two elements
    MyMemoryResource resource{};
    PolymorphicOffsetPtrAllocator<std::uint64_t> allocator{resource};
    NonRelocatableVector<std::uint64_t, PolymorphicOffsetPtrAllocator<std::uint64_t>> unit{5U, allocator};
    unit.emplace_back(42U);
    unit.emplace_back(43U);

    // When copy-constructing a new vector from it
    NonRelocatableVector<std::uint64_t, PolymorphicOffsetPtrAllocator<std::uint64_t>> copy{unit};

    // Then the copy contains the same elements as the original
    ASSERT_EQ(copy.size(), 2U);
    EXPECT_EQ(copy[0], 42U);
    EXPECT_EQ(copy[1], 43U);
}

TEST(ContainersCompatibility, NonRelocatableVectorCanBeMoveConstructedWithPolymorphicOffsetPtrAllocator)
{
    // Given a NonRelocatableVector, using a PolymorphicOffsetPtrAllocator, populated with two elements
    MyMemoryResource resource{};
    PolymorphicOffsetPtrAllocator<std::uint64_t> allocator{resource};
    NonRelocatableVector<std::uint64_t, PolymorphicOffsetPtrAllocator<std::uint64_t>> unit{5U, allocator};
    unit.emplace_back(42U);
    unit.emplace_back(43U);

    // When move-constructing a new vector from it
    NonRelocatableVector<std::uint64_t, PolymorphicOffsetPtrAllocator<std::uint64_t>> moved{std::move(unit)};

    // Then the moved-to vector contains the same elements as the original
    ASSERT_EQ(moved.size(), 2U);
    EXPECT_EQ(moved[0], 42U);
    EXPECT_EQ(moved[1], 43U);
}

TEST(ContainersCompatibility, NonRelocatableVectorCanBeMoveAssignedWithPolymorphicOffsetPtrAllocator)
{
    // Given two NonRelocatableVectors, using a PolymorphicOffsetPtrAllocator, one populated with two elements and
    // the other default-constructed with capacity for a single element
    // Note: copy assignment is not tested here since NonRelocatableVector's copy assignment operator is deleted.
    using UnitType = NonRelocatableVector<std::uint64_t, PolymorphicOffsetPtrAllocator<std::uint64_t>>;
    static_assert(!std::is_copy_assignable_v<UnitType>, "NonRelocatableVector is expected to not be copy-assignable");
    MyMemoryResource resource{};
    PolymorphicOffsetPtrAllocator<std::uint64_t> allocator{resource};
    NonRelocatableVector<std::uint64_t, PolymorphicOffsetPtrAllocator<std::uint64_t>> unit{5U, allocator};
    unit.emplace_back(42U);
    unit.emplace_back(43U);
    NonRelocatableVector<std::uint64_t, PolymorphicOffsetPtrAllocator<std::uint64_t>> assigned{1U, allocator};

    // When move-assigning the populated vector into the other one
    assigned = std::move(unit);

    // Then the move-assigned-to vector contains the same elements as the original
    ASSERT_EQ(assigned.size(), 2U);
    EXPECT_EQ(assigned[0], 42U);
    EXPECT_EQ(assigned[1], 43U);
}

TEST(ContainersCompatibility, NonRelocatableVectorWithBoundedMemoryResourceDeallocatesOnDestruction)
{
    // Given a bounded memory resource (used here, rather than MyMemoryResource, because it additionally tracks
    // deallocated bytes via GetUserDeAllocatedBytes(), which this test relies on) and a NonRelocatableVector, using
    // a PolymorphicOffsetPtrAllocator, populated with one element, and given that memory has been allocated but
    // nothing has been deallocated yet
    MyBoundedMemoryResource resource{2000U};
    {
        PolymorphicOffsetPtrAllocator<std::uint64_t> allocator{resource};
        NonRelocatableVector<std::uint64_t, PolymorphicOffsetPtrAllocator<std::uint64_t>> unit{5U, allocator};
        unit.emplace_back(42U);
        ASSERT_GT(resource.GetUserAllocatedBytes(), 0U);
        ASSERT_EQ(resource.GetUserDeAllocatedBytes(), 0U);
    }
    // When the vector goes out of scope and is destroyed
    // Then all allocated memory has been deallocated
    EXPECT_EQ(resource.GetUserAllocatedBytes(), resource.GetUserDeAllocatedBytes());
}

TEST(ContainersCompatibility, DynamicArrayWithBoundedMemoryResourceDeallocatesOnDestruction)
{
    // Given a bounded memory resource and a DynamicArray constructed using a PolymorphicOffsetPtrAllocator, and
    // given that memory has been allocated but nothing has been deallocated yet
    MyBoundedMemoryResource resource{2000U};
    {
        PolymorphicOffsetPtrAllocator<std::uint64_t> allocator{resource};
        DynamicArray<std::uint64_t, PolymorphicOffsetPtrAllocator<std::uint64_t>> unit{5U, allocator};
        score::cpp::ignore = unit;
        ASSERT_GT(resource.GetUserAllocatedBytes(), 0U);
        ASSERT_EQ(resource.GetUserDeAllocatedBytes(), 0U);
    }
    // When the array goes out of scope and is destroyed
    // Then all allocated memory has been deallocated
    EXPECT_EQ(resource.GetUserAllocatedBytes(), resource.GetUserDeAllocatedBytes());
}

}  // namespace
}  // namespace score::memory::shared::test
