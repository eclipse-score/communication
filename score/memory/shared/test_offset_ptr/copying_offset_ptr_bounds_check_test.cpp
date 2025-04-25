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
#include "score/memory/shared/fake/my_bounded_memory_resource.h"
#include "score/memory/shared/memory_resource_registry.h"
#include "score/memory/shared/offset_ptr.h"

#include "score/memory/shared/test_offset_ptr/bounds_check_memory_pool.h"

#include <score/assert_support.hpp>

#include <score/utility.hpp>
#include <gtest/gtest.h>
#include <cstdint>
#include <utility>
#include <vector>

namespace score::memory::shared::test
{
namespace
{

// Custom data type is used to allow us to construct a PointedType at a distance of sizeof(PointedType) / 2 from the end
// region while still respecting alignment.
class PointedType
{
  public:
    PointedType(int value) : a{value}, b{value}, c{value}, d{value} {}
    int a;
    int b;
    int c;
    int d;
};
bool operator==(const PointedType& lhs, const PointedType& rhs)
{
    return ((lhs.a == rhs.a) && (lhs.b == rhs.b) && (lhs.c == rhs.c) && (lhs.d == rhs.d));
}

// We use a global memory pool so that we can use BoundsCheckMemoryPool methods to paramaterise the tests
static BoundsCheckMemoryPool<PointedType> gMemoryPool{};
static BoundsCheckMemoryPool<PointedType> gSecondMemoryPool{};

struct TestParams
{
    BoundsCheckMemoryPool<PointedType>::MemoryPool::iterator offset_ptr_address;
    BoundsCheckMemoryPool<PointedType>::MemoryPool::iterator pointed_to_address;
    BoundsCheckMemoryPool<PointedType>::MemoryPool::iterator copied_offset_ptr_address;
};

class CopiedOffsetPtrBoundsCheckParamaterisedFixture : public ::testing::TestWithParam<TestParams>
{
  public:
    template <typename T = PointedType>
    [[nodiscard]] auto CopyOffsetPtrTo(
        const OffsetPtr<T>& offset_ptr_to_copy,
        BoundsCheckMemoryPool<PointedType>::MemoryPool::iterator copied_offset_ptr_address) noexcept -> OffsetPtr<T>*
    {
        auto* copied_offset_ptr = new (copied_offset_ptr_address) OffsetPtr<T>(offset_ptr_to_copy);
        return copied_offset_ptr;
    }

    // Guard wrapper to reset the memory pool between each test (since its lifetime is static so it will not be created
    // / destroyed for every test)
    BoundsCheckMemoryPoolGuard<PointedType> memory_pool_guard_{gMemoryPool};
    BoundsCheckMemoryPoolGuard<PointedType> second_memory_pool_guard_{gSecondMemoryPool};
    MyBoundedMemoryResource memory_resource_{{gMemoryPool.GetStartOfValidRegion(), gMemoryPool.GetEndOfValidRegion()}};
    MyBoundedMemoryResource memory_resource_2_{
        {gSecondMemoryPool.GetStartOfValidRegion(), gSecondMemoryPool.GetEndOfValidRegion()}};
};

std::vector<TestParams> GenerateOffsetPtrAddressesThatPassBoundsChecks(BoundsCheckMemoryPool<PointedType>& memory_pool)
{
    // clang-format off
    return {
            // Copying: resource to resource
            //      - from: OffsetPtr (in resource) -> pointed-to (in resource) 
            //      - to:   other address in resource
            TestParams{memory_pool.GetOffsetPtrAddressInValidRange(), memory_pool.GetPointedToAddressInValidRange(),          memory_pool.GetSecondOffsetPtrAddressInValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressInValidRange(), memory_pool.GetPointedToAddressFinishingAtEndAddress(), memory_pool.GetSecondOffsetPtrAddressInValidRange()},

            // Copying: resource to stack
            //      - from: OffsetPtr (in resource) -> pointed-to (in resource) 
            //      - to:   other start address not in any resource
            TestParams{memory_pool.GetOffsetPtrAddressInValidRange(), memory_pool.GetPointedToAddressInValidRange(),          memory_pool.GetOffsetPtrAddressBeforeValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressInValidRange(), memory_pool.GetPointedToAddressFinishingAtEndAddress(), memory_pool.GetOffsetPtrAddressBeforeValidRange()},

            // Copying: stack to resource
            //      - from: OffsetPtr (not in resource) -> pointed-to (in resource) 
            //      - to:   other address in resource
            TestParams{memory_pool.GetOffsetPtrAddressAfterValidRange(), memory_pool.GetPointedToAddressInValidRange(),          memory_pool.GetSecondOffsetPtrAddressInValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressAfterValidRange(), memory_pool.GetPointedToAddressFinishingAtEndAddress(), memory_pool.GetSecondOffsetPtrAddressInValidRange()},

            // Copying: stack to stack
            //      - from: OffsetPtr (not in resource) -> pointed-to (in resource) 
            //      - to:   other address not in resource
            TestParams{memory_pool.GetOffsetPtrAddressAfterValidRange(), memory_pool.GetPointedToAddressInValidRange(),          memory_pool.GetOffsetPtrAddressBeforeValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressAfterValidRange(), memory_pool.GetPointedToAddressFinishingAtEndAddress(), memory_pool.GetOffsetPtrAddressBeforeValidRange()},

            // Copying: stack to stack
            //      - from: OffsetPtr (not in resource) -> pointed-to (not in resource) 
            //      - to:   other address not in resource
            TestParams{memory_pool.GetOffsetPtrAddressAfterValidRange(), memory_pool.GetPointedToAddressBeforeValidRange(),               memory_pool.GetOffsetPtrAddressBeforeValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressAfterValidRange(), memory_pool.GetPointedToAddressAfterValidRange(),                memory_pool.GetOffsetPtrAddressBeforeValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressAfterValidRange(), memory_pool.GetPointedToAddressOverlappingWithStartRange(),      memory_pool.GetOffsetPtrAddressBeforeValidRange()}
        };
    // clang-format on
}

std::vector<TestParams> GenerateOffsetPtrAddressesThatFailBoundChecks(
    BoundsCheckMemoryPool<PointedType>& memory_pool,
    BoundsCheckMemoryPool<PointedType>& second_memory_pool)
{
    // clang-format off
    return {
            // Copying: resource to resource
            //      - from: OffsetPtr (in resource) -> pointed-to (outside resource or starting inside / ending outside resource)
            //      - to:   other address in resource
            TestParams{memory_pool.GetOffsetPtrAddressInValidRange(), memory_pool.GetPointedToAddressOverlappingWithEndRange(),   memory_pool.GetSecondOffsetPtrAddressInValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressInValidRange(), memory_pool.GetPointedToAddressBeforeValidRange(),          memory_pool.GetSecondOffsetPtrAddressInValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressInValidRange(), memory_pool.GetPointedToAddressAfterValidRange(),           memory_pool.GetSecondOffsetPtrAddressInValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressInValidRange(), memory_pool.GetPointedToAddressOverlappingWithStartRange(), memory_pool.GetSecondOffsetPtrAddressInValidRange()},

            // Copying: resource to resource
            //      - from: OffsetPtr (in resource) -> pointed-to (another resource)
            //      - to:   other address in resource
            TestParams{memory_pool.GetOffsetPtrAddressInValidRange(), second_memory_pool.GetPointedToAddressBeforeValidRange(),          memory_pool.GetSecondOffsetPtrAddressInValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressInValidRange(), second_memory_pool.GetPointedToAddressAfterValidRange(),           memory_pool.GetSecondOffsetPtrAddressInValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressInValidRange(), second_memory_pool.GetPointedToAddressOverlappingWithStartRange(), memory_pool.GetSecondOffsetPtrAddressInValidRange()},

            // Copying: resource to other resource
            //      - from: OffsetPtr (in resource) -> pointed-to (same resource) 
            //      - to:   address in other resource
            TestParams{memory_pool.GetOffsetPtrAddressInValidRange(), memory_pool.GetPointedToAddressInValidRange(),            second_memory_pool.GetOffsetPtrAddressInValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressInValidRange(), memory_pool.GetPointedToAddressFinishingAtEndAddress(),   second_memory_pool.GetOffsetPtrAddressInValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressInValidRange(), memory_pool.GetPointedToAddressOverlappingWithEndRange(), second_memory_pool.GetOffsetPtrAddressInValidRange()},

            // Copying: resource to other resource
            //      - from: OffsetPtr (in resource) -> pointed-to (outside resource) 
            //      - to:   address in other resource
            TestParams{memory_pool.GetOffsetPtrAddressInValidRange(), memory_pool.GetPointedToAddressBeforeValidRange(),          second_memory_pool.GetOffsetPtrAddressInValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressInValidRange(), memory_pool.GetPointedToAddressAfterValidRange(),           second_memory_pool.GetOffsetPtrAddressInValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressInValidRange(), memory_pool.GetPointedToAddressOverlappingWithStartRange(), second_memory_pool.GetOffsetPtrAddressInValidRange()},
            
            // Copying: resource to stack
            //      - from: OffsetPtr (in resource) -> pointed-to (outside resource or starting inside / ending outside resource)
            //      - to:   other address not in resource
            TestParams{memory_pool.GetOffsetPtrAddressInValidRange(), memory_pool.GetPointedToAddressOverlappingWithEndRange(),   memory_pool.GetOffsetPtrAddressBeforeValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressInValidRange(), memory_pool.GetPointedToAddressBeforeValidRange(),          memory_pool.GetOffsetPtrAddressBeforeValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressInValidRange(), memory_pool.GetPointedToAddressAfterValidRange(),           memory_pool.GetOffsetPtrAddressBeforeValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressInValidRange(), memory_pool.GetPointedToAddressOverlappingWithStartRange(), memory_pool.GetOffsetPtrAddressBeforeValidRange()},

            // Copying: resource to stack
            //      - from: OffsetPtr (in resource) -> pointed-to (another resource) 
            //      - to:   other start address not in any resource
            TestParams{memory_pool.GetOffsetPtrAddressInValidRange(), second_memory_pool.GetPointedToAddressInValidRange(),            memory_pool.GetOffsetPtrAddressBeforeValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressInValidRange(), second_memory_pool.GetPointedToAddressFinishingAtEndAddress(),   memory_pool.GetOffsetPtrAddressBeforeValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressInValidRange(), second_memory_pool.GetPointedToAddressOverlappingWithEndRange(), memory_pool.GetOffsetPtrAddressBeforeValidRange()},

            // Copying: stack to resource
            //      - from: OffsetPtr (outside resource) -> pointed-to (starting inside / ending outside resource)
            //      - to:   address in same resource
            TestParams{memory_pool.GetOffsetPtrAddressAfterValidRange(), memory_pool.GetPointedToAddressOverlappingWithEndRange(), memory_pool.GetOffsetPtrAddressInValidRange()},

            // Copying: stack to resource
            //      - from: OffsetPtr (outside resource) -> pointed-to (resource)
            //      - to:   address in different resource
            TestParams{memory_pool.GetOffsetPtrAddressAfterValidRange(), memory_pool.GetPointedToAddressInValidRange(),            second_memory_pool.GetOffsetPtrAddressInValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressAfterValidRange(), memory_pool.GetPointedToAddressFinishingAtEndAddress(),   second_memory_pool.GetOffsetPtrAddressInValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressAfterValidRange(), memory_pool.GetPointedToAddressOverlappingWithEndRange(), second_memory_pool.GetOffsetPtrAddressInValidRange()}
        };
    // clang-format on
}

using OffsetPtrCopyStartCheckOnlyFixture = CopiedOffsetPtrBoundsCheckParamaterisedFixture;
TEST_P(OffsetPtrCopyStartCheckOnlyFixture, CopyingOffsetPointerDoesNotTerminate)
{
    const auto& params = GetParam();

    // Given an OffsetPtr pointing to an object
    auto* const ptr_to_offset_ptr = CreateOffsetPtr<PointedType>(params.offset_ptr_address, params.pointed_to_address);

    // When copying the OffsetPtr to another location
    score::cpp::ignore = CopyOffsetPtrTo(*ptr_to_offset_ptr, params.copied_offset_ptr_address);

    // Then the program should not terminate
}

// clang-format off
INSTANTIATE_TEST_SUITE_P(CopiedOffsetPtrNoBoundsCheckBoundChecksWouldPass, OffsetPtrCopyStartCheckOnlyFixture, 
    ::testing::ValuesIn(GenerateOffsetPtrAddressesThatPassBoundsChecks(gMemoryPool))
);
INSTANTIATE_TEST_SUITE_P(CopiedOffsetPtrNoBoundsCheckBoundChecksWouldFail, OffsetPtrCopyStartCheckOnlyFixture, 
    ::testing::ValuesIn(GenerateOffsetPtrAddressesThatFailBoundChecks(gMemoryPool, gSecondMemoryPool))
);

// clang-format on

using OffsetPtrCopyStartAndEndChecksFixture = CopiedOffsetPtrBoundsCheckParamaterisedFixture;
TEST_P(OffsetPtrCopyStartAndEndChecksFixture, GettingOffsetPointerReturnsCorrectPointer)
{
    const auto& params = GetParam();

    // Given an OffsetPtr pointing to an object that was copied to another location
    auto* ptr_to_offset_ptr = CreateOffsetPtr<PointedType>(params.offset_ptr_address, params.pointed_to_address);
    ptr_to_offset_ptr = CopyOffsetPtrTo(*ptr_to_offset_ptr, params.copied_offset_ptr_address);

    // When getting a raw pointer from the copied OffsetPtr
    auto* actual_pointed_to_address = ptr_to_offset_ptr->get();

    // Then returned pointer should be the same as the initial pointed-to address
    EXPECT_EQ(actual_pointed_to_address, reinterpret_cast<PointedType*>(params.pointed_to_address));
}

TEST_P(OffsetPtrCopyStartAndEndChecksFixture, GettingOffsetPointerWithTypedGetReturnsCorrectPointer)
{
    const auto& params = GetParam();

    // Given an OffsetPtr pointing to an object that was copied to another location
    auto* ptr_to_offset_ptr = CreateOffsetPtr<void>(params.offset_ptr_address, params.pointed_to_address);
    ptr_to_offset_ptr = CopyOffsetPtrTo(*ptr_to_offset_ptr, params.copied_offset_ptr_address);

    // When getting a raw pointer from the copied OffsetPtr<void> by explicitly specifying the type
    auto* actual_pointed_to_address = ptr_to_offset_ptr->get<PointedType>();

    // Then returned pointer should be the same as the initial pointed-to address
    EXPECT_EQ(actual_pointed_to_address, reinterpret_cast<PointedType*>(params.pointed_to_address));
}

TEST_P(OffsetPtrCopyStartAndEndChecksFixture, GettingOffsetPointerWithSizedGetReturnsCorrectPointer)
{
    const auto& params = GetParam();

    // Given an OffsetPtr pointing to an object that was copied to another location
    auto* ptr_to_offset_ptr = CreateOffsetPtr<void>(params.offset_ptr_address, params.pointed_to_address);
    ptr_to_offset_ptr = CopyOffsetPtrTo(*ptr_to_offset_ptr, params.copied_offset_ptr_address);

    // When getting a raw pointer from the OffsetPtr<void> by explicitly specifying the size of the type
    const auto pointed_type_size = sizeof(PointedType);
    auto* actual_pointed_to_address = ptr_to_offset_ptr->get(pointed_type_size);

    // Then returned pointer should be the same as the initial pointed-to address
    EXPECT_EQ(actual_pointed_to_address, reinterpret_cast<PointedType*>(params.pointed_to_address));
}

TEST_P(OffsetPtrCopyStartAndEndChecksFixture, DereferencingOffsetPointerReturnsCorrectValue)
{
    const auto& params = GetParam();

    // Given an OffsetPtr pointing to an object that was copied to another location
    auto* ptr_to_offset_ptr = CreateOffsetPtr<PointedType>(params.offset_ptr_address, params.pointed_to_address);
    ptr_to_offset_ptr = CopyOffsetPtrTo(*ptr_to_offset_ptr, params.copied_offset_ptr_address);

    // When dereferencing the copied OffsetPtr
    const auto actual_value = *(*ptr_to_offset_ptr);

    // Then the correct value should be returned
    EXPECT_EQ(actual_value, *reinterpret_cast<PointedType*>(params.pointed_to_address));
}

TEST_P(OffsetPtrCopyStartAndEndChecksFixture, PointerOperatorReturnsCorrectPointer)
{
    const auto& params = GetParam();

    // Given an OffsetPtr pointing to an object that was copied to another location
    auto* ptr_to_offset_ptr = CreateOffsetPtr<PointedType>(params.offset_ptr_address, params.pointed_to_address);
    ptr_to_offset_ptr = CopyOffsetPtrTo(*ptr_to_offset_ptr, params.copied_offset_ptr_address);

    // When getting calling the pointer operator on the copied OffsetPtr
    auto* actual_pointed_to_address = static_cast<PointedType*>(*ptr_to_offset_ptr);

    // Then returned pointer should be the same as the initial pointed-to address
    EXPECT_EQ(actual_pointed_to_address, reinterpret_cast<PointedType*>(params.pointed_to_address));
}

TEST_P(OffsetPtrCopyStartAndEndChecksFixture, ArrowOperatorReturnsCorrectPointer)
{
    const auto& params = GetParam();

    // Given an OffsetPtr pointing to an object that was copied to another location
    auto* ptr_to_offset_ptr = CreateOffsetPtr<PointedType>(params.offset_ptr_address, params.pointed_to_address);
    ptr_to_offset_ptr = CopyOffsetPtrTo(*ptr_to_offset_ptr, params.copied_offset_ptr_address);

    // When getting calling the arrow operator on the copied OffsetPtr
    auto* actual_pointed_to_address = (*ptr_to_offset_ptr).operator->();

    // Then returned pointer should be the same as the initial pointed-to address
    EXPECT_EQ(actual_pointed_to_address, reinterpret_cast<PointedType*>(params.pointed_to_address));
}

// clang-format off
INSTANTIATE_TEST_SUITE_P(CopiedOffsetPtrStartAndEndChecksPass, OffsetPtrCopyStartAndEndChecksFixture,
    ::testing::ValuesIn(GenerateOffsetPtrAddressesThatPassBoundsChecks(gMemoryPool))
);
// clang-format on

using OffsetPtrCopyStartAndEndChecksDeathFixture = CopiedOffsetPtrBoundsCheckParamaterisedFixture;
TEST_P(OffsetPtrCopyStartAndEndChecksDeathFixture, GettingOffsetPointerTerminates)
{
    const auto& params = GetParam();

    // Given an OffsetPtr pointing to an object that was copied to another location
    auto* ptr_to_offset_ptr = CreateOffsetPtr<PointedType>(params.offset_ptr_address, params.pointed_to_address);
    ptr_to_offset_ptr = CopyOffsetPtrTo(*ptr_to_offset_ptr, params.copied_offset_ptr_address);

    // When getting a raw pointer from the copied OffsetPtr
    // Then the program should terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = ptr_to_offset_ptr->get());
}

TEST_P(OffsetPtrCopyStartAndEndChecksDeathFixture, GettingOffsetPointerWithTypedGetTerminates)
{
    const auto& params = GetParam();

    // Given an OffsetPtr pointing to an object that was copied to another location
    auto* ptr_to_offset_ptr = CreateOffsetPtr<void>(params.offset_ptr_address, params.pointed_to_address);
    ptr_to_offset_ptr = CopyOffsetPtrTo(*ptr_to_offset_ptr, params.copied_offset_ptr_address);

    // When getting a raw pointer from the copied OffsetPtr<void> by explicitly specifying the type
    // Then the program should terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = ptr_to_offset_ptr->template get<PointedType>());
}

TEST_P(OffsetPtrCopyStartAndEndChecksDeathFixture, GettingOffsetPointerWithSizedGetTerminates)
{
    const auto& params = GetParam();

    // Given an OffsetPtr pointing to an object that was copied to another location
    auto* ptr_to_offset_ptr = CreateOffsetPtr<void>(params.offset_ptr_address, params.pointed_to_address);
    ptr_to_offset_ptr = CopyOffsetPtrTo(*ptr_to_offset_ptr, params.copied_offset_ptr_address);

    // When getting a raw pointer from the OffsetPtr<void> by explicitly specifying the size of the type
    // Then the program should terminate
    const auto pointed_type_size = sizeof(PointedType);
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = ptr_to_offset_ptr->get(pointed_type_size));
}

TEST_P(OffsetPtrCopyStartAndEndChecksDeathFixture, DereferencingOffsetPointerTerminates)
{
    const auto& params = GetParam();

    // Given an OffsetPtr pointing to an object that was copied to another location
    auto* ptr_to_offset_ptr = CreateOffsetPtr<PointedType>(params.offset_ptr_address, params.pointed_to_address);
    ptr_to_offset_ptr = CopyOffsetPtrTo(*ptr_to_offset_ptr, params.copied_offset_ptr_address);

    // When dereferencing the copied OffsetPtr
    // Then the program should terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = *(*ptr_to_offset_ptr));
}

TEST_P(OffsetPtrCopyStartAndEndChecksDeathFixture, PointerOperatorTerminates)
{
    const auto& params = GetParam();

    // Given an OffsetPtr pointing to an object that was copied to another location
    auto* ptr_to_offset_ptr = CreateOffsetPtr<PointedType>(params.offset_ptr_address, params.pointed_to_address);
    ptr_to_offset_ptr = CopyOffsetPtrTo(*ptr_to_offset_ptr, params.copied_offset_ptr_address);

    // When getting calling the pointer operator on the copied OffsetPtr
    // Then the program should terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = static_cast<PointedType*>(*ptr_to_offset_ptr));
}

TEST_P(OffsetPtrCopyStartAndEndChecksDeathFixture, ArrowOperatorTerminates)
{
    const auto& params = GetParam();

    // Given an OffsetPtr pointing to an object that was copied to another location
    auto* ptr_to_offset_ptr = CreateOffsetPtr<PointedType>(params.offset_ptr_address, params.pointed_to_address);
    ptr_to_offset_ptr = CopyOffsetPtrTo(*ptr_to_offset_ptr, params.copied_offset_ptr_address);

    // When getting calling the arrow operator on the copied OffsetPtr
    // Then the program should terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = (*ptr_to_offset_ptr).operator->());
}

// clang-format off
INSTANTIATE_TEST_SUITE_P(CopiedOffsetPtrEndChecksFail, OffsetPtrCopyStartAndEndChecksDeathFixture,
    ::testing::ValuesIn(GenerateOffsetPtrAddressesThatFailBoundChecks(gMemoryPool, gSecondMemoryPool))
);
// clang-format on

}  // namespace
}  // namespace score::memory::shared::test
