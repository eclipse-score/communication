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
#include "score/memory/shared/map.h"

#include "score/memory/shared/fake/my_memory_resource.h"
#include "score/memory/shared/vector.h"

#include "gtest/gtest.h"

namespace score::memory::shared
{
namespace
{

TEST(Map, AllocatesMemoryOnProvidedResource)
{
    // Given a Map of int to int, ensure that the Map uses the allocator to get its memory
    test::MyMemoryResource memory{};
    Map<int, int> unit{memory};
    auto before_allocating_vector = memory.getAllocatedMemory();

    // When inserting an element
    unit[1] = 1;

    // ...we expect memory to be allocated from our allocator, and more than "needed" since Map also needs memory for
    // its bookkeeping
    EXPECT_GT(memory.getAllocatedMemory(), before_allocating_vector + sizeof(int));
}

TEST(Map, InnerVectorAllocatesMemoryOnProvidedResource)
{
    // Given a Map of a Vector
    test::MyMemoryResource memory{};
    Map<int, Vector<std::uint8_t>> unit{memory};
    auto before_allocating_vector = memory.getAllocatedMemory();

    // When constructing a Vector within the Map
    unit.emplace(std::piecewise_construct, std::forward_as_tuple(1), std::forward_as_tuple(64, 42));

    // Then the memory is allocated on the memory resource that was provided to the map (seamless passing of allocator)
    EXPECT_GT(memory.getAllocatedMemory(), before_allocating_vector + (sizeof(std::uint8_t) * 64));
}

TEST(Map, InnerVectorAllocatesMemoryOnProvidedResourceByDefaultConstruction)
{
    // Given a Map of a Vector
    test::MyMemoryResource memory{};
    Map<int, Vector<std::uint8_t>> unit{memory};
    auto before_allocating_vector = memory.getAllocatedMemory();

    // When default constructing the Vector
    unit[1].resize(128);

    // Then the memory is allocated on the memory resource that was provided to the map (seamless passing of allocator)
    EXPECT_GT(memory.getAllocatedMemory(), before_allocating_vector + (sizeof(std::uint8_t) * 128));
}

TEST(Map, UserConstructedVectorCanBeUsed)
{
    // Given a Map of a Vector
    test::MyMemoryResource memory{};
    Map<int, Vector<std::uint8_t>> unit{memory};
    auto before_allocating_vector = memory.getAllocatedMemory();

    // When the user constructs a Vector by using the same allocator
    unit[1] = Vector<std::uint8_t>(128, 42, unit.get_allocator());

    // Then the memory is allocated on the memory resource that was provided to the map (seamless passing of allocator)
    EXPECT_GT(memory.getAllocatedMemory(), before_allocating_vector + (sizeof(std::uint8_t) * 128));
}

TEST(Map, MapInMapAllocatesMemoryFromCorrectRessource)
{
    // Given a Map of a Map
    test::MyMemoryResource memory{};
    Map<int, Map<int, int>> unit{memory};
    auto before_allocating_vector = memory.getAllocatedMemory();

    // When implicit constructing the inner map
    unit[1][2] = 3;

    // Then the memory is allocated on the memory resource that was provided to the map (seamless passing of allocator)
    EXPECT_GT(memory.getAllocatedMemory(), before_allocating_vector + (sizeof(std::uint8_t) * 2));
}

}  // namespace
}  // namespace score::memory::shared
