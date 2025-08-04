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
///
/// @file
///
/// @brief Unit tests for OffsetPtr indexing and dereferencing. Does not apply for OffsetPtr<void>.
///
#include "score/memory/shared/offset_ptr.h"
#include "score/memory/shared/test_offset_ptr/offset_ptr_test_resources.h"

#include <gtest/gtest.h>
#include <cstddef>

namespace score::memory::shared::test
{

template <typename T>
using OffsetPtrIndexDereferenceFixture = OffsetPtrNoBoundsCheckingMemoryResourceFixture<T>;
TYPED_TEST_SUITE(OffsetPtrIndexDereferenceFixture, AllMemoryResourceAndNonVoidTypeCombinations, );

TYPED_TEST(OffsetPtrIndexDereferenceFixture, HandlesRegularPtrDereference)
{
    using PointedType = typename TypeParam::second_type::Type;

    const auto initial_value = TypeParam::second_type::CreateDummyValue();
    auto [offset_ptr_wrapper, raw_ptr] =
        OffsetPtrCreator<PointedType>::CreateOffsetPtrInResource(this->memory_resource_, initial_value);
    auto& offset_ptr = offset_ptr_wrapper.get();

    EXPECT_EQ(*offset_ptr, *raw_ptr);
}

TYPED_TEST(OffsetPtrIndexDereferenceFixture, HandlesCustomPtrDereference)
{
    struct A
    {
        A(int first_in, int second_in) : first{first_in}, second{second_in} {}
        int first;
        int second;
    };
    auto [offset_ptr_wrapper, raw_ptr] = OffsetPtrCreator<A>::CreateOffsetPtrInResource(this->memory_resource_, 10, 20);
    auto& offset_ptr = offset_ptr_wrapper.get();

    EXPECT_EQ((*offset_ptr).first, (*raw_ptr).first);
    EXPECT_EQ((*offset_ptr).second, (*raw_ptr).second);

    EXPECT_EQ(offset_ptr->first, raw_ptr->first);
    EXPECT_EQ(offset_ptr->second, raw_ptr->second);
}

TYPED_TEST(OffsetPtrIndexDereferenceFixture, HandlesRegularPtrIndex)
{
    using PointedType = typename TypeParam::second_type::Type;

    const auto initial_value = TypeParam::second_type::CreateDummyValue();
    auto [offset_ptr_wrapper, raw_ptr] =
        OffsetPtrCreator<PointedType>::CreateOffsetPtrInResource(this->memory_resource_, initial_value);
    auto& offset_ptr = offset_ptr_wrapper.get();

    EXPECT_EQ(offset_ptr[0], raw_ptr[0]);
}

TYPED_TEST(OffsetPtrIndexDereferenceFixture, HandlesCustomPtrIndex)
{
    struct A
    {
        A(int first_in, int second_in) : first{first_in}, second{second_in} {}
        int first;
        int second;
    };
    auto [offset_ptr_wrapper, raw_ptr] = OffsetPtrCreator<A>::CreateOffsetPtrInResource(this->memory_resource_, 10, 20);
    auto& offset_ptr = offset_ptr_wrapper.get();

    EXPECT_EQ(offset_ptr[0].first, raw_ptr[0].first);
    EXPECT_EQ(offset_ptr[0].second, raw_ptr[0].second);
}

TYPED_TEST(OffsetPtrIndexDereferenceFixture, HandlesRegularArrayIndex)
{
    constexpr std::size_t arraySize{3};

    auto* raw_ptr = this->memory_resource_.allocate(sizeof(int) * arraySize);
    raw_ptr = new (raw_ptr) int[arraySize]{1, 2, 3};

    auto* const int_ptr = static_cast<int*>(raw_ptr);

    auto* const ptr_to_offset_ptr = this->memory_resource_.template construct<OffsetPtr<int>>(int_ptr);
    auto offset_ptr = *ptr_to_offset_ptr;

    for (size_t i = 0; i < arraySize; ++i)
    {
        EXPECT_EQ(offset_ptr[static_cast<OffsetPtr<int>::difference_type>(i)], int_ptr[i]);
    }
}

TYPED_TEST(OffsetPtrIndexDereferenceFixture, HandlesRegularArrayIndexOnStack)
{
    constexpr std::size_t arraySize{3};

    // The helper code for creating OffsetPtrs uses construct internally which doesn't seem to support creation of
    // arrays. So we create the array manually.
    auto* raw_ptr = this->memory_resource_.allocate(sizeof(int) * arraySize);
    raw_ptr = new (raw_ptr) int[arraySize]{1, 2, 3};

    auto* const int_ptr = static_cast<int*>(raw_ptr);

    OffsetPtr<int> offset_ptr(int_ptr);

    for (size_t i = 0; i < arraySize; ++i)
    {
        EXPECT_EQ(offset_ptr[static_cast<OffsetPtr<int>::difference_type>(i)], int_ptr[i]);
    }
}

TYPED_TEST(OffsetPtrIndexDereferenceFixture, HandlesCustomArrayIndex)
{
    struct A
    {
        A(int first_, int second_) : first(first_), second(second_) {};
        int first;
        int second;
    };
    constexpr std::size_t arraySize{3};

    // The helper code for creating OffsetPtrs uses construct internally which doesn't seem to support creation of
    // arrays. So we create the array manually.
    auto* raw_ptr = this->memory_resource_.allocate(sizeof(int) * arraySize);
    raw_ptr = new (raw_ptr) A[arraySize]{A(1, 1), A(2, 2), A(3, 3)};

    auto* const a_ptr = static_cast<A*>(raw_ptr);

    auto* const ptr_to_offset_ptr = this->memory_resource_.template construct<OffsetPtr<A>>(a_ptr);
    auto offset_ptr = *ptr_to_offset_ptr;

    using difference_type = typename OffsetPtr<A>::difference_type;
    for (size_t i = 0; i < arraySize; ++i)
    {
        EXPECT_EQ(offset_ptr[static_cast<difference_type>(i)].first, a_ptr[i].first);
        EXPECT_EQ(offset_ptr[static_cast<difference_type>(i)].second, a_ptr[i].second);
    }
}

}  // namespace score::memory::shared::test
