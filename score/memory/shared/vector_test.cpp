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
#include "score/memory/shared/vector.h"

#include "score/memory/shared/fake/my_memory_resource.h"

#include "gtest/gtest.h"

namespace score::memory::shared
{
namespace
{

TEST(Vector, OuterVectorAllocatesMemoryOnProvidedResource)
{
    // Given a Vector of Vectors including integers
    test::MyMemoryResource memory{};
    score::memory::shared::Vector<score::memory::shared::Vector<std::uint8_t>> unit{memory};
    auto before_allocating_vector = memory.getAllocatedMemory();

    // When allocating a new inner vector
    unit.resize(1);

    // Then only the memory is allocated that we expect
    EXPECT_EQ(memory.getAllocatedMemory(),
              before_allocating_vector + sizeof(score::memory::shared::Vector<std::uint8_t>));
}

TEST(Vector, InnerVectorAllocatesMemoryOnProvidedResource)
{
    // Given a Vector of Vectors including integers
    test::MyMemoryResource memory{};
    score::memory::shared::Vector<score::memory::shared::Vector<std::uint8_t>> unit{memory};
    unit.resize(1);
    auto before_allocating_integer = memory.getAllocatedMemory();

    // When allocating integer on the inner vector
    unit.at(0).resize(4);

    // Then only the memory is allocated that we expect
    EXPECT_EQ(memory.getAllocatedMemory(), before_allocating_integer + (sizeof(std::uint8_t) * 4));
}

TEST(Vector, PositiveComparisonOfStdVector)
{
    // Given a pmr::vector and a std::vector with the same content
    test::MyMemoryResource memory{};
    score::memory::shared::Vector<std::uint8_t> unit{{1U, 2U, 3U}, memory};
    std::vector<std::uint8_t> other{1U, 2U, 3U};

    // When comparing them
    const bool result = unit == other;

    // Then they are equal
    EXPECT_TRUE(result);
}

TEST(Vector, NegativeComparisonOfStdVector)
{
    // Given a pmr::vector and a std::vector with different content
    test::MyMemoryResource memory{};
    score::memory::shared::Vector<std::uint8_t> unit{{1U, 2U, 3U}, memory};
    std::vector<std::uint8_t> other{1U, 3U, 3U};

    // When comparing them
    const bool result = unit == other;

    // Then they are _not_ equal
    EXPECT_FALSE(result);
}

TEST(Vector, PositiveComparisonOfStdVectorReverse)
{
    // Given a pmr::vector and a std::vector with the same content
    test::MyMemoryResource memory{};
    score::memory::shared::Vector<std::uint8_t> unit{{1U, 2U, 3U}, memory};
    std::vector<std::uint8_t> other{1U, 2U, 3U};

    // When comparing them
    const bool result = other == unit;

    // Then they are equal
    EXPECT_TRUE(result);
}

TEST(Vector, NegativeComparisonOfStdVectorReverse)
{
    // Given a pmr::vector and a std::vector with different content
    test::MyMemoryResource memory{};
    score::memory::shared::Vector<std::uint8_t> unit{{1U, 2U, 3U}, memory};
    std::vector<std::uint8_t> other{1U, 3U, 3U};

    // When comparing them
    const bool result = other == unit;

    // Then they are _not_ equal
    EXPECT_FALSE(result);
}

TEST(Vector, CanConstructWithIterators)
{
    std::vector<std::uint8_t> test{1U, 3U, 3U};
    score::memory::shared::Vector<std::uint8_t> unit(test.cbegin(), test.cend());
}

}  // namespace
}  // namespace score::memory::shared
