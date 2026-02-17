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
#include "polymorphic_offset_ptr_allocator.h"

#include "fake/my_memory_resource.h"

#include "gtest/gtest.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace score::memory::shared::test
{

TEST(PolymorphicOffsetPtrAllocator, IsDefaultConstructible)
{
    PolymorphicOffsetPtrAllocator<> unit{};
}

TEST(PolymorphicOffsetPtrAllocator, AllocatesAndDeallocatesMemory)
{
    MyMemoryResource resource{};

    std::vector<std::uint64_t, PolymorphicOffsetPtrAllocator<std::uint64_t>> unit(resource);
    unit.emplace_back(42U);
    unit.emplace_back(43U);
}

TEST(PolymorphicOffsetPtrAllocator, SupportsRebinding)
{
    MyMemoryResource resource{};

    std::unordered_map<std::uint64_t,
                       std::uint64_t,
                       std::hash<std::uint64_t>,
                       std::equal_to<std::uint64_t>,
                       PolymorphicOffsetPtrAllocator<std::pair<const std::uint64_t, std::uint64_t>>>
        unit(resource);

    unit.insert({42U, 0U});
}

TEST(PolymorphicOffsetPtrAllocator, AllocatorsWithNullPtrMemoryResourceProxiesComparisonOperators)
{
    PolymorphicOffsetPtrAllocator<std::uint64_t> allocator1;
    PolymorphicOffsetPtrAllocator<std::uint64_t> allocator2;

    EXPECT_TRUE(allocator1 == allocator2);
    EXPECT_TRUE(allocator2 == allocator1);
    EXPECT_FALSE(allocator1 != allocator2);
    EXPECT_FALSE(allocator2 != allocator1);
}

}  // namespace score::memory::shared::test
