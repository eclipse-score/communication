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
/// @brief Unit tests for OffsetPtr arithmetic. Does not apply for OffsetPtr<void> or
///        an OffsetPtr initialised with a nullptr.
///
#include "score/memory/shared/offset_ptr.h"
#include "score/memory/shared/test_offset_ptr/offset_ptr_test_resources.h"

#include <score/assert.hpp>
#include <score/assert_support.hpp>
#include <score/utility.hpp>

#include <gtest/gtest.h>
#include <cstddef>
#include <utility>

namespace score::memory::shared::test
{

template <typename T>
using OffsetPtrArithmeticFixture = OffsetPtrNoBoundsCheckingMemoryResourceFixture<T>;
TYPED_TEST_SUITE(OffsetPtrArithmeticFixture, AllMemoryResourceAndNonVoidTypeCombinations, );

template <typename T>
using OffsetPtrArithmeticComplexTypeFixture = OffsetPtrNoBoundsCheckingMemoryResourceFixture<T>;
using AllMemoryResourceAndComplexTypeCombinations =
    ::testing::Types<std::pair<UseRegisteredMemoryResource, ComplexType>,
                     std::pair<UseRegisteredMemoryResource, ConstComplexType>,
                     std::pair<UseUnregisteredMemoryResource, ComplexType>,
                     std::pair<UseUnregisteredMemoryResource, ConstComplexType>>;
TYPED_TEST_SUITE(OffsetPtrArithmeticComplexTypeFixture, AllMemoryResourceAndComplexTypeCombinations, );

TYPED_TEST(OffsetPtrArithmeticFixture, OffsetPtrArithmeticMatchesRawPtrArithmetic)
{
    using PointedType = typename TypeParam::second_type::Type;

    auto [offset_ptr_wrapper, raw_ptr] =
        OffsetPtrCreator<PointedType>::CreateOffsetPtrInResource(this->memory_resource_);
    auto& offset_ptr = offset_ptr_wrapper.get();

    const typename OffsetPtr<PointedType>::difference_type offset{5};

    ASSERT_EQ((offset_ptr + offset).get(), raw_ptr + offset);
    ASSERT_EQ((offset + offset_ptr).get(), raw_ptr + offset);
    ASSERT_EQ((offset_ptr - offset).get(), raw_ptr - offset);
    ASSERT_EQ((offset - offset_ptr).get(), raw_ptr - offset);
}

TYPED_TEST(OffsetPtrArithmeticFixture, OffsetPtrSubtractionMatchesRawPtrSubtraction)
{
    // In general, subtracting OffsetPtrs only makes sense in the context of OffsetPtrs pointing into an array.
    // Therefore, we create an array of PointedType and perform arithmetic on 2 OffsetPtrs pointed to the first and last
    // elements of the array.
    using PointedType = typename TypeParam::second_type::Type;

    constexpr std::size_t array_size{3U};
    constexpr std::size_t number_of_bytes_to_allocate{array_size * sizeof(PointedType)};
    constexpr auto pointed_type_alignment = alignof(PointedType);

    void* const memory = this->memory_resource_.allocate(number_of_bytes_to_allocate, pointed_type_alignment);
    auto* const pointed_type_memory = new (memory) PointedType[array_size]();

    auto* const start_ptr = pointed_type_memory;
    auto* const last_ptr = pointed_type_memory;

    auto* const ptr_to_start_offset_ptr = this->memory_resource_.template construct<OffsetPtr<PointedType>>(start_ptr);
    auto* const ptr_to_last_offset_ptr = this->memory_resource_.template construct<OffsetPtr<PointedType>>(last_ptr);

    ASSERT_EQ(*ptr_to_start_offset_ptr - *ptr_to_last_offset_ptr, start_ptr - last_ptr);
    ASSERT_EQ(*ptr_to_last_offset_ptr - *ptr_to_start_offset_ptr, last_ptr - start_ptr);
    ASSERT_EQ(*ptr_to_start_offset_ptr - last_ptr, start_ptr - last_ptr);
    ASSERT_EQ(start_ptr - *ptr_to_last_offset_ptr, start_ptr - last_ptr);
    this->memory_resource_.deallocate(memory, number_of_bytes_to_allocate, pointed_type_alignment);
}

TYPED_TEST(OffsetPtrArithmeticFixture, OffsetPtrPostIncrementMatchesRawPtrPostIncrement)
{
    using PointedType = typename TypeParam::second_type::Type;

    auto [offset_ptr_wrapper, raw_ptr] =
        OffsetPtrCreator<PointedType>::CreateOffsetPtrInResource(this->memory_resource_);
    auto& offset_ptr = offset_ptr_wrapper.get();

    // Test post increment
    ASSERT_EQ((offset_ptr++).get(), raw_ptr);
    ASSERT_EQ(offset_ptr.get(), raw_ptr + 1);
}

TYPED_TEST(OffsetPtrArithmeticFixture, OffsetPtrPreIncrementMatchesRawPtrPreIncrement)
{
    using PointedType = typename TypeParam::second_type::Type;

    auto [offset_ptr_wrapper, raw_ptr] =
        OffsetPtrCreator<PointedType>::CreateOffsetPtrInResource(this->memory_resource_);
    auto& offset_ptr = offset_ptr_wrapper.get();

    // Test pre increment
    ASSERT_EQ((++offset_ptr).get(), raw_ptr + 1);
}

TYPED_TEST(OffsetPtrArithmeticFixture, OffsetPtrPostDecrementMatchesRawPtrPostDecrement)
{
    using PointedType = typename TypeParam::second_type::Type;

    // Allocate 50 bytes in the memory region so that when we decrement offset_ptr, the resulting pointer still lies
    // within the memory region
    score::cpp::ignore = this->memory_resource_.allocate(50U);

    auto [offset_ptr_wrapper, raw_ptr] =
        OffsetPtrCreator<PointedType>::CreateOffsetPtrInResource(this->memory_resource_);
    auto& offset_ptr = offset_ptr_wrapper.get();

    // Test post decrement
    ASSERT_EQ((offset_ptr--).get(), raw_ptr);
    ASSERT_EQ(offset_ptr.get(), raw_ptr - 1);
}

TYPED_TEST(OffsetPtrArithmeticFixture, OffsetPtrPreDecrementMatchesRawPtrPreDecrement)
{
    using PointedType = typename TypeParam::second_type::Type;

    // Allocate 50 bytes in the memory region so that when we decrement offset_ptr, the resulting pointer still lies
    // within the memory region
    score::cpp::ignore = this->memory_resource_.allocate(50U);

    auto [offset_ptr_wrapper, raw_ptr] =
        OffsetPtrCreator<PointedType>::CreateOffsetPtrInResource(this->memory_resource_);
    auto& offset_ptr = offset_ptr_wrapper.get();

    // Test pre decrement
    ASSERT_EQ((--offset_ptr).get(), raw_ptr - 1);
}

TYPED_TEST(OffsetPtrArithmeticFixture, OffsetPtrPlusEqualsWithPositiveValueMatchesRawPtrPlusEquals)
{
    using PointedType = typename TypeParam::second_type::Type;

    auto [offset_ptr_wrapper, raw_ptr] =
        OffsetPtrCreator<PointedType>::CreateOffsetPtrInResource(this->memory_resource_);
    auto& offset_ptr = offset_ptr_wrapper.get();

    // Test plus equals
    offset_ptr += 2;
    raw_ptr += 2;

    EXPECT_EQ(offset_ptr, raw_ptr);
}

TYPED_TEST(OffsetPtrArithmeticFixture, OffsetPtrPlusEqualsWithNegativeValueMatchesRawPtrPlusEquals)
{
    using PointedType = typename TypeParam::second_type::Type;

    // Allocate 50 bytes in the memory region so that when we decrement offset_ptr, the resulting pointer still lies
    // within the memory region
    score::cpp::ignore = this->memory_resource_.allocate(50U);

    auto [offset_ptr_wrapper, raw_ptr] =
        OffsetPtrCreator<PointedType>::CreateOffsetPtrInResource(this->memory_resource_);
    auto& offset_ptr = offset_ptr_wrapper.get();

    // Test plus equals
    offset_ptr += -2;
    raw_ptr += -2;

    EXPECT_EQ(offset_ptr, raw_ptr);
}

TYPED_TEST(OffsetPtrArithmeticFixture, OffsetPtrPlusEqualsWithZeroMatchesRawPtrPlusEquals)
{
    using PointedType = typename TypeParam::second_type::Type;

    auto [offset_ptr_wrapper, raw_ptr] =
        OffsetPtrCreator<PointedType>::CreateOffsetPtrInResource(this->memory_resource_);
    auto& offset_ptr = offset_ptr_wrapper.get();

    // Test plus equals
    offset_ptr += 0;
    raw_ptr += 0;

    EXPECT_EQ(offset_ptr, raw_ptr);
}

TYPED_TEST(OffsetPtrArithmeticFixture, OffsetPtrMinusEqualsWithPositiveValueMatchesRawPtrMinusEquals)
{
    using PointedType = typename TypeParam::second_type::Type;

    // Allocate 50 bytes in the memory region so that when we decrement offset_ptr, the resulting pointer still lies
    // within the memory region
    score::cpp::ignore = this->memory_resource_.allocate(50U);

    auto [offset_ptr_wrapper, raw_ptr] =
        OffsetPtrCreator<PointedType>::CreateOffsetPtrInResource(this->memory_resource_);
    auto& offset_ptr = offset_ptr_wrapper.get();

    // Test minus equals
    offset_ptr -= 2;
    raw_ptr -= 2;

    EXPECT_EQ(offset_ptr, raw_ptr);
}

TYPED_TEST(OffsetPtrArithmeticFixture, OffsetPtrMinusEqualsWithNegativeValueMatchesRawPtrMinusEquals)
{
    using PointedType = typename TypeParam::second_type::Type;

    auto [offset_ptr_wrapper, raw_ptr] =
        OffsetPtrCreator<PointedType>::CreateOffsetPtrInResource(this->memory_resource_);
    auto& offset_ptr = offset_ptr_wrapper.get();

    // Test minus equals
    offset_ptr -= -2;
    raw_ptr -= -2;

    EXPECT_EQ(offset_ptr, raw_ptr);
}

TYPED_TEST(OffsetPtrArithmeticFixture, OffsetPtrMinusEqualsWithZeroMatchesRawPtrMinusEquals)
{
    using PointedType = typename TypeParam::second_type::Type;

    auto [offset_ptr_wrapper, raw_ptr] =
        OffsetPtrCreator<PointedType>::CreateOffsetPtrInResource(this->memory_resource_);
    auto& offset_ptr = offset_ptr_wrapper.get();

    // Test minus equals
    offset_ptr -= 0;
    raw_ptr -= 0;

    EXPECT_EQ(offset_ptr, raw_ptr);
}

TYPED_TEST(OffsetPtrArithmeticComplexTypeFixture, OffsetPtrSubtractionBetweenElementsIncorrectlyAlignedTerminates)
{
    using PointedType = typename TypeParam::second_type::Type;

    constexpr auto pointed_type_size{sizeof(PointedType)};

    // Given 2 OffsetPtrs in which the offset between the pointed-to elements is NOT a multiple of
    // sizeof(PointedType)

    // Allocate first pointed-to object
    auto* const raw_ptr_0 = this->memory_resource_.template construct<PointedType>();

    // Allocate some memory that will result in the next next allocation of PointedType not being allocated as a
    // multiple of sizeof(PointedType).
    this->memory_resource_.allocate(pointed_type_size / 2U);

    // Allocate second pointed-to object
    auto* const raw_ptr_1 = this->memory_resource_.template construct<PointedType>();

    // Allocate OffetPtrs for each pointed-to object
    auto* const offset_ptr_0 = this->memory_resource_.template construct<OffsetPtr<PointedType>>(raw_ptr_0);
    auto* const offset_ptr_1 = this->memory_resource_.template construct<OffsetPtr<PointedType>>(raw_ptr_1);

    // When subtracting the 2 OffsetPtrs
    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = *offset_ptr_0 - *offset_ptr_1);
}

TYPED_TEST(OffsetPtrArithmeticComplexTypeFixture,
           OffsetPtrAndRawPtrSubtractionBetweenElementsIncorrectlyAlignedTerminates)
{
    using PointedType = typename TypeParam::second_type::Type;

    constexpr auto pointed_type_size{sizeof(PointedType)};

    // Given an OffsetPtr and a raw pointer in which the offset between the pointed-to elements is NOT a multiple of
    // sizeof(PointedType)

    // Allocate first pointed-to object
    auto* const raw_ptr_0 = this->memory_resource_.template construct<PointedType>();

    // Allocate some memory that will result in the next next allocation of PointedType not being allocated as a
    // multiple of sizeof(PointedType).
    this->memory_resource_.allocate(pointed_type_size / 2U);

    // Allocate second pointed-to object
    auto* const raw_ptr_1 = this->memory_resource_.template construct<PointedType>();

    // Allocate an OffetPtr for the first pointed-to object
    auto* const offset_ptr_0 = this->memory_resource_.template construct<OffsetPtr<PointedType>>(raw_ptr_0);

    // When subtracting the OffsetPtr and the raw pointer
    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = *offset_ptr_0 - raw_ptr_1);
}

}  // namespace score::memory::shared::test
