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

namespace score::memory::shared::test
{

template <typename T>
using OffsetPtrBoolOperationsFixture = OffsetPtrNoBoundsCheckingMemoryResourceFixture<T>;
TYPED_TEST_SUITE(OffsetPtrBoolOperationsFixture, AllMemoryResourceAndAllTypeCombinations, );

TYPED_TEST(OffsetPtrBoolOperationsFixture, NullPtrIsFalse)
{
    using PointedType = typename TypeParam::second_type::Type;

    auto* const ptr_to_offset_ptr = this->memory_resource_.template construct<OffsetPtr<PointedType>>(nullptr);
    auto offset_ptr = *ptr_to_offset_ptr;

    EXPECT_FALSE(offset_ptr);
}

TYPED_TEST(OffsetPtrBoolOperationsFixture, RegularPtrIsTrue)
{
    using PointedType = typename TypeParam::second_type::Type;

    auto [offset_ptr_wrapper, raw_ptr] =
        OffsetPtrCreator<PointedType>::CreateOffsetPtrInResource(this->memory_resource_);
    auto& offset_ptr = offset_ptr_wrapper.get();

    EXPECT_TRUE(offset_ptr);
}

}  // namespace score::memory::shared::test
