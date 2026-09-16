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
#include "score/mw/com/impl/mocking/test_type_utilities.h"

#include "score/mw/com/impl/handle_type.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>

namespace score::mw::com::impl
{
namespace
{

const std::uint16_t kDummyUniqueIdentifier{1U};
const std::uint16_t kDummyUniqueIdentifier2{2U};

class TestTypesFixture : public ::testing::Test
{
  public:
    ~TestTypesFixture()
    {
        ResetInstanceIdentifierConfiguration();
    }
};

using TestTypesDummyHandleCreationFixture = TestTypesFixture;
TEST_F(TestTypesDummyHandleCreationFixture, HashIsSameForSameUniqueIdentifiers)
{
    // Given two dummy HandleTypes created from the same unique identifier
    auto unit = MakeFakeHandle(kDummyUniqueIdentifier);
    auto unit_2 = MakeFakeHandle(kDummyUniqueIdentifier);

    // When hashing each
    const auto hash_result = std::hash<HandleType>{}.operator()(unit);
    const auto hash_result_2 = std::hash<HandleType>{}.operator()(unit_2);

    // Then comparing the results should return true
    EXPECT_EQ(hash_result, hash_result_2);
}

TEST_F(TestTypesDummyHandleCreationFixture, HashIsDifferentForDifferentUniqueIdentifiers)
{
    // Given two dummy HandleTypes created from different unique identifiers
    auto unit = MakeFakeHandle(kDummyUniqueIdentifier);
    auto unit_2 = MakeFakeHandle(kDummyUniqueIdentifier2);

    // When hashing
    const auto hash_result = std::hash<HandleType>{}.operator()(unit);
    const auto hash_result_2 = std::hash<HandleType>{}.operator()(unit_2);

    // Then comparing the results should return false
    EXPECT_NE(hash_result, hash_result_2);
}

using TestTypesDummyInstanceIdentifierCreationFixture = TestTypesFixture;
TEST_F(TestTypesDummyInstanceIdentifierCreationFixture, ToStringIsSameForSameUniqueIdentifiers)
{
    // Given two dummy InstanceIdentifier created from the same unique identifier
    auto unit = MakeFakeInstanceIdentifier(kDummyUniqueIdentifier);
    auto unit_2 = MakeFakeInstanceIdentifier(kDummyUniqueIdentifier);

    // When calling ToString on each
    const auto stringified_instance_identifier = unit.ToString();
    const auto stringified_instance_identifier_2 = unit_2.ToString();

    // Then comparing the results should return true
    EXPECT_EQ(stringified_instance_identifier, stringified_instance_identifier_2);
}

TEST_F(TestTypesDummyInstanceIdentifierCreationFixture, ToStringIsDifferentForDifferentUniqueIdentifiers)
{
    // Given two dummy InstanceIdentifier created from different unique identifiers
    auto unit = MakeFakeInstanceIdentifier(kDummyUniqueIdentifier);
    auto unit_2 = MakeFakeInstanceIdentifier(kDummyUniqueIdentifier2);

    // When calling ToString on each
    const auto stringified_instance_identifier = unit.ToString();
    const auto stringified_instance_identifier_2 = unit_2.ToString();

    // Then comparing the results should return false
    EXPECT_NE(stringified_instance_identifier, stringified_instance_identifier_2);
}

TEST_F(TestTypesDummyInstanceIdentifierCreationFixture, SameUniqueIdentifiersCompareEqual)
{
    // Given two dummy InstanceIdentifier created from the same unique identifier
    auto unit = MakeFakeInstanceIdentifier(kDummyUniqueIdentifier);
    auto unit_2 = MakeFakeInstanceIdentifier(kDummyUniqueIdentifier);

    // When checking equality of the two InstanceIdentifiers
    const auto comparison_result = unit == unit_2;

    // Then the result should be true
    EXPECT_TRUE(comparison_result);
}

TEST_F(TestTypesDummyInstanceIdentifierCreationFixture, DifferentUniqueIdentifiersCompareDifferent)
{
    // Given two dummy InstanceIdentifier created from different unique identifiers
    auto unit = MakeFakeInstanceIdentifier(kDummyUniqueIdentifier);
    auto unit_2 = MakeFakeInstanceIdentifier(kDummyUniqueIdentifier2);

    // When checking equality of the two InstanceIdentifiers
    const auto comparison_result = unit == unit_2;

    // Then the result should be false
    EXPECT_FALSE(comparison_result);
}

TEST_F(TestTypesDummyInstanceIdentifierCreationFixture, LessCompareReturnsTrueIfUniqueIdentifierIsSmaller)
{
    // Given two dummy InstanceIdentifier created from different unique identifiers in which the first identifier is
    // smaller than the second
    auto unit = MakeFakeInstanceIdentifier(0U);
    auto unit_2 = MakeFakeInstanceIdentifier(1U);

    // When comparing the two InstanceIdentifiers
    const auto comparison_result = unit < unit_2;

    // Then the result should be true
    EXPECT_TRUE(comparison_result);
}

TEST_F(TestTypesDummyInstanceIdentifierCreationFixture, LessCompareReturnsFalseIfUniqueIdentifierIsLarger)
{
    // Given two dummy InstanceIdentifier created from different unique identifiers in which the first identifier is
    // larger than the second
    auto unit = MakeFakeInstanceIdentifier(1U);
    auto unit_2 = MakeFakeInstanceIdentifier(0U);

    // When comparing the two InstanceIdentifiers
    const auto comparison_result = unit < unit_2;

    // Then the result should be false
    EXPECT_FALSE(comparison_result);
}

TEST_F(TestTypesDummyInstanceIdentifierCreationFixture, HashIsSameForSameUniqueIdentifiers)
{
    // Given two dummy InstanceIdentifier created from the same unique identifier
    auto unit = MakeFakeInstanceIdentifier(kDummyUniqueIdentifier);
    auto unit_2 = MakeFakeInstanceIdentifier(kDummyUniqueIdentifier);

    // When hashing each
    const auto hash_result = std::hash<InstanceIdentifier>{}.operator()(unit);
    const auto hash_result_2 = std::hash<InstanceIdentifier>{}.operator()(unit_2);

    // Then comparing the results should return true
    EXPECT_EQ(hash_result, hash_result_2);
}

TEST_F(TestTypesDummyInstanceIdentifierCreationFixture, HashIsDifferentForDifferentUniqueIdentifiers)
{
    // Given two dummy InstanceIdentifier created from different unique identifiers
    auto unit = MakeFakeInstanceIdentifier(kDummyUniqueIdentifier);
    auto unit_2 = MakeFakeInstanceIdentifier(kDummyUniqueIdentifier2);

    // When hashing
    const auto hash_result = std::hash<InstanceIdentifier>{}.operator()(unit);
    const auto hash_result_2 = std::hash<InstanceIdentifier>{}.operator()(unit_2);

    // Then comparing the results should return false
    EXPECT_NE(hash_result, hash_result_2);
}

using TestTypesSampleAllocateePtrCreationFixture = TestTypesFixture;
TEST_F(TestTypesSampleAllocateePtrCreationFixture, CanCreateFakeSampleAllocateePtrWithUniquePtr)
{
    // Given a sample allocatee location/variable
    std::uint32_t pointed_to_value{10U};

    // When creating a fake SampleAllocateePtr pointinmg to it
    auto sample_allocatee_ptr = MakeFakeSampleAllocateePtr(&pointed_to_value);

    // Then the SampleAllocateePtr points to the same data
    ASSERT_TRUE(sample_allocatee_ptr);
    EXPECT_EQ(*sample_allocatee_ptr, pointed_to_value);
}

using TestTypesSamplePtrCreationFixture = TestTypesFixture;
TEST_F(TestTypesSamplePtrCreationFixture, CanCreateFakeSamplePtrWithUniquePtr)
{
    // Given a unique_ptr pointing to some value
    const std::uint32_t pointed_to_value{10U};
    auto my_unique_ptr = std::make_unique<std::uint32_t>(pointed_to_value);

    // When creating a fake SamplePtr from the unique_ptr
    auto sample_ptr = MakeFakeSamplePtr(std::move(my_unique_ptr));

    // Then the SamplePtr points to the same data
    ASSERT_TRUE(sample_ptr);
    EXPECT_EQ(*sample_ptr, pointed_to_value);
}

}  // namespace
}  // namespace score::mw::com::impl
