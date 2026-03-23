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
#include "score/memory/shared/offset_ptr.h"

#include "score/memory/shared/pointer_arithmetic_util.h"
#include "score/memory/shared/test_offset_ptr/bounds_check_memory_pool.h"

#include <score/assert_support.hpp>

#include <gtest/gtest.h>
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
    BoundsCheckMemoryPool<PointedType>::MemoryPool::iterator ptr_to_offset_ptr;
    BoundsCheckMemoryPool<PointedType>::MemoryPool::iterator pointed_to_address;
};

class OffsetPtrBoundsCheckParamaterisedFixture : public ::testing::TestWithParam<TestParams>
{
  public:
    // Guard wrapper to reset the memory pool between each test (since its lifetime is static so it will not be created
    // / destroyed for every test)
    BoundsCheckMemoryPoolGuard<PointedType> memory_pool_guard_{gMemoryPool};
    BoundsCheckMemoryPoolGuard<PointedType> memory_pool_guard_2_{gSecondMemoryPool};
    MyBoundedMemoryResource memory_resource_{{gMemoryPool.GetStartOfValidRegion(), gMemoryPool.GetEndOfValidRegion()}};
    MyBoundedMemoryResource memory_resource_2_{
        {gSecondMemoryPool.GetStartOfValidRegion(), gSecondMemoryPool.GetEndOfValidRegion()}};
};

TEST(OffsetPtrBoundsCheckDeathTest, IndexDereferenceGoesOutOfMemoryRegion)
{
    // Given a memory resource with a certain size, where an array is allocated
    MyBoundedMemoryResource memory_resource_{{gMemoryPool.GetStartOfValidRegion(), gMemoryPool.GetEndOfValidRegion()}};
    constexpr std::size_t arraySize{3};

    auto* raw_ptr = memory_resource_.allocate(sizeof(std::uint8_t) * arraySize);
    raw_ptr = new (raw_ptr) std::uint8_t[arraySize]{1, 2, 3};

    auto* const int_ptr = static_cast<std::uint8_t*>(raw_ptr);

    auto* const ptr_to_offset_ptr = memory_resource_.template construct<OffsetPtr<std::uint8_t>>(int_ptr);
    auto offset_ptr = *ptr_to_offset_ptr;

    // When accessing that array through the []-operator which goes out of the memory region
    // Then the bounds checking kicks in
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(offset_ptr[gMemoryPool.GetEndOfValidRegion() - gMemoryPool.GetStartOfValidRegion()]);
}

std::vector<TestParams> GenerateOffsetPtrAddressesThatPassBoundsChecks(BoundsCheckMemoryPool<PointedType>& memory_pool)
{
    // OffsetPtr that lies inside valid range will not die if start address of pointed-to object is in the same range.
    // clang-format off
    return {TestParams{memory_pool.GetOffsetPtrAddressInValidRange(), memory_pool.GetPointedToAddressInValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressInValidRange(), memory_pool.GetPointedToAddressFinishingAtEndAddress()},
            TestParams{memory_pool.GetOffsetPtrAddressFinishingAtEndAddress(), memory_pool.GetPointedToAddressInValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressFinishingAtEndAddress(), memory_pool.GetPointedToAddressFinishingAtEndAddress()}};
    // clang-format on
}

std::vector<TestParams> GenerateOffsetPtrAddressesThatDoNotTriggerChecks(
    BoundsCheckMemoryPool<PointedType>& memory_pool)
{
    // OffsetPtr that lies outside valid range of a registered memory resource does no bounds checking so will never
    // die regardless of location of pointed-to object (even if pointed-to object is in valid range)
    // clang-format off
    return {TestParams{memory_pool.GetOffsetPtrAddressBeforeValidRange(), memory_pool.GetPointedToAddressBeforeValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressBeforeValidRange(), memory_pool.GetPointedToAddressAfterValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressBeforeValidRange(), memory_pool.GetPointedToAddressFinishingAtEndAddress()},
            TestParams{memory_pool.GetOffsetPtrAddressBeforeValidRange(), memory_pool.GetPointedToAddressOverlappingWithStartRange()},
            TestParams{memory_pool.GetOffsetPtrAddressBeforeValidRange(), memory_pool.GetPointedToAddressOverlappingWithEndRange()},
            TestParams{memory_pool.GetOffsetPtrAddressAfterValidRange(), memory_pool.GetPointedToAddressBeforeValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressAfterValidRange(), memory_pool.GetPointedToAddressAfterValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressAfterValidRange(), memory_pool.GetPointedToAddressFinishingAtEndAddress()},
            TestParams{memory_pool.GetOffsetPtrAddressAfterValidRange(), memory_pool.GetPointedToAddressOverlappingWithStartRange()},
            TestParams{memory_pool.GetOffsetPtrAddressAfterValidRange(), memory_pool.GetPointedToAddressOverlappingWithEndRange()}};
    // clang-format on
}

std::vector<TestParams> GenerateOffsetPtrAddressesThatFailsBoundsChecks(
    BoundsCheckMemoryPool<PointedType>& memory_pool,
    BoundsCheckMemoryPool<PointedType>& memory_pool_2)
{
    // clang-format off
    return {
            // OffsetPtr that lies inside valid range will die if pointed-to object is not completely in the same range.
            TestParams{memory_pool.GetOffsetPtrAddressInValidRange(), memory_pool.GetPointedToAddressBeforeValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressInValidRange(), memory_pool.GetPointedToAddressAfterValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressInValidRange(), memory_pool.GetPointedToAddressOverlappingWithStartRange()},
            TestParams{memory_pool.GetOffsetPtrAddressInValidRange(), memory_pool.GetPointedToAddressOverlappingWithEndRange()},
            TestParams{memory_pool.GetOffsetPtrAddressInValidRange(), memory_pool.GetPointedToAddressOverlappingWithEndRange()},
            TestParams{memory_pool.GetOffsetPtrAddressFinishingAtEndAddress(), memory_pool.GetPointedToAddressBeforeValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressFinishingAtEndAddress(), memory_pool.GetPointedToAddressAfterValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressFinishingAtEndAddress(), memory_pool.GetPointedToAddressOverlappingWithStartRange()},
            TestParams{memory_pool.GetOffsetPtrAddressFinishingAtEndAddress(), memory_pool.GetPointedToAddressOverlappingWithEndRange()},
            TestParams{memory_pool.GetOffsetPtrAddressFinishingAtEndAddress(), memory_pool.GetPointedToAddressOverlappingWithEndRange()},

            // OffsetPtr that lies inside valid range will die if pointed-to object is in a different memory region.
            TestParams{memory_pool.GetOffsetPtrAddressInValidRange(), memory_pool_2.GetPointedToAddressInValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressInValidRange(), memory_pool_2.GetPointedToAddressFinishingAtEndAddress()},
            TestParams{memory_pool.GetOffsetPtrAddressInValidRange(), memory_pool_2.GetPointedToAddressOverlappingWithEndRange()},
            TestParams{memory_pool.GetOffsetPtrAddressInValidRange(), memory_pool_2.GetPointedToAddressOverlappingWithStartRange()},
            TestParams{memory_pool.GetOffsetPtrAddressInValidRange(), memory_pool_2.GetPointedToAddressOverlappingWithEndRange()},
            TestParams{memory_pool.GetOffsetPtrAddressFinishingAtEndAddress(), memory_pool_2.GetPointedToAddressInValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressFinishingAtEndAddress(), memory_pool_2.GetPointedToAddressFinishingAtEndAddress()},
            TestParams{memory_pool.GetOffsetPtrAddressFinishingAtEndAddress(), memory_pool_2.GetPointedToAddressOverlappingWithEndRange()},
            TestParams{memory_pool.GetOffsetPtrAddressFinishingAtEndAddress(), memory_pool_2.GetPointedToAddressOverlappingWithStartRange()},
            TestParams{memory_pool.GetOffsetPtrAddressFinishingAtEndAddress(), memory_pool_2.GetPointedToAddressOverlappingWithEndRange()},

            // OffsetPtr that does not fit completely within the registered memory resource will always die.
            TestParams{memory_pool.GetOffsetPtrAddressOverlappingWithStartRange(), memory_pool.GetPointedToAddressInValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressOverlappingWithStartRange(), memory_pool.GetPointedToAddressBeforeValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressOverlappingWithStartRange(), memory_pool.GetPointedToAddressAfterValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressOverlappingWithStartRange(), memory_pool.GetPointedToAddressOverlappingWithStartRange()},
            TestParams{memory_pool.GetOffsetPtrAddressOverlappingWithStartRange(), memory_pool.GetPointedToAddressOverlappingWithEndRange()},
            TestParams{memory_pool.GetOffsetPtrAddressOverlappingWithStartRange(), memory_pool.GetPointedToAddressFinishingAtEndAddress()},
            TestParams{memory_pool.GetOffsetPtrAddressOverlappingWithEndRange(), memory_pool.GetPointedToAddressInValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressOverlappingWithEndRange(), memory_pool.GetPointedToAddressBeforeValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressOverlappingWithEndRange(), memory_pool.GetPointedToAddressAfterValidRange()},
            TestParams{memory_pool.GetOffsetPtrAddressOverlappingWithEndRange(), memory_pool.GetPointedToAddressOverlappingWithStartRange()},
            TestParams{memory_pool.GetOffsetPtrAddressOverlappingWithEndRange(), memory_pool.GetPointedToAddressOverlappingWithEndRange()},
            TestParams{memory_pool.GetOffsetPtrAddressOverlappingWithEndRange(), memory_pool.GetPointedToAddressFinishingAtEndAddress()}};
    // clang-format on
}

using OffsetPtrNoBoundsCheckFixture = OffsetPtrBoundsCheckParamaterisedFixture;
TEST_P(OffsetPtrNoBoundsCheckFixture, CreatingOffsetPtrDoesNotTerminate)
{
    const auto& params = GetParam();

    // When creating an OffsetPtr
    score::cpp::ignore = CreateOffsetPtr<PointedType>(params.ptr_to_offset_ptr, params.pointed_to_address);

    // Then the program should not terminate
}

// Since gtest doesn't provide a convenient way to have typed and paramaterised tests, we simply duplicate the test
// cases for a void PointedType
TEST_P(OffsetPtrNoBoundsCheckFixture, CreatingVoidOffsetPtrDoesNotTerminate)
{
    const auto& params = GetParam();

    // When creating an OffsetPtr
    score::cpp::ignore = CreateOffsetPtr<void>(params.ptr_to_offset_ptr, params.pointed_to_address);

    // Then the program should not terminate
}

// clang-format off
INSTANTIATE_TEST_SUITE_P(OffsetPtrNoBoundsCheckBoundsChecksWouldPass, OffsetPtrNoBoundsCheckFixture,
    ::testing::ValuesIn(GenerateOffsetPtrAddressesThatPassBoundsChecks(gMemoryPool))
);
INSTANTIATE_TEST_SUITE_P(OffsetPtrNoBoundsCheckBoundsChecksNotRequired, OffsetPtrNoBoundsCheckFixture,
    ::testing::ValuesIn(GenerateOffsetPtrAddressesThatDoNotTriggerChecks(gMemoryPool))
);
INSTANTIATE_TEST_SUITE_P(OffsetPtrNoBoundsCheckBoundsChecksWouldFail, OffsetPtrNoBoundsCheckFixture,
    ::testing::ValuesIn(GenerateOffsetPtrAddressesThatFailsBoundsChecks(gMemoryPool, gSecondMemoryPool))
);
// clang-format on

using OffsetPtrBoundsCheckFixture = OffsetPtrBoundsCheckParamaterisedFixture;
TEST_P(OffsetPtrBoundsCheckFixture, DereferencingOffsetPtrReturnsCorrectValue)
{
    RecordProperty("Verifies", "SCR-5899238");
    RecordProperty("Description", "Checks that dereferencing performs bounds checking");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const auto& params = GetParam();

    // Given a memory region registered with the MemoryResourceRegistry and an OffsetPtr
    auto* const ptr_to_offset_ptr = CreateOffsetPtr<PointedType>(params.ptr_to_offset_ptr, params.pointed_to_address);

    // When dereferencing the offset_ptr
    const auto actual_value = *(*ptr_to_offset_ptr);

    // Then the correct value should be returned
    EXPECT_EQ(actual_value, *reinterpret_cast<PointedType*>(params.pointed_to_address));
}

TEST_P(OffsetPtrBoundsCheckFixture, GettingOffsetPtr)
{
    RecordProperty("Verifies", "SCR-5899238");
    RecordProperty("Description", "Checks that calling get() performs bounds checking");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const auto& params = GetParam();

    // Given a memory region registered with the MemoryResourceRegistry and an OffsetPtr
    auto* const ptr_to_offset_ptr = CreateOffsetPtr<PointedType>(params.ptr_to_offset_ptr, params.pointed_to_address);

    // When getting a raw pointer from the OffsetPtr
    auto* actual_pointed_to_address = ptr_to_offset_ptr->get();

    // Then returned pointer should be the same as the initial pointed-to address
    EXPECT_EQ(actual_pointed_to_address, reinterpret_cast<PointedType*>(params.pointed_to_address));
}

TEST_P(OffsetPtrBoundsCheckFixture, GettingOffsetPtrWithTypedGet)
{
    RecordProperty("Verifies", "SCR-5899238");
    RecordProperty("Description", "Checks that calling typed get() performs bounds checking");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const auto& params = GetParam();

    // Given a memory region registered with the MemoryResourceRegistry and an OffsetPtr
    auto* const ptr_to_offset_ptr = CreateOffsetPtr<void>(params.ptr_to_offset_ptr, params.pointed_to_address);

    // When getting a raw pointer from the OffsetPtr<void> by explicitly specifying the type
    auto* actual_pointed_to_address = ptr_to_offset_ptr->get<PointedType>();

    // Then returned pointer should be the same as the initial pointed-to address
    EXPECT_EQ(actual_pointed_to_address, reinterpret_cast<PointedType*>(params.pointed_to_address));
}

TEST_P(OffsetPtrBoundsCheckFixture, GettingOffsetPtrWithSizedGet)
{
    RecordProperty("Verifies", "SCR-5899238");
    RecordProperty("Description", "Checks that calling get() that accepts size as argument performs bounds checking");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const auto& params = GetParam();

    // Given a memory region registered with the MemoryResourceRegistry and an OffsetPtr
    auto* const ptr_to_offset_ptr = CreateOffsetPtr<void>(params.ptr_to_offset_ptr, params.pointed_to_address);

    // When getting a raw pointer from the OffsetPtr<void> by explicitly specifying the size of the type
    const auto pointed_type_size = sizeof(PointedType);
    auto* actual_pointed_to_address = ptr_to_offset_ptr->get(pointed_type_size);

    // Then returned pointer should be the same as the initial pointed-to address
    EXPECT_EQ(actual_pointed_to_address, params.pointed_to_address);
}

TEST_P(OffsetPtrBoundsCheckFixture, PointerOperatorReturnsCorrectPointer)
{
    const auto& params = GetParam();

    // Given a memory region registered with the MemoryResourceRegistry and an OffsetPtr
    auto* const ptr_to_offset_ptr = CreateOffsetPtr<PointedType>(params.ptr_to_offset_ptr, params.pointed_to_address);

    // When getting calling the pointer operator on the OffsetPtr
    auto* actual_pointed_to_address = static_cast<PointedType*>(*ptr_to_offset_ptr);

    // Then returned pointer should be the same as the initial pointed-to address
    EXPECT_EQ(actual_pointed_to_address, reinterpret_cast<PointedType*>(params.pointed_to_address));
}

TEST_P(OffsetPtrBoundsCheckFixture, ArrowOperatorReturnsCorrectPointer)
{
    const auto& params = GetParam();

    // Given a memory region registered with the MemoryResourceRegistry and an OffsetPtr
    auto* const ptr_to_offset_ptr = CreateOffsetPtr<PointedType>(params.ptr_to_offset_ptr, params.pointed_to_address);

    // When getting calling the arrow operator on the OffsetPtr
    auto* actual_pointed_to_address = (*ptr_to_offset_ptr).operator->();

    // Then returned pointer should be the same as the initial pointed-to address
    EXPECT_EQ(actual_pointed_to_address, reinterpret_cast<PointedType*>(params.pointed_to_address));
}

// clang-format off
INSTANTIATE_TEST_SUITE_P(OffsetPtrCreationStartChecksPass, OffsetPtrBoundsCheckFixture,
    ::testing::ValuesIn(GenerateOffsetPtrAddressesThatPassBoundsChecks(gMemoryPool))
);
INSTANTIATE_TEST_SUITE_P(OffsetPtrCreationStartChecksNotRequired, OffsetPtrBoundsCheckFixture,
    ::testing::ValuesIn(GenerateOffsetPtrAddressesThatDoNotTriggerChecks(gMemoryPool))
);
// clang-format on

using OffsetPtrBoundsCheckDeathFixture = OffsetPtrBoundsCheckParamaterisedFixture;
TEST_P(OffsetPtrBoundsCheckDeathFixture, DereferencingOffsetPtrTerminates)
{
    RecordProperty("Verifies", "SCR-5899238");
    RecordProperty("Description", "Checks that dereferencing performs bounds checking");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const auto& params = GetParam();

    // Given a memory region registered with the MemoryResourceRegistry and an OffsetPtr
    auto* const ptr_to_offset_ptr = CreateOffsetPtr<PointedType>(params.ptr_to_offset_ptr, params.pointed_to_address);

    // When dereferencing the offset_ptr
    // Then the program should terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = *(*ptr_to_offset_ptr));
}

TEST_P(OffsetPtrBoundsCheckDeathFixture, OffsetPtrGetTerminates)
{
    RecordProperty("Verifies", "SCR-5899238");
    RecordProperty("Description", "Checks that calling get() performs bounds checking");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const auto& params = GetParam();

    // Given a memory region registered with the MemoryResourceRegistry and an OffsetPtr
    auto* const ptr_to_offset_ptr = CreateOffsetPtr<PointedType>(params.ptr_to_offset_ptr, params.pointed_to_address);

    // When calling get() on the offset_ptr
    // Then the program should terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = ptr_to_offset_ptr->get());
}

TEST_P(OffsetPtrBoundsCheckDeathFixture, HandlesRegularArrayIndexAndPerformanceBoundsChecking)
{
    const auto& params = GetParam();

    // Given a memory region registered with the MemoryResourceRegistry and an OffsetPtr
    auto* const ptr_to_offset_ptr = CreateOffsetPtr<PointedType>(params.ptr_to_offset_ptr, params.pointed_to_address);

    // When calling [[]-operator on the offset_ptr
    // Then the program should terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = (*ptr_to_offset_ptr)[0]);
}

TEST_P(OffsetPtrBoundsCheckDeathFixture, OffsetPtrTypedGetTerminates)
{
    RecordProperty("Verifies", "SCR-5899238");
    RecordProperty("Description", "Checks that calling typed get() performs bounds checking");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const auto& params = GetParam();

    // Given a memory region registered with the MemoryResourceRegistry and an OffsetPtr
    auto* const ptr_to_offset_ptr = CreateOffsetPtr<void>(params.ptr_to_offset_ptr, params.pointed_to_address);

    // When getting a raw pointer from the copied OffsetPtr<void> by explicitly specifying the type
    // Then the program should terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = ptr_to_offset_ptr->template get<PointedType>());
}

TEST_P(OffsetPtrBoundsCheckDeathFixture, OffsetPtrSizedGetTerminates)
{
    RecordProperty("Verifies", "SCR-5899238");
    RecordProperty("Description", "Checks that calling get() that takes size as argument performs bounds checking");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const auto& params = GetParam();

    // Given a memory region registered with the MemoryResourceRegistry and an OffsetPtr
    auto* const ptr_to_offset_ptr = CreateOffsetPtr<void>(params.ptr_to_offset_ptr, params.pointed_to_address);

    // When getting a raw pointer from the OffsetPtr<void> by explicitly specifying the size of the type
    // Then the program should terminate
    const auto pointed_type_size = sizeof(PointedType);
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = ptr_to_offset_ptr->get(pointed_type_size));
}

TEST_P(OffsetPtrBoundsCheckDeathFixture, PointerOperatorTerminates)
{
    const auto& params = GetParam();

    // Given a memory region registered with the MemoryResourceRegistry and an OffsetPtr
    auto* const ptr_to_offset_ptr = CreateOffsetPtr<PointedType>(params.ptr_to_offset_ptr, params.pointed_to_address);

    // When getting calling the pointer operator on the OffsetPtr
    // Then the program should terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = static_cast<PointedType*>(*ptr_to_offset_ptr));
}

TEST_P(OffsetPtrBoundsCheckDeathFixture, ArrowOperatorTerminates)
{
    RecordProperty("Verifies", "SCR-5899238");
    RecordProperty("Description", "Checks that calling get() performs bounds checking");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const auto& params = GetParam();

    // Given a memory region registered with the MemoryResourceRegistry and an OffsetPtr
    auto* const ptr_to_offset_ptr = CreateOffsetPtr<PointedType>(params.ptr_to_offset_ptr, params.pointed_to_address);

    // When getting calling the arrow operator on the OffsetPtr
    // Then the program should terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = (*ptr_to_offset_ptr).operator->());
}

// clang-format off
INSTANTIATE_TEST_SUITE_P(OffsetPtrBoundChecksFail, OffsetPtrBoundsCheckDeathFixture, ::testing::ValuesIn(
    GenerateOffsetPtrAddressesThatFailsBoundsChecks(gMemoryPool, gSecondMemoryPool)
));
// clang-format on

}  // namespace
}  // namespace score::memory::shared::test
