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
#include "memory_resource_registry.h"

#include "score/memory/shared/managed_memory_resource.h"
#include "score/memory/shared/pointer_arithmetic_util.h"
#include "score/memory/shared/shared_memory_error.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <thread>
#include <utility>
#include <vector>

namespace score::memory::shared::test
{

class MemoryResourceRegistryTest : public ::testing::Test
{
  public:
    MemoryResourceRegistryTest() : unit{MemoryResourceRegistry::getInstance()} {}

    void TearDown() override
    {
        MemoryResourceRegistry::getInstance().clear();
    }

    MemoryResourceRegistry& unit;
};

class BasicMemoryResource : public ManagedMemoryResource
{
  public:
    BasicMemoryResource(const std::pair<std::uintptr_t, std::uintptr_t> memory_range =
                            {std::uintptr_t{1U}, std::numeric_limits<std::uintptr_t>::max()}) noexcept
        : base_address_{reinterpret_cast<void*>(memory_range.first)},
          end_address_{reinterpret_cast<void*>(memory_range.second)}
    {
    }
    MemoryResourceProxy* getMemoryResourceProxy() noexcept override
    {
        return nullptr;
    }
    void* getBaseAddress() const noexcept override
    {
        return base_address_;
    }
    void* getUsableBaseAddress() const noexcept override
    {
        return base_address_;
    }
    std::size_t GetUserAllocatedBytes() const noexcept override
    {
        return static_cast<std::size_t>(SubtractPointersBytes(end_address_, base_address_));
    };
    bool IsOffsetPtrBoundsCheckBypassingEnabled() const noexcept override
    {
        return false;
    }

  private:
    const void* getEndAddress() const noexcept override
    {
        return end_address_;
    }
    void* do_allocate(const std::size_t, std::size_t) override
    {
        return nullptr;
    }
    void do_deallocate(void*, const std::size_t, std::size_t) override {}
    bool do_is_equal(const memory_resource&) const noexcept override
    {
        return false;
    }

    void* const base_address_;
    void* const end_address_;
};

class BoundsCheckBypassingMemoryResource : public ManagedMemoryResource
{
  public:
    BoundsCheckBypassingMemoryResource(const std::pair<std::uintptr_t, std::uintptr_t> memory_range =
                                           {std::uintptr_t{1U}, std::numeric_limits<std::uintptr_t>::max()}) noexcept
        : base_address_{reinterpret_cast<void*>(memory_range.first)},
          end_address_{reinterpret_cast<void*>(memory_range.second)}
    {
    }
    MemoryResourceProxy* getMemoryResourceProxy() noexcept override
    {
        return nullptr;
    }
    void* getBaseAddress() const noexcept override
    {
        return base_address_;
    }
    void* getUsableBaseAddress() const noexcept override
    {
        return base_address_;
    }
    std::size_t GetUserAllocatedBytes() const noexcept override
    {
        return static_cast<std::size_t>(SubtractPointersBytes(end_address_, base_address_));
    };
    bool IsOffsetPtrBoundsCheckBypassingEnabled() const noexcept override
    {
        return true;
    }

  private:
    const void* getEndAddress() const noexcept override
    {
        return end_address_;
    }
    void* do_allocate(const std::size_t, std::size_t) override
    {
        return nullptr;
    }
    void do_deallocate(void*, const std::size_t, std::size_t) override {}
    bool do_is_equal(const memory_resource&) const noexcept override
    {
        return false;
    }

    void* const base_address_;
    void* const end_address_;
};

TEST_F(MemoryResourceRegistryTest, CanInsertAndReturnAMemoryResource)
{
    // Given a memory resource is inserted into the registry
    BasicMemoryResource resource{};
    const bool insert_successful = unit.insert_resource({0U, &resource});

    // When searching for a specific resource
    const auto returnedResource = unit.at(0U);

    // Then the return value of the insert operation is true
    EXPECT_TRUE(insert_successful);

    // and the correct resource is returned
    EXPECT_EQ(returnedResource, &resource);
}

TEST_F(MemoryResourceRegistryTest, CanRemoveAMemoryResource)
{
    const std::uint64_t id0{0U};
    const std::uint64_t id1{1U};

    // Given 2 memory resources with different memory regions are inserted into the registry
    BasicMemoryResource resource{{std::uintptr_t{10U}, std::uintptr_t{20U}}};
    unit.insert_resource({id0, &resource});

    BasicMemoryResource resource2{{std::uintptr_t{30U}, std::uintptr_t{40U}}};
    unit.insert_resource({id1, &resource2});

    // When removing a specific resource
    unit.remove_resource(id0);

    // Then that resource will be removed and the others will remain
    const auto returnedResource0 = unit.at(id0);
    const auto returnedResource1 = unit.at(id1);

    EXPECT_EQ(returnedResource0, nullptr);
    EXPECT_EQ(returnedResource1, &resource2);

    // And when removing the remaining resource
    unit.remove_resource(id1);

    // Then that resource will be removed
    const auto returnedResourceAfterRemoval1 = unit.at(id1);
    EXPECT_EQ(returnedResourceAfterRemoval1, nullptr);
}

TEST_F(MemoryResourceRegistryTest, CanFailOverwriteResourceOnSecondInsert)
{
    // Given a memory resource is inserted into the registry
    BasicMemoryResource resource{};
    unit.insert_resource({0U, &resource});

    // When inserting a resource with the same identifier
    BasicMemoryResource resource2{};
    const bool insert_successful = unit.insert_resource({0U, &resource2});

    // Then the return value of the insert operation is false
    EXPECT_FALSE(insert_successful);

    // and still the old resource is returned
    const auto returnedResource = unit.at(0U);
    EXPECT_EQ(returnedResource, &resource);
}

TEST_F(MemoryResourceRegistryTest, ReturnsNullptrOnNonExistingIdentifier)
{
    // Given an empty resource registry
    // When searching for a specific resource
    const auto returnedResource = unit.at(0U);

    // Then the resource is a nullptr, because it does not exist
    EXPECT_EQ(returnedResource, nullptr);
}

TEST_F(MemoryResourceRegistryTest, IsThreadSafe)
{
    // Given an empty registry

    // When accessing it from multiple threads
    std::vector<std::thread> collectionOfThreads{};
    for (std::uint32_t counter = 0U; counter < 250U; counter++)
    {
        collectionOfThreads.emplace_back(std::thread([this, counter]() {
            // Address ranges should be not overlapping and not contain 0 as this will be converted to a nullptr
            const std::uintptr_t start_address{2 * counter + 1};
            const std::uintptr_t end_address{2 * counter + 2};
            BasicMemoryResource resource{{start_address, end_address}};
            if (counter % 2 == 0)
            {
                unit.insert_resource(std::make_pair(counter, &resource));
            }
            else
            {
                unit.at(static_cast<std::uint64_t>(counter - 1));
            }
        }));
    }

    for (auto& thread : collectionOfThreads)
    {
        thread.join();
    }

    // Then we don't crash
}

class MemoryResourceRegistryMemoryBoundsTest : public MemoryResourceRegistryTest
{
  protected:
    void InsertMemoryResourcesIntoRegistry()
    {
        EXPECT_TRUE(unit.insert_resource({memory_resource_id0_, &resource_}));
        EXPECT_TRUE(unit.insert_resource({memory_resource_id1_, &resource2_}));
    }

    const MemoryResourceRegistry::MemoryResourceIdentifier memory_resource_id0_{0U};
    const MemoryResourceRegistry::MemoryResourceIdentifier memory_resource_id1_{1U};

    void* const firstBoundsStart_{reinterpret_cast<void*>(50)};
    void* const firstBoundsEnd_{reinterpret_cast<void*>(100)};
    void* const secondBoundsStart_{reinterpret_cast<void*>(150)};
    void* const secondBoundsEnd_{reinterpret_cast<void*>(200)};

    const MemoryRegionBounds firstMemoryBounds{CastPointerToInteger(firstBoundsStart_),
                                               CastPointerToInteger(firstBoundsEnd_)};
    const MemoryRegionBounds secondMemoryBounds{CastPointerToInteger(secondBoundsStart_),
                                                CastPointerToInteger(secondBoundsEnd_)};

    BasicMemoryResource resource_{{CastPointerToInteger(firstBoundsStart_), CastPointerToInteger(firstBoundsEnd_)}};
    BasicMemoryResource resource2_{{CastPointerToInteger(secondBoundsStart_), CastPointerToInteger(secondBoundsEnd_)}};
};

TEST_F(MemoryResourceRegistryMemoryBoundsTest, ReturnsNullMemoryBoundsIfRegistryIsEmpty)
{
    // Given an empty resource registry
    // When checking the memory bounds for a pointer
    const auto foundMemoryBounds = unit.GetBoundsFromAddress(reinterpret_cast<void*>(50));

    // Then null memory bounds should be returned
    EXPECT_FALSE(foundMemoryBounds.has_value());
}

TEST_F(MemoryResourceRegistryMemoryBoundsTest, ReturnsMemoryBoundsForPointersInBounds)
{
    // Given 2 memory ranges are inserted into the registry
    InsertMemoryResourcesIntoRegistry();

    // When checking the memory bounds for pointers inside the memory bounds
    auto* const pointer_in_first_bounds = reinterpret_cast<void*>(75);
    auto* const pointer_in_second_bounds = reinterpret_cast<void*>(175);

    const auto firstFoundMemoryBounds0 = unit.GetBoundsFromAddress(firstBoundsStart_);
    const auto firstFoundMemoryBounds1 = unit.GetBoundsFromAddress(pointer_in_first_bounds);
    const auto firstFoundMemoryBounds2 = unit.GetBoundsFromAddress(firstBoundsEnd_);
    const auto secondFoundMemoryBounds0 = unit.GetBoundsFromAddress(secondBoundsStart_);
    const auto secondFoundMemoryBounds1 = unit.GetBoundsFromAddress(pointer_in_second_bounds);
    const auto secondFoundMemoryBounds2 = unit.GetBoundsFromAddress(secondBoundsEnd_);

    // Then the correct bounds should be returned
    ASSERT_TRUE(firstFoundMemoryBounds0.has_value());
    ASSERT_TRUE(firstFoundMemoryBounds1.has_value());
    ASSERT_TRUE(firstFoundMemoryBounds2.has_value());
    ASSERT_TRUE(secondFoundMemoryBounds0.has_value());
    ASSERT_TRUE(secondFoundMemoryBounds1.has_value());
    ASSERT_TRUE(secondFoundMemoryBounds2.has_value());

    EXPECT_EQ(firstFoundMemoryBounds0.value(), firstMemoryBounds);
    EXPECT_EQ(firstFoundMemoryBounds1.value(), firstMemoryBounds);
    EXPECT_EQ(firstFoundMemoryBounds2.value(), firstMemoryBounds);
    EXPECT_EQ(secondFoundMemoryBounds0.value(), secondMemoryBounds);
    EXPECT_EQ(secondFoundMemoryBounds1.value(), secondMemoryBounds);
    EXPECT_EQ(secondFoundMemoryBounds2.value(), secondMemoryBounds);
}

TEST_F(MemoryResourceRegistryMemoryBoundsTest, ReturnsMemoryBoundsForPointersAsIntegersInBounds)
{
    // Given 2 memory ranges are inserted into the registry
    InsertMemoryResourcesIntoRegistry();

    // When checking the memory bounds for pointers inside the memory bounds
    const auto pointer_in_first_bounds = 75;
    const auto pointer_in_second_bounds = 175;

    const auto firstFoundMemoryBounds0 =
        unit.GetBoundsFromAddressAsInteger(reinterpret_cast<std::uintptr_t>(firstBoundsStart_));
    const auto firstFoundMemoryBounds1 = unit.GetBoundsFromAddressAsInteger(pointer_in_first_bounds);
    const auto firstFoundMemoryBounds2 =
        unit.GetBoundsFromAddressAsInteger(reinterpret_cast<std::uintptr_t>(firstBoundsEnd_));
    const auto secondFoundMemoryBounds0 =
        unit.GetBoundsFromAddressAsInteger(reinterpret_cast<std::uintptr_t>(secondBoundsStart_));
    const auto secondFoundMemoryBounds1 = unit.GetBoundsFromAddressAsInteger(pointer_in_second_bounds);
    const auto secondFoundMemoryBounds2 =
        unit.GetBoundsFromAddressAsInteger(reinterpret_cast<std::uintptr_t>(secondBoundsEnd_));

    // Then the correct bounds should be returned
    ASSERT_TRUE(firstFoundMemoryBounds0.has_value());
    ASSERT_TRUE(firstFoundMemoryBounds1.has_value());
    ASSERT_TRUE(firstFoundMemoryBounds2.has_value());
    ASSERT_TRUE(secondFoundMemoryBounds0.has_value());
    ASSERT_TRUE(secondFoundMemoryBounds1.has_value());
    ASSERT_TRUE(secondFoundMemoryBounds2.has_value());

    EXPECT_EQ(firstFoundMemoryBounds0.value(), firstMemoryBounds);
    EXPECT_EQ(firstFoundMemoryBounds1.value(), firstMemoryBounds);
    EXPECT_EQ(firstFoundMemoryBounds2.value(), firstMemoryBounds);
    EXPECT_EQ(secondFoundMemoryBounds0.value(), secondMemoryBounds);
    EXPECT_EQ(secondFoundMemoryBounds1.value(), secondMemoryBounds);
    EXPECT_EQ(secondFoundMemoryBounds2.value(), secondMemoryBounds);
}

TEST_F(MemoryResourceRegistryMemoryBoundsTest, ReturnsNullMemoryBoundsForPointersOutOfBounds)
{
    // Given 2 memory ranges are inserted into the registry
    InsertMemoryResourcesIntoRegistry();

    // When checking the memory bounds for pointers outside the memory bounds
    const auto outOfBoundsFoundMemoryBounds0 = unit.GetBoundsFromAddress(reinterpret_cast<void*>(10));
    const auto outOfBoundsFoundMemoryBounds1 = unit.GetBoundsFromAddress(reinterpret_cast<void*>(110));
    const auto outOfBoundsFoundMemoryBounds2 = unit.GetBoundsFromAddress(reinterpret_cast<void*>(210));

    // Then no memory bounds should be returned
    EXPECT_FALSE(outOfBoundsFoundMemoryBounds0.has_value());
    EXPECT_FALSE(outOfBoundsFoundMemoryBounds1.has_value());
    EXPECT_FALSE(outOfBoundsFoundMemoryBounds2.has_value());
}

TEST_F(MemoryResourceRegistryMemoryBoundsTest, ReturnsNullMemoryBoundsForPointersAsIntegersOutOfBounds)
{
    // Given 2 memory ranges are inserted into the registry
    InsertMemoryResourcesIntoRegistry();

    // When checking the memory bounds for pointers as integers outside the memory bounds
    const auto outOfBoundsFoundMemoryBounds0 = unit.GetBoundsFromAddressAsInteger(10);
    const auto outOfBoundsFoundMemoryBounds1 = unit.GetBoundsFromAddressAsInteger(110);
    const auto outOfBoundsFoundMemoryBounds2 = unit.GetBoundsFromAddressAsInteger(210);

    // Then no memory bounds should be returned
    EXPECT_FALSE(outOfBoundsFoundMemoryBounds0.has_value());
    EXPECT_FALSE(outOfBoundsFoundMemoryBounds1.has_value());
    EXPECT_FALSE(outOfBoundsFoundMemoryBounds2.has_value());
}

TEST_F(MemoryResourceRegistryMemoryBoundsTest, ReturnsMemoryBoundsFromIdentifier)
{
    // Given 2 memory ranges are inserted into the registry
    InsertMemoryResourcesIntoRegistry();

    // When getting the memory bounds from the identifer
    const auto firstFoundMemoryBounds = unit.GetBoundsFromIdentifier(memory_resource_id0_);
    const auto secondFoundMemoryBounds = unit.GetBoundsFromIdentifier(memory_resource_id1_);

    // Returns the correct memory bounds
    ASSERT_TRUE(firstFoundMemoryBounds.has_value());
    ASSERT_TRUE(secondFoundMemoryBounds.has_value());

    EXPECT_EQ(firstFoundMemoryBounds, firstMemoryBounds);
    EXPECT_EQ(secondFoundMemoryBounds, secondFoundMemoryBounds);
}

TEST_F(MemoryResourceRegistryMemoryBoundsTest, ReturnsErrorForInvalidIdentifier)
{
    const std::uint64_t invalid_identifier{10U};

    // Given 2 memory ranges are inserted into the registry
    InsertMemoryResourcesIntoRegistry();

    // When checking the memory bounds for pointers outside the memory bounds
    const auto GetBoundsFromIdentifierInvalidIdentifierResult = unit.GetBoundsFromIdentifier(invalid_identifier);

    // Then a result should be returned
    ASSERT_FALSE(GetBoundsFromIdentifierInvalidIdentifierResult.has_value());
    EXPECT_EQ(GetBoundsFromIdentifierInvalidIdentifierResult.error(),
              SharedMemoryErrorCode::UnknownSharedMemoryIdentifier);
}

using MemoryResourceRegistryOverlappingMemoryBoundsTest = MemoryResourceRegistryTest;
TEST_F(MemoryResourceRegistryOverlappingMemoryBoundsTest, CannotInsertResourcesWithOverlappingMemoryBounds)
{
    const std::uint64_t memory_resource_id_0{0U};
    const std::uint64_t memory_resource_id_1{1U};

    // Given 2 memory resources with overlapping memory bounds for which IsBoundsCheckBypassingMemoryResource
    // returns false
    BasicMemoryResource resource_0{{std::uintptr_t{10U}, std::uintptr_t{20}}};
    BasicMemoryResource resource_1{{std::uintptr_t{5U}, std::uintptr_t{15}}};

    // Then inserting the first memory resource should return true
    EXPECT_TRUE(unit.insert_resource({memory_resource_id_0, &resource_0}));

    // But inserting the second resource should return false
    EXPECT_FALSE(unit.insert_resource({memory_resource_id_1, &resource_1}));
}

TEST_F(MemoryResourceRegistryOverlappingMemoryBoundsTest,
       CanInsertResourcesWithOverlappingMemoryBoundsForBoundsBypassingResource)
{
    const std::uint64_t memory_resource_id_0{0U};
    const std::uint64_t memory_resource_id_1{1U};

    // Given 2 memory resources with overlapping memory bounds for which IsBoundsCheckBypassingMemoryResource
    // returns true
    BoundsCheckBypassingMemoryResource resource_0{{std::uintptr_t{10U}, std::uintptr_t{20}}};
    BoundsCheckBypassingMemoryResource resource_1{{std::uintptr_t{5U}, std::uintptr_t{15}}};

    // Then inserting the first memory resource should return true
    EXPECT_TRUE(unit.insert_resource({memory_resource_id_0, &resource_0}));

    // and inserting the second resource should also return true
    EXPECT_TRUE(unit.insert_resource({memory_resource_id_1, &resource_1}));
}

TEST_F(MemoryResourceRegistryOverlappingMemoryBoundsTest, CannotGetBoundsWithPointerForBoundsCheckingBypassingResource)
{
    const std::uint64_t memory_resource_id{0U};
    const std::uintptr_t start_address{10};
    const std::uintptr_t end_address{20};

    // Given a memory resources with overlapping memory bounds for which IsBoundsCheckBypassingMemoryResource
    // returns true
    BoundsCheckBypassingMemoryResource resource{{start_address, end_address}};

    // When inserting the resources into the registry
    EXPECT_TRUE(unit.insert_resource({memory_resource_id, &resource}));

    // when getting the bounds for the region with an identifier
    const auto region_from_identifier = unit.GetBoundsFromIdentifier(memory_resource_id);

    // Then the returned region should be correct
    ASSERT_TRUE(region_from_identifier.has_value());
    EXPECT_EQ(region_from_identifier->GetStartAddress(), start_address);
    EXPECT_EQ(region_from_identifier->GetEndAddress(), end_address);

    // and when get the bounds for a pointer within the region
    auto* const pointer_in_region = reinterpret_cast<void*>(15);
    const auto region_from_pointer = unit.GetBoundsFromAddress(pointer_in_region);

    // Then the result should be an empty optional
    ASSERT_FALSE(region_from_pointer.has_value());

    // and when get the bounds for a pointer as integer within the region
    const auto pointer_as_integer_in_region = 15;
    const auto region_from_pointer_as_integer = unit.GetBoundsFromAddressAsInteger(pointer_as_integer_in_region);

    // Then the result should be an empty optional
    ASSERT_FALSE(region_from_pointer_as_integer.has_value());
}

using MemoryResourceRegistryDeathTest = MemoryResourceRegistryTest;
TEST_F(MemoryResourceRegistryDeathTest, InsertingANullMemoryResourceTerminates)
{
    // When inserting a null MemoryResource into the registry
    // Then the program terminates
    EXPECT_DEATH(unit.insert_resource({0U, nullptr}), ".*");
}

TEST_F(MemoryResourceRegistryDeathTest, InsertingAMemoryResourceWithNullStartingAddressTerminates)
{
    // When inserting a MemoryResource with a null starting address into the registry
    // Then the program terminates
    BasicMemoryResource resource{{std::uintptr_t{0U}, std::uintptr_t{1U}}};
    EXPECT_DEATH(unit.insert_resource({0U, &resource}), ".*");
}

TEST_F(MemoryResourceRegistryDeathTest, InsertingAMemoryResourceWithNullEndingAddressTerminates)
{
    // When inserting a MemoryResource with a null ending address into the registry
    // Then the program terminates
    BasicMemoryResource resource{{std::uintptr_t{1U}, std::uintptr_t{0U}}};
    EXPECT_DEATH(unit.insert_resource({0U, &resource}), ".*");
}

}  // namespace score::memory::shared::test
