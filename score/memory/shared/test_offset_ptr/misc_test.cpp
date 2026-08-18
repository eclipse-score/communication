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
#include "score/memory/shared/offset_ptr.h"
#include "score/memory/shared/test_offset_ptr/offset_ptr_test_resources.h"

#include <gtest/gtest.h>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace score::memory::shared::test
{

constexpr std::ptrdiff_t kNullPtrRepresentation{1};

template <typename T>
using OffsetPtrMiscFixture = OffsetPtrNoBoundsCheckingMemoryResourceFixture<T>;
TYPED_TEST_SUITE(OffsetPtrMiscFixture, AllMemoryResourceAndAllTypeCombinations, );

template <typename T>
using OffsetPtrMiscPointerToIntOnlyFixture = OffsetPtrNoBoundsCheckingMemoryResourceFixture<T>;
TYPED_TEST_SUITE(OffsetPtrMiscPointerToIntOnlyFixture, AllMemoryResourceAndNonVoidTypeCombinations, );

template <typename T>
using OffsetPtrMiscPointerToVoidOnlyFixture = OffsetPtrNoBoundsCheckingMemoryResourceFixture<T>;
TYPED_TEST_SUITE(OffsetPtrMiscPointerToVoidOnlyFixture, AllMemoryResourceAndVoidTypeCombinations, );

TYPED_TEST(OffsetPtrMiscFixture, SwapHandlesNullPtr)
{
    using PointedType = typename TypeParam::second_type::Type;

    auto* const ptr_to_offset_ptr_0 = this->memory_resource_.template construct<OffsetPtr<PointedType>>(nullptr);
    auto offset_ptr_0 = *ptr_to_offset_ptr_0;

    auto* const ptr_to_offset_ptr_1 = this->memory_resource_.template construct<OffsetPtr<PointedType>>(nullptr);
    auto offset_ptr_1 = *ptr_to_offset_ptr_1;

    swap(offset_ptr_0, offset_ptr_1);

    EXPECT_EQ(OffsetPtrCreator<PointedType>::GetRawPointer(offset_ptr_1), nullptr);
    EXPECT_EQ(OffsetPtrCreator<PointedType>::GetRawPointer(offset_ptr_0), nullptr);
}

TYPED_TEST(OffsetPtrMiscFixture, SwapHandlesRegularPtr)
{
    using PointedType = typename TypeParam::second_type::Type;

    auto [offset_ptr_wrapper_0, raw_ptr_0] =
        OffsetPtrCreator<PointedType>::CreateOffsetPtrInResource(this->memory_resource_);
    auto& offset_ptr_0 = offset_ptr_wrapper_0.get();

    auto [offset_ptr_wrapper_1, raw_ptr_1] =
        OffsetPtrCreator<PointedType>::CreateOffsetPtrInResource(this->memory_resource_);
    auto& offset_ptr_1 = offset_ptr_wrapper_1.get();

    swap(offset_ptr_0, offset_ptr_1);

    EXPECT_EQ(OffsetPtrCreator<PointedType>::GetRawPointer(offset_ptr_0), raw_ptr_1);
    EXPECT_EQ(OffsetPtrCreator<PointedType>::GetRawPointer(offset_ptr_1), raw_ptr_0);
}

TYPED_TEST(OffsetPtrMiscPointerToIntOnlyFixture, PointerToHandlesRegularValue)
{
    using PointedType = typename TypeParam::second_type::Type;

    const auto initial_value = TypeParam::second_type::CreateDummyValue();
    auto* raw_ptr = OffsetPtrCreator<PointedType>::CreatePointedToObject(this->memory_resource_, initial_value);

    auto* const ptr_to_offset_ptr =
        this->memory_resource_.template construct<OffsetPtr<PointedType>>(OffsetPtr<PointedType>::pointer_to(*raw_ptr));
    auto offset_ptr = *ptr_to_offset_ptr;

    EXPECT_EQ(offset_ptr.get(), raw_ptr);
}

TYPED_TEST(OffsetPtrMiscPointerToVoidOnlyFixture, PointerToHandlesRegularValue)
{
    using PointedVoidType = typename TypeParam::second_type::Type;

    auto* raw_ptr = OffsetPtrCreator<PointedVoidType>::CreatePointedToObject(this->memory_resource_, 10);

    auto* const ptr_to_offset_ptr = this->memory_resource_.template construct<OffsetPtr<PointedVoidType>>(
        OffsetPtr<PointedVoidType>::pointer_to(raw_ptr));
    auto offset_ptr = *ptr_to_offset_ptr;

    EXPECT_EQ(OffsetPtrCreator<PointedVoidType>::GetRawPointer(offset_ptr), raw_ptr);
}

TYPED_TEST(OffsetPtrMiscPointerToVoidOnlyFixture, TypedGetHandlesRegularValue)
{
    using PointedVoidType = typename TypeParam::second_type::Type;

    auto* const value_ptr = this->memory_resource_.template construct<int>(10);
    auto* raw_ptr = static_cast<PointedVoidType*>(value_ptr);

    auto* const ptr_to_offset_ptr = this->memory_resource_.template construct<OffsetPtr<PointedVoidType>>(
        OffsetPtr<PointedVoidType>::pointer_to(raw_ptr));
    auto offset_ptr = *ptr_to_offset_ptr;

    if constexpr (std::is_const_v<PointedVoidType>)
    {
        EXPECT_EQ(offset_ptr.template get<const int>(), value_ptr);
    }
    else
    {
        EXPECT_EQ(offset_ptr.template get<int>(), value_ptr);
    }
}

TYPED_TEST(OffsetPtrMiscPointerToVoidOnlyFixture, GetWithSizeHandlesRegularValue)
{
    using PointedVoidType = typename TypeParam::second_type::Type;

    auto* const value_ptr = this->memory_resource_.template construct<int>(10);
    auto* raw_ptr = static_cast<PointedVoidType*>(value_ptr);

    auto* const ptr_to_offset_ptr = this->memory_resource_.template construct<OffsetPtr<PointedVoidType>>(
        OffsetPtr<PointedVoidType>::pointer_to(raw_ptr));
    auto offset_ptr = *ptr_to_offset_ptr;

    const auto pointed_type_size = sizeof(int);
    EXPECT_EQ(offset_ptr.get(pointed_type_size), raw_ptr);
}

TYPED_TEST(OffsetPtrMiscFixture, OffsetEqualsNullPtrRepresentationTerminates)
{
    using PointedType = typename TypeParam::second_type::Type;

    // Given a pre-allocated buffer in a registered memory resource that fits an OffsetPtr
    auto* buffer = this->memory_resource_.allocate(sizeof(OffsetPtr<PointedType>));
    auto* int_buffer = static_cast<std::uint8_t*>(buffer);

    // When creating an OffsetPtr which points to an address kNullPtrRepresentation bytes away
    // THen the program should terminate.
    auto* invalid_pointed_to_address = reinterpret_cast<PointedType*>(int_buffer + kNullPtrRepresentation);
    EXPECT_DEATH(new (buffer) OffsetPtr<PointedType>{invalid_pointed_to_address}, ".*");
}

TYPED_TEST(OffsetPtrMiscPointerToIntOnlyFixture, HandlesCastingOffsetPtrToRegularPointer)
{
    using PointedType = typename TypeParam::second_type::Type;

    const auto initial_value = TypeParam::second_type::CreateDummyValue();
    auto [offset_ptr_wrapper, raw_ptr] =
        OffsetPtrCreator<PointedType>::CreateOffsetPtrInResource(this->memory_resource_, initial_value);
    auto& offset_ptr = offset_ptr_wrapper.get();

    EXPECT_EQ(static_cast<PointedType*>(offset_ptr), raw_ptr);
}

TYPED_TEST(OffsetPtrMiscPointerToIntOnlyFixture, HandlesCastingOffsetPtrContainingNullptrToRegularPointer)
{
    using PointedType = typename TypeParam::second_type::Type;

    auto* allocated_memory = this->memory_resource_.allocate(sizeof(PointedType));

    auto* const ptr_to_offset_ptr = new (allocated_memory) OffsetPtr<PointedType>{nullptr};
    auto& offset_ptr = *ptr_to_offset_ptr;

    EXPECT_EQ(static_cast<PointedType*>(offset_ptr), nullptr);
}

TYPED_TEST(OffsetPtrMiscPointerToIntOnlyFixture, HandlesGettingOffsetPtr)
{
    using PointedType = typename TypeParam::second_type::Type;

    const auto initial_value = TypeParam::second_type::CreateDummyValue();
    auto [offset_ptr_wrapper, raw_ptr] =
        OffsetPtrCreator<PointedType>::CreateOffsetPtrInResource(this->memory_resource_, initial_value);
    auto& offset_ptr = offset_ptr_wrapper.get();

    EXPECT_EQ(offset_ptr.get(), raw_ptr);
}

TYPED_TEST(OffsetPtrMiscPointerToIntOnlyFixture, HandlesGettingOffsetPtrToNullptr)
{
    using PointedType = typename TypeParam::second_type::Type;

    auto* allocated_memory = this->memory_resource_.allocate(sizeof(PointedType));

    auto* const ptr_to_offset_ptr = new (allocated_memory) OffsetPtr<PointedType>{nullptr};
    auto& offset_ptr = *ptr_to_offset_ptr;

    EXPECT_EQ(offset_ptr.get(), nullptr);
}

TYPED_TEST(OffsetPtrMiscPointerToIntOnlyFixture, HandlesGettingNonDereferenceableOffsetPtr)
{
    using PointedType = typename TypeParam::second_type::Type;

    const auto initial_value = TypeParam::second_type::CreateDummyValue();
    auto [offset_ptr_wrapper, raw_ptr] =
        OffsetPtrCreator<PointedType>::CreateOffsetPtrInResource(this->memory_resource_, initial_value);
    auto& offset_ptr = offset_ptr_wrapper.get();

    EXPECT_EQ(offset_ptr.get(), raw_ptr);
}

TYPED_TEST(OffsetPtrMiscPointerToIntOnlyFixture, HandlesGettingNonDereferenceableOffsetPtrToNullptr)
{
    using PointedType = typename TypeParam::second_type::Type;

    auto* allocated_memory = this->memory_resource_.allocate(sizeof(PointedType));

    auto* const ptr_to_offset_ptr = new (allocated_memory) OffsetPtr<PointedType>{nullptr};
    auto& offset_ptr = *ptr_to_offset_ptr;

    EXPECT_EQ(offset_ptr.get(), nullptr);
}

}  // namespace score::memory::shared::test
