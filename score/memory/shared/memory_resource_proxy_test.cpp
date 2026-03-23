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
#include "memory_resource_proxy.h"

#include "fake/my_bounded_memory_resource.h"
#include "fake/my_memory_resource.h"
#include "memory_resource_registry.h"
#include "score/memory/shared/pointer_arithmetic_util.h"
#include "score/memory/shared/shared_memory_test_resources.h"

#include "gtest/gtest.h"
#include <cstddef>

namespace score::memory::shared::test
{

TEST(MemoryResourceProxyComparisonTest, MemoryResourceProxiesWithSameIdsComparisonOperators)
{
    // Given 2 MemoryResourceProxies with the same memory identifier
    MemoryResourceProxy proxy1{0U};
    MemoryResourceProxy proxy2{0U};

    // Then comparing the MemoryResourceProxies works as expected
    EXPECT_TRUE(proxy1 == proxy1);
    EXPECT_TRUE(proxy1 == proxy2);
    EXPECT_FALSE(proxy1 != proxy2);
}

TEST(MemoryResourceProxyComparisonTest, MemoryResourceProxiesWithDifferentIdsComparisonOperators)
{
    // Given 2 MemoryResourceProxies with the same memory identifier
    MemoryResourceProxy proxy1{0U};
    MemoryResourceProxy proxy2{1U};

    // Then comparing the MemoryResourceProxies works as expected
    EXPECT_FALSE(proxy1 == proxy2);
    EXPECT_TRUE(proxy1 != proxy2);
}

class MemoryResourceProxyTest : public ::testing::Test
{
  public:
    void SetUp() override
    {
        MemoryResourceRegistry::getInstance().insert_resource({0U, &memoryResource0});
        MemoryResourceRegistry::getInstance().insert_resource({42U, &memoryResource42});
    }

    void TearDown() override
    {
        MemoryResourceRegistry::getInstance().clear();
    }

    MyMemoryResource memoryResource0{};
    MyMemoryResource memoryResource42{};
    MemoryResourceProxy unit{42U};
};

TEST_F(MemoryResourceProxyTest, ForwardsAllocateRequestToCorrectMemoryResource)
{
    // Given multiple registered memory resources and one respective MemoryResourceProxy
    // When allocating memory on the MemoryResourceProxy
    auto* allocatedMemory = unit.allocate(42U);

    // Then the allocation request is forwarded to the correct MemoryResource and we get a valid pointer
    EXPECT_EQ(memoryResource42.getAllocatedMemory(), 42U);
    EXPECT_NE(allocatedMemory, nullptr);
    EXPECT_EQ(memoryResource0.getAllocatedMemory(), 0U);
    unit.deallocate(allocatedMemory, 42U);
}

TEST_F(MemoryResourceProxyTest, ProperHandleIfNoAllocationIsPossible)
{
    // Given multiple registered memory resources and one respective MemoryResourceProxy
    //       and it is not possible to allocate anymore memory
    memoryResource42.setAllocationPossible(false);

    // When allocating memory on the MemoryResourceProxy
    // Then no more memory is allocated and an excpetion is thrown (which aborts)
    EXPECT_DEATH(unit.allocate(42U), "");
}

TEST_F(MemoryResourceProxyTest, ForwardsDeAllocateRequestToCorrectMemoryResource)
{
    // Given multiple registered memory resources and one respective MemoryResourceProxy
    //       where some memory is already allocated
    auto* allocatedMemory = unit.allocate(42U);

    // When de-allocating this memory on the MemoryResourceProxy
    unit.deallocate(allocatedMemory, 42U);

    // Then the de-allocation request is forwarded to the correct MemoryResource
    EXPECT_EQ(memoryResource42.getAllocatedMemory(), 0U);
    EXPECT_EQ(memoryResource0.getAllocatedMemory(), 0U);
}

TEST_F(MemoryResourceProxyTest, DeallocationRequestToNonExistingResourceIsIgnored)
{
    // Given multiple registered memory resources, but a MemoryResourceProxy
    //       pointing to an none existing resource
    MemoryResourceProxy notValidIdentifier{12U};

    // When de-allocating memory on it
    // Then it does not throw and fails silently
    EXPECT_NO_THROW(notValidIdentifier.deallocate(nullptr, 42U));
}

using MemoryResourceManagerDeathTest = MemoryResourceProxyTest;
TEST_F(MemoryResourceManagerDeathTest, ProperHandleNonExistingMemoryResource)
{
    RecordProperty("Verifies", "SCR-6223631");
    RecordProperty("Description",
                   "The MemoryRessourceProxy shall store its identifier in a way, that it can detect corruptions.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given multiple registered memory resources, but a MemoryResourceProxy pointing to a non-existing resource
    MemoryResourceProxy notValidIdentifier{12U};

    // When allocating memory
    // Then the program terminates
    EXPECT_DEATH(notValidIdentifier.allocate(42U), ".*");
}

class BoundCheckedMemoryResourceProxyTest : public ::testing::Test
{
  public:
    void TearDown() override
    {
        MemoryResourceRegistry::getInstance().clear();
    }

    MyBoundedMemoryResource memoryResource{};
};

TEST_F(BoundCheckedMemoryResourceProxyTest, AllocationIsPossibleWhenProxyInBounds)
{
    // Given a registered memory resource and its respective MemoryResourceProxy
    // When allocating memory on the MemoryResourceProxy
    ManagedMemoryResourceTestAttorney attorney(memoryResource);
    const MemoryResourceProxy* proxy = attorney.getMemoryResourceProxy();

    // Align the memory since that will be done before allocating the new memory
    const auto initial_number_allocated_bytes = memoryResource.getAllocatedMemory();
    auto* allocatedMemory = proxy->allocate(42U);

    // Then the allocation request is forwarded to the correct MemoryResource and we get a valid pointer
    const auto initial_number_allocated_bytes_after_alignment =
        CalculateAlignedSize(initial_number_allocated_bytes, alignof(std::max_align_t));
    EXPECT_EQ(memoryResource.getAllocatedMemory(), 42U + initial_number_allocated_bytes_after_alignment);
    EXPECT_EQ(allocatedMemory,
              AddOffsetToPointer(memoryResource.getBaseAddress(), initial_number_allocated_bytes_after_alignment));
    proxy->deallocate(allocatedMemory, 42U);
}

TEST_F(BoundCheckedMemoryResourceProxyTest, OutOfBoundsAllocationSucceedsWhenBoudscheckingDisabled)
{
    // Given a registered memory resource and a MemoryResourceProxy with the same ID but not in the memory region
    // managed by the ManagedMemoryResource.
    MemoryResourceProxy proxy{memoryResource.getMemoryResourceId()};

    // when disabling bounds-checking
    auto bounds_check_previously_enabled = MemoryResourceProxy::EnableBoundsChecking(false);

    // Expect, that it was previously enabled (default)
    EXPECT_TRUE(bounds_check_previously_enabled);

    // When allocating memory, program does not die.
    EXPECT_NE(proxy.allocate(42U), nullptr);
}

using BoundCheckedMemoryResourceManagerDeathTest = BoundCheckedMemoryResourceProxyTest;
TEST_F(BoundCheckedMemoryResourceManagerDeathTest, AllocationTerminatesWhenProxyOutOfBounds)
{
    RecordProperty("Verifies", "SCR-6223631");
    RecordProperty("Description",
                   "The MemoryRessourceProxy shall store its identifier in a way, that it can detect corruptions.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a registered memory resource and a MemoryResourceProxy with the same ID but not in the memory region
    // managed by the ManagedMemoryResource.
    MemoryResourceProxy proxy{memoryResource.getMemoryResourceId()};

    // When allocating memory
    // Then the program terminates
    EXPECT_DEATH(proxy.allocate(42U), ".*");
}

}  // namespace score::memory::shared::test
