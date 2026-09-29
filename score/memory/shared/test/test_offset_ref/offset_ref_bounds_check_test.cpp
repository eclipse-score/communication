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
#include "score/memory/shared/offset_ref.h"

#include "score/memory/shared/fake/my_bounded_memory_resource.h"
#include "score/memory/shared/test/bounds_check_memory_pool.h"

#include <score/assert_support.hpp>

#include <gtest/gtest.h>

namespace score::memory::shared::test
{
namespace
{

// We use a global memory pool so that we can use BoundsCheckMemoryPool methods to parameterise the tests, mirroring
// the pattern used in test_offset_ptr/bounds_check_test.cpp
static BoundsCheckMemoryPool<int> gMemoryPool{};

// \brief Sanity-checks that OffsetRef properly delegates to OffsetPtr's bounds-checking mechanism. Exhaustive
// coverage of the bounds-checking logic itself is already provided by OffsetPtr's own test suite
// (test_offset_ptr/bounds_check_test.cpp); OffsetRef simply forwards to it.
TEST(OffsetRefBoundsCheckDeathTest, GetTerminatesWhenReferencedObjectLiesOutsideMemoryRegion)
{
    BoundsCheckMemoryPoolGuard<int> memory_pool_guard{gMemoryPool};
    MyBoundedMemoryResource memory_resource{{gMemoryPool.GetStartOfValidRegion(), gMemoryPool.GetEndOfValidRegion()}};

    // Given an OffsetRef which lies within a registered shared-memory region but which references an object outside
    // of that region
    auto* const pointed_to_address = gMemoryPool.GetPointedToAddressAfterValidRange();
    auto* const referenced_object = new (reinterpret_cast<void*>(pointed_to_address)) int{42};
    auto* const ref =
        new (reinterpret_cast<void*>(gMemoryPool.GetOffsetPtrAddressInValidRange())) OffsetRef<int>(*referenced_object);

    // When calling get()
    // Then the program terminates (bounds-check violation)
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = ref->get());
}

TEST(OffsetRefBoundsCheckTest, GetSucceedsWhenReferencedObjectLiesWithinMemoryRegion)
{
    BoundsCheckMemoryPoolGuard<int> memory_pool_guard{gMemoryPool};
    MyBoundedMemoryResource memory_resource{{gMemoryPool.GetStartOfValidRegion(), gMemoryPool.GetEndOfValidRegion()}};

    // Given an OffsetRef which lies within a registered shared-memory region and references an object also within
    // that region
    auto* const pointed_to_address = gMemoryPool.GetPointedToAddressInValidRange();
    auto* const referenced_object = new (reinterpret_cast<void*>(pointed_to_address)) int{42};
    auto* const ref =
        new (reinterpret_cast<void*>(gMemoryPool.GetOffsetPtrAddressInValidRange())) OffsetRef<int>(*referenced_object);

    // When calling get()
    // Then it succeeds and returns the correct value
    EXPECT_EQ(ref->get(), 42);
}

}  // namespace
}  // namespace score::memory::shared::test
