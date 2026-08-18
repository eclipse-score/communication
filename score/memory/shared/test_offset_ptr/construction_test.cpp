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

namespace
{

struct Base
{
    virtual ~Base() = default;
    int a = 0;
};
struct Base2
{
    virtual ~Base2() = default;
    int b = 0;
};

struct Derived : public Base, public Base2
{
};

}  // anonymous namespace

namespace score::memory::shared::test
{

template <typename T>
using OffsetPtrConstructionFixture = OffsetPtrNoBoundsCheckingMemoryResourceFixture<T>;
TYPED_TEST_SUITE(OffsetPtrConstructionFixture, AllMemoryResourceAndAllTypeCombinations, );

template <typename T>
using OffsetPtrConstructionIntOnlyFixture = OffsetPtrNoBoundsCheckingMemoryResourceFixture<T>;
TYPED_TEST_SUITE(OffsetPtrConstructionIntOnlyFixture, AllMemoryResourceAndNonVoidTypeCombinations, );

template <typename T>
using OffsetPtrConstructionVoidOnlyFixture = OffsetPtrNoBoundsCheckingMemoryResourceFixture<T>;
TYPED_TEST_SUITE(OffsetPtrConstructionVoidOnlyFixture, AllMemoryResourceAndVoidTypeCombinations, );

TYPED_TEST(OffsetPtrConstructionFixture, CanConstructOffsetPtrPointingToNullptr)
{
    using PointedType = typename TypeParam::second_type::Type;

    auto* const ptr_to_offset_ptr = this->memory_resource_.template construct<OffsetPtr<PointedType>>(nullptr);
    auto offset_ptr = *ptr_to_offset_ptr;

    EXPECT_EQ(OffsetPtrCreator<PointedType>::GetRawPointer(offset_ptr), nullptr);
}

TYPED_TEST(OffsetPtrConstructionFixture, CanConstructOffsetPtrOnStackPointingToNullptr)
{
    using PointedType = typename TypeParam::second_type::Type;

    OffsetPtr<PointedType> offset_ptr{nullptr};

    EXPECT_EQ(OffsetPtrCreator<PointedType>::GetRawPointer(offset_ptr), nullptr);
}

TYPED_TEST(OffsetPtrConstructionFixture, CanConstructOffsetPtrPointingToObject)
{
    using PointedType = typename TypeParam::second_type::Type;

    auto [offset_ptr_wrapper, raw_ptr] =
        OffsetPtrCreator<PointedType>::CreateOffsetPtrInResource(this->memory_resource_);
    auto& offset_ptr = offset_ptr_wrapper.get();

    EXPECT_EQ(OffsetPtrCreator<PointedType>::GetRawPointer(offset_ptr), raw_ptr);
}

TYPED_TEST(OffsetPtrConstructionFixture, CanConstructOffsetPtrOnStackPointingToObject)
{
    using PointedType = typename TypeParam::second_type::Type;

    auto* const raw_ptr = OffsetPtrCreator<PointedType>::CreatePointedToObject(this->memory_resource_);

    OffsetPtr<PointedType> offset_ptr{raw_ptr};

    EXPECT_EQ(OffsetPtrCreator<PointedType>::GetRawPointer(offset_ptr), raw_ptr);
}

TYPED_TEST(OffsetPtrConstructionFixture, CanCopyConstructorOffsetPtrPointingToObject)
{
    using PointedType = typename TypeParam::second_type::Type;

    auto [offset_ptr_wrapper_0, raw_ptr_0] =
        OffsetPtrCreator<PointedType>::CreateOffsetPtrInResource(this->memory_resource_);
    auto& offset_ptr_0 = offset_ptr_wrapper_0.get();

    auto* const ptr_to_offset_ptr_1 = this->memory_resource_.template construct<OffsetPtr<PointedType>>(offset_ptr_0);
    auto offset_ptr_1 = *ptr_to_offset_ptr_1;

    EXPECT_EQ(OffsetPtrCreator<PointedType>::GetRawPointer(offset_ptr_1), raw_ptr_0);
}

TYPED_TEST(OffsetPtrConstructionFixture, CanCopyConstructOffsetPtrPointingToObjectToStack)
{
    using PointedType = typename TypeParam::second_type::Type;

    auto [offset_ptr_wrapper_0, raw_ptr_0] =
        OffsetPtrCreator<PointedType>::CreateOffsetPtrInResource(this->memory_resource_);
    auto& offset_ptr_0 = offset_ptr_wrapper_0.get();

    OffsetPtr<PointedType> offset_ptr_1{offset_ptr_0};

    EXPECT_EQ(OffsetPtrCreator<PointedType>::GetRawPointer(offset_ptr_1), raw_ptr_0);
}

TYPED_TEST(OffsetPtrConstructionFixture, CanMoveConstructOffsetPtrPointingToObject)
{
    using PointedType = typename TypeParam::second_type::Type;

    auto [offset_ptr_wrapper_0, raw_ptr_0] =
        OffsetPtrCreator<PointedType>::CreateOffsetPtrInResource(this->memory_resource_);
    auto& offset_ptr_0 = offset_ptr_wrapper_0.get();

    auto* const ptr_to_offset_ptr_1 =
        this->memory_resource_.template construct<OffsetPtr<PointedType>>(std::move(offset_ptr_0));
    auto offset_ptr_1 = *ptr_to_offset_ptr_1;

    EXPECT_EQ(OffsetPtrCreator<PointedType>::GetRawPointer(offset_ptr_1), raw_ptr_0);
}

TYPED_TEST(OffsetPtrConstructionFixture, CanMoveConstructOffsetPtrPointingToObjectToStack)
{
    using PointedType = typename TypeParam::second_type::Type;

    auto [offset_ptr_wrapper_0, raw_ptr_0] =
        OffsetPtrCreator<PointedType>::CreateOffsetPtrInResource(this->memory_resource_);
    auto& offset_ptr_0 = offset_ptr_wrapper_0.get();

    OffsetPtr<PointedType> offset_ptr_1{std::move(offset_ptr_0)};

    EXPECT_EQ(OffsetPtrCreator<PointedType>::GetRawPointer(offset_ptr_1), raw_ptr_0);
}

TYPED_TEST(OffsetPtrConstructionIntOnlyFixture, DifferentTypeConstructorHandlesDifferentTypeOffsetPtr)
{
    auto [offset_ptr_derived_wrapper, raw_ptr] =
        OffsetPtrCreator<Derived>::CreateOffsetPtrInResource(this->memory_resource_);
    auto& offset_ptr_derived = offset_ptr_derived_wrapper.get();

    auto* const ptr_to_offset_ptr_base_0 =
        this->memory_resource_.template construct<OffsetPtr<Base>>(offset_ptr_derived);
    auto offset_ptr_base_0 = *ptr_to_offset_ptr_base_0;

    auto* const ptr_to_offset_ptr_base_1 =
        this->memory_resource_.template construct<OffsetPtr<Base2>>(offset_ptr_derived);
    auto offset_ptr_base_1 = *ptr_to_offset_ptr_base_1;

    // The memory addresses of static_cast<Base*>(unique_ptr.get()) and
    // static_cast<Base2*>(unique_ptr.get()) should be different to each other and at
    // least one should be different to unique_ptr.get(), which is of type Derived. If the
    // copy constructor doesn't correctly cast the Derived pointer to the element_type
    // of the OffsetPtr (i.e. Base or Base2), then the pointed-to address would be of the
    // derived object, rather than the Base object, and one of the tests below would fail.
    EXPECT_EQ(offset_ptr_base_0.get(), static_cast<Base*>(raw_ptr));
    EXPECT_EQ(offset_ptr_base_1.get(), static_cast<Base2*>(raw_ptr));
}

TYPED_TEST(OffsetPtrConstructionIntOnlyFixture, DifferentTypeConstructorHandlesDifferentTypeOffsetPtrOnStack)
{
    auto [offset_ptr_derived_wrapper, raw_ptr] =
        OffsetPtrCreator<Derived>::CreateOffsetPtrInResource(this->memory_resource_);
    auto& offset_ptr_derived = offset_ptr_derived_wrapper.get();

    OffsetPtr<Base> offset_ptr_base_0{offset_ptr_derived};
    OffsetPtr<Base2> offset_ptr_base_1{offset_ptr_derived};

    // The memory addresses of static_cast<Base*>(raw_ptr) and
    // static_cast<Base2*>(raw_ptr) should be different to each other and at
    // least one should be different to raw_ptr, which is of type Derived. If the
    // assignment operator doesn't correctly cast the Derived pointer to the element_type
    // of the OffsetPtr (i.e. Base or Base2), then the pointed-to address would be of the
    // derived object, rather than the Base object, and one of the tests below would fail.
    EXPECT_EQ(offset_ptr_base_0.get(), static_cast<Base*>(raw_ptr));
    EXPECT_EQ(offset_ptr_base_1.get(), static_cast<Base2*>(raw_ptr));
}

TYPED_TEST(OffsetPtrConstructionVoidOnlyFixture, DifferentTypeConstructorHandlesDifferentTypeOffsetPtr)
{
    using PointedVoidType = typename TypeParam::second_type::Type;

    auto [offset_ptr_base_wrapper, raw_ptr] = OffsetPtrCreator<Base>::CreateOffsetPtrInResource(this->memory_resource_);
    auto& offset_ptr_base = offset_ptr_base_wrapper.get();

    auto* const ptr_to_offset_ptr_void =
        this->memory_resource_.template construct<OffsetPtr<PointedVoidType>>(offset_ptr_base);
    auto offset_ptr_void = *ptr_to_offset_ptr_void;

    EXPECT_EQ(OffsetPtrCreator<PointedVoidType>::GetRawPointer(offset_ptr_void), raw_ptr);
}

TYPED_TEST(OffsetPtrConstructionVoidOnlyFixture, DifferentTypeConstructorHandlesDifferentTypeOffsetPtrOnStack)
{
    using PointedVoidType = typename TypeParam::second_type::Type;

    auto [offset_ptr_base_wrapper, raw_ptr] = OffsetPtrCreator<Base>::CreateOffsetPtrInResource(this->memory_resource_);
    auto& offset_ptr_base = offset_ptr_base_wrapper.get();

    OffsetPtr<PointedVoidType> offset_ptr_void{offset_ptr_base};

    EXPECT_EQ(OffsetPtrCreator<PointedVoidType>::GetRawPointer(offset_ptr_void), raw_ptr);
}

}  // namespace score::memory::shared::test
