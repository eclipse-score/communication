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

#include "score/mw/com/test/methods/semi_dynamic_methods/runtime_sized_array.h"

#include <score/assert_support.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <type_traits>

namespace
{

TEST(RuntimeSizedArrayTest, DefaultConstructorInitializesSizeAndCapacityToZero)
{
    // When default constructing a RuntimeSizedArray
    score::mw::com::test::RuntimeSizedArray<std::int32_t> array{std::allocator<std::int32_t>{}};

    // Then the size and capacity should both be zero
    EXPECT_EQ(array.size(), 0U);
    EXPECT_EQ(array.capacity(), 0U);
}

TEST(RuntimeSizedArrayTest, ReserveOnceSetsCapacityWithoutChangingSize)
{
    // Given a RuntimeSizedArray with default constructor
    score::mw::com::test::RuntimeSizedArray<std::int32_t> array{std::allocator<std::int32_t>{}};

    // When reserving a capacity of 5
    array.ReserveOnce(5U);

    // Then the capacity should be 5 and size should still be zero
    EXPECT_EQ(array.capacity(), 5U);
    EXPECT_EQ(array.size(), 0U);
}

TEST(RuntimeSizedArrayTest, CallingReserveTwiceTerminates)
{
    // Given a RuntimeSizedArray with default constructor
    score::mw::com::test::RuntimeSizedArray<std::int32_t> array{std::allocator<std::int32_t>{}};

    // and given that ReserveOnce has been called once
    array.ReserveOnce(5U);

    // When calling ReserveOnce again
    // Then the program should terminate
    SCORE_LANGUAGE_FUTURECPP_ASSERT_CONTRACT_VIOLATED(array.ReserveOnce(10U));
}

TEST(RuntimeSizedArrayTest, EmplaceBackIncreasesSize)
{
    // Given a RuntimeSizedArray with reserved capacity
    score::mw::com::test::RuntimeSizedArray<std::int32_t> array{std::allocator<std::int32_t>{}};
    array.ReserveOnce(3U);

    // When emplacing back an element
    array.emplace_back(42);

    // Then the size should increase to 1 and the capacity should remain 3
    EXPECT_EQ(array.size(), 1U);
    EXPECT_EQ(array.capacity(), 3U);
    EXPECT_EQ(array.at(0), 42);
}

TEST(RuntimeSizedArrayTest, EmplaceBackBeyondCapacityTerminates)
{
    // Given a RuntimeSizedArray with reserved capacity of 2
    score::mw::com::test::RuntimeSizedArray<std::int32_t> array{std::allocator<std::int32_t>{}};
    array.ReserveOnce(2U);

    // and given that two elements have been emplaced
    array.emplace_back(1);
    array.emplace_back(2);

    // When trying to emplace back a third element
    // Then the program should terminate
    SCORE_LANGUAGE_FUTURECPP_ASSERT_CONTRACT_VIOLATED(array.emplace_back(3));
}

}  // namespace
