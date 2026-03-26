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
#include <type_traits>

namespace score::memory::shared::test
{

namespace
{

template <typename T1, typename T2, typename RefType1, typename RefType2>
void checkCompareOperators(const T1& p1, const T2& p2, const RefType1& ref1, const RefType2& ref2)
{
    EXPECT_EQ(p1 == p2, ref1 == ref2);
    EXPECT_EQ(p1 != p2, ref1 != ref2);
    EXPECT_EQ(p1 >= p2, ref1 >= ref2);
    EXPECT_EQ(p1 <= p2, ref1 <= ref2);
    EXPECT_EQ(p1 < p2, ref1 < ref2);
    EXPECT_EQ(p1 > p2, ref1 > ref2);
}

template <typename T1, typename T2>
void compare(OffsetPtr<T1>& offset_ptr_0, OffsetPtr<T2>& offset_ptr_1, T1* raw_ptr_0, T2* raw_ptr_1)
{
    // Compare 2 OffsetPtrs
    checkCompareOperators(offset_ptr_0, offset_ptr_1, raw_ptr_0, raw_ptr_1);

    // Compare OffsetPtr with raw pointer
    checkCompareOperators(raw_ptr_0, offset_ptr_1, raw_ptr_0, raw_ptr_1);
    checkCompareOperators(offset_ptr_0, raw_ptr_1, raw_ptr_0, raw_ptr_1);
}

}  // namespace

template <typename T>
using OffsetPtrCompareFixture = OffsetPtrNoBoundsCheckingMemoryResourceFixture<T>;
TYPED_TEST_SUITE(OffsetPtrCompareFixture, AllMemoryResourceAndAllTypeCombinations, );

TYPED_TEST(OffsetPtrCompareFixture, NullOffsetPtrCompareOperatorsMatchesRawPtrCompareOperators)
{
    using PointedType = typename TypeParam::second_type::Type;

    auto* const ptr_to_offset_ptr_0 = this->memory_resource_.template construct<OffsetPtr<PointedType>>(nullptr);
    auto offset_ptr_0 = *ptr_to_offset_ptr_0;

    auto* const ptr_to_offset_ptr_1 = this->memory_resource_.template construct<OffsetPtr<PointedType>>(nullptr);
    auto offset_ptr_1 = *ptr_to_offset_ptr_1;

    compare<PointedType, PointedType>(offset_ptr_0, offset_ptr_1, nullptr, nullptr);
}

TYPED_TEST(OffsetPtrCompareFixture, NullOffsetPtrCompareOperatorsMatchesRawPtrCompareOperatorsOneConst)
{
    using PointedType = typename TypeParam::second_type::Type;
    using ConstPointedType = std::add_const_t<PointedType>;

    auto* const ptr_to_offset_ptr_0 = this->memory_resource_.template construct<OffsetPtr<PointedType>>(nullptr);
    auto offset_ptr_0 = *ptr_to_offset_ptr_0;

    auto* const ptr_to_offset_ptr_1 = this->memory_resource_.template construct<OffsetPtr<ConstPointedType>>(nullptr);
    auto offset_ptr_1 = *ptr_to_offset_ptr_1;

    compare<PointedType, ConstPointedType>(offset_ptr_0, offset_ptr_1, nullptr, nullptr);
}

TYPED_TEST(OffsetPtrCompareFixture, OffsetPtrCompareOperatorsMatchesRawPtrCompareOperators)
{
    using PointedType = typename TypeParam::second_type::Type;

    auto [offset_ptr_wrapper_0, raw_ptr_0] =
        OffsetPtrCreator<PointedType>::CreateOffsetPtrInResource(this->memory_resource_);
    auto& offset_ptr_0 = offset_ptr_wrapper_0.get();

    auto [offset_ptr_wrapper_1, raw_ptr_1] =
        OffsetPtrCreator<PointedType>::CreateOffsetPtrInResource(this->memory_resource_);
    auto& offset_ptr_1 = offset_ptr_wrapper_1.get();

    compare<PointedType, PointedType>(offset_ptr_0, offset_ptr_1, raw_ptr_0, raw_ptr_1);
}

TYPED_TEST(OffsetPtrCompareFixture, OffsetPtrCompareOperatorsMatchesRawPtrCompareOperatorsOneConst)
{
    using PointedType = typename TypeParam::second_type::Type;
    using ConstPointedType = std::add_const_t<PointedType>;

    auto [offset_ptr_wrapper_0, raw_ptr_0] =
        OffsetPtrCreator<PointedType>::CreateOffsetPtrInResource(this->memory_resource_);
    auto& offset_ptr_0 = offset_ptr_wrapper_0.get();

    auto [offset_ptr_wrapper_1, raw_ptr_1] =
        OffsetPtrCreator<ConstPointedType>::CreateOffsetPtrInResource(this->memory_resource_);
    auto& offset_ptr_1 = offset_ptr_wrapper_1.get();

    compare<PointedType, ConstPointedType>(offset_ptr_0, offset_ptr_1, raw_ptr_0, raw_ptr_1);
}

}  // namespace score::memory::shared::test
