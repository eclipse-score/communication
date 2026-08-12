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
#include "score/memory/shared/new_delete_delegate_resource.h"

#include "score/memory/shared/shared_memory_test_resources.h"

#include "gtest/gtest.h"
#include <score/memory_resource.hpp>

namespace score::memory::shared::test
{

constexpr std::uint64_t DUMMY_MEM_RES_ID{0x0011223344556677U};
constexpr std::uint64_t DUMMY_MEM_RES_ID_OTHER{0x0000223344556677U};

class InvalidMemoryResource : public score::cpp::pmr::memory_resource
{
  private:
    void* do_allocate(const std::size_t, std::size_t) override
    {
        return nullptr;
    }

    void do_deallocate(void*, const std::size_t, std::size_t) override {}

    bool do_is_equal(const memory_resource&) const noexcept override
    {
        return false;
    }
};

class NewDeleteDelegateMemoryResourceFixture : public ::testing::Test
{
  public:
    NewDeleteDelegateMemoryResourceFixture() : unit_(DUMMY_MEM_RES_ID) {}

    void TearDown() override {}

    NewDeleteDelegateMemoryResource unit_;
};

TEST_F(NewDeleteDelegateMemoryResourceFixture, CallingGetBaseAddressReturnsNonNullAddress)
{
    // Given a NewDeleteDelegateMemoryResource

    // When calling GetBaseAddress
    auto* const base_address = unit_.getBaseAddress();

    // Then the result is not a nullptr
    EXPECT_NE(base_address, nullptr);
}

TEST_F(NewDeleteDelegateMemoryResourceFixture, CallingGetUsableBaseAddressReturnsNonNullAddress)
{
    // Given a NewDeleteDelegateMemoryResource

    // When calling GetUsableBaseAddress
    auto* const usable_base_address = unit_.getUsableBaseAddress();

    // Then the result is not a nullptr
    EXPECT_NE(usable_base_address, nullptr);
}

TEST_F(NewDeleteDelegateMemoryResourceFixture, CanAllocateAndDeallocateSomeMemory)
{
    constexpr std::size_t size_to_alloc = 100U;
    constexpr std::size_t alignment = 4U;
    // when allocating some bytes with a given alignment
    auto ptr_to_alloc_mem = unit_.allocate(size_to_alloc, alignment);

    // Expect, that a valid ptr is returned
    EXPECT_NE(ptr_to_alloc_mem, nullptr);
    // and that it is aligned correctly
    EXPECT_TRUE(is_aligned(ptr_to_alloc_mem, alignment));
    // and that deallocating it doesn't crash.
    unit_.deallocate(ptr_to_alloc_mem, size_to_alloc, alignment);
}

using NewDeleteDelegateMemoryResourceFixtureDeathTest = NewDeleteDelegateMemoryResourceFixture;
TEST_F(NewDeleteDelegateMemoryResourceFixtureDeathTest, DeallocateWrongMemoryCrashes)
{
    // given some variable on stack
    std::uint32_t some_stack_variable;

    // when trying to deallocate it, the program crashes ...
    EXPECT_DEATH(unit_.deallocate(&some_stack_variable, sizeof(some_stack_variable), alignof(std::uint32_t)), ".*");
}

TEST_F(NewDeleteDelegateMemoryResourceFixtureDeathTest, DuplicateMemResIDCrashes)
{
    // expect, that creation of a NewDeleteDelegateMemoryResource with an existing key crashes.
    EXPECT_DEATH(NewDeleteDelegateMemoryResource unit(DUMMY_MEM_RES_ID), ".*");
}

TEST_F(NewDeleteDelegateMemoryResourceFixtureDeathTest, DestructionDeallocatesAllocatedMem)
{
    constexpr std::size_t size_to_alloc = 100U;
    constexpr std::size_t alignment = 4U;
    void* ptr_to_alloc_mem;
    {
        // when allocating some bytes with a given alignment
        NewDeleteDelegateMemoryResource delegate_res(DUMMY_MEM_RES_ID_OTHER);
        ptr_to_alloc_mem = delegate_res.allocate(size_to_alloc, alignment);
    }

    // expect, that trying to deallocate it, after the NewDeleteDelegateMemoryResource has been destructed, the program
    // crashes ...
    EXPECT_DEATH(unit_.deallocate(ptr_to_alloc_mem, size_to_alloc, alignment), ".*");
}

TEST(NewDeleteDelegateMemoryResourceDeathTest, CallingDoAllocateTerminatesIfUnderlyingResourceReturnsNullptr)
{
    // Given a NewDeleteDelegateMemoryResource which contains a memory resource which always returns a nullptr when
    // calling allocate().
    InvalidMemoryResource invalid_memory_resource{};
    NewDeleteDelegateMemoryResource unit_(DUMMY_MEM_RES_ID, &invalid_memory_resource);

    // When trying to allocate some bytes
    // Then the program should terminate
    constexpr std::size_t size_to_alloc = 100U;
    constexpr std::size_t alignment = 4U;
    EXPECT_DEATH(score::cpp::ignore = unit_.allocate(size_to_alloc, alignment), ".*");
}

TEST_F(NewDeleteDelegateMemoryResourceFixture, ComparingTheSameResourceWithItselfReturnsTrue)
{
    // Given a NewDeleteDelegateMemoryResource

    // When comparing it with itself
    const auto is_equal = unit_.is_equal(unit_);

    // Then the result is true
    EXPECT_TRUE(is_equal);
}

TEST(NewDeleteDelegateMemoryResourceTest, ComparingTwoNewDeleteDelegateResourceWithTheSameUnderlyingResourceReturnsTrue)
{
    // Given two NewDeleteDelegateMemoryResources with the same underlying memory resource
    score::cpp::pmr::memory_resource* upstream_resource = score::cpp::pmr::new_delete_resource();
    NewDeleteDelegateMemoryResource unit1(DUMMY_MEM_RES_ID, upstream_resource);
    NewDeleteDelegateMemoryResource unit2(DUMMY_MEM_RES_ID + 1U, upstream_resource);

    // When comparing the two NewDeleteDelegateMemoryResources
    const auto is_equal = unit1.is_equal(unit2);

    // Then the result is true
    EXPECT_TRUE(is_equal);
}

TEST(NewDeleteDelegateMemoryResourceTest,
     ComparingTwoNewDeleteDelegateResourceWithDifferentUnderlyingResourceReturnsFalse)
{
    // Given two NewDeleteDelegateMemoryResources with different underlying memory resources
    score::cpp::pmr::memory_resource* upstream_resource = score::cpp::pmr::new_delete_resource();
    InvalidMemoryResource invalid_memory_resource{};
    NewDeleteDelegateMemoryResource unit1(DUMMY_MEM_RES_ID, upstream_resource);
    NewDeleteDelegateMemoryResource unit2(DUMMY_MEM_RES_ID + 1U, &invalid_memory_resource);

    // When comparing the two NewDeleteDelegateMemoryResources
    const auto is_equal = unit1.is_equal(unit2);

    // Then the result is false
    EXPECT_FALSE(is_equal);
}

TEST(NewDeleteDelegateMemoryResourceTest, ComparingNewDeleteDelegateResourceWithADifferentResourceReturnsFalse)
{
    // Given two NewDeleteDelegateMemoryResources with different underlying memory resources
    score::cpp::pmr::memory_resource* upstream_resource = score::cpp::pmr::new_delete_resource();
    NewDeleteDelegateMemoryResource unit(DUMMY_MEM_RES_ID, upstream_resource);
    InvalidMemoryResource invalid_memory_resource{};

    // When comparing the NewDeleteDelegateMemoryResource with another resource
    const auto is_equal = unit.is_equal(invalid_memory_resource);

    // Then the result is false
    EXPECT_FALSE(is_equal);
}

}  // namespace score::memory::shared::test
