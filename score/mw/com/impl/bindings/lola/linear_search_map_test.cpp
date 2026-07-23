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
#include "score/mw/com/impl/bindings/lola/linear_search_map.h"

#include "score/memory/shared/new_delete_delegate_resource.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <tuple>
#include <utility>

namespace score::mw::com::impl::lola
{
namespace
{

const std::uint64_t kMemoryResourceId{42U};

using TestMap = LinearSearchMap<std::int32_t, std::int32_t>;

class LinearSearchMapFixture : public ::testing::Test
{
  protected:
    memory::shared::NewDeleteDelegateMemoryResource memory_{kMemoryResourceId};
};

TEST_F(LinearSearchMapFixture, IsEmptyAfterConstruction)
{
    TestMap unit{4U, memory_};

    EXPECT_TRUE(unit.empty());
    EXPECT_EQ(unit.size(), 0U);
    EXPECT_EQ(unit.capacity(), 4U);
    EXPECT_EQ(unit.begin(), unit.end());
}

TEST_F(LinearSearchMapFixture, EmplaceInsertsElement)
{
    TestMap unit{4U, memory_};

    const auto result = unit.emplace(1, 100);

    EXPECT_TRUE(result.second);
    ASSERT_NE(result.first, unit.end());
    EXPECT_EQ(result.first->first, 1);
    EXPECT_EQ(result.first->second, 100);
    EXPECT_EQ(unit.size(), 1U);
    EXPECT_FALSE(unit.empty());
}

TEST_F(LinearSearchMapFixture, EmplaceWithPiecewiseConstructInsertsElement)
{
    TestMap unit{4U, memory_};

    const auto result = unit.emplace(std::piecewise_construct, std::forward_as_tuple(7), std::forward_as_tuple(700));

    EXPECT_TRUE(result.second);
    ASSERT_NE(result.first, unit.end());
    EXPECT_EQ(result.first->first, 7);
    EXPECT_EQ(result.first->second, 700);
}

TEST_F(LinearSearchMapFixture, EmplaceWithDuplicateKeyDoesNotInsert)
{
    TestMap unit{4U, memory_};

    std::ignore = unit.emplace(1, 100);
    const auto result = unit.emplace(1, 999);

    EXPECT_FALSE(result.second);
    EXPECT_EQ(unit.size(), 1U);
    // The originally inserted value must be preserved.
    EXPECT_EQ(result.first->second, 100);
}

TEST_F(LinearSearchMapFixture, FindReturnsInsertedElement)
{
    TestMap unit{4U, memory_};
    std::ignore = unit.emplace(1, 100);
    std::ignore = unit.emplace(2, 200);
    std::ignore = unit.emplace(3, 300);

    const auto it = unit.find(2);

    ASSERT_NE(it, unit.end());
    EXPECT_EQ(it->second, 200);
}

TEST_F(LinearSearchMapFixture, FindReturnsEndForMissingKey)
{
    TestMap unit{4U, memory_};
    std::ignore = unit.emplace(1, 100);

    EXPECT_EQ(unit.find(99), unit.end());
}

TEST_F(LinearSearchMapFixture, ConstFindWorks)
{
    TestMap unit{4U, memory_};
    std::ignore = unit.emplace(5, 500);

    const TestMap& const_unit = unit;
    const auto it = const_unit.find(5);

    ASSERT_NE(it, const_unit.cend());
    EXPECT_EQ(it->second, 500);
    EXPECT_EQ(const_unit.find(6), const_unit.cend());
}

TEST_F(LinearSearchMapFixture, AtReturnsMappedValue)
{
    TestMap unit{4U, memory_};
    std::ignore = unit.emplace(8, 800);

    EXPECT_EQ(unit.at(8), 800);

    unit.at(8) = 801;
    EXPECT_EQ(unit.at(8), 801);
}

TEST_F(LinearSearchMapFixture, IterationVisitsAllElements)
{
    TestMap unit{4U, memory_};
    std::ignore = unit.emplace(1, 10);
    std::ignore = unit.emplace(2, 20);

    std::int32_t sum_keys{0};
    std::int32_t sum_values{0};
    for (const auto& element : unit)
    {
        sum_keys += element.first;
        sum_values += element.second;
    }

    EXPECT_EQ(sum_keys, 3);
    EXPECT_EQ(sum_values, 30);
}

}  // namespace
}  // namespace score::mw::com::impl::lola
