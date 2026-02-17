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
#include "score/memory/shared/shared_memory_test_resources.h"

#include "fake/my_memory_resource.h"
#include "score/memory/shared/pointer_arithmetic_util.h"

#include "score/hash.hpp"

#include "gtest/gtest.h"

#include <array>
#include <cstddef>
#include <memory>

namespace score::memory::shared::test
{

using ::testing::_;
using ::testing::AtMost;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrEq;

using Mman = ::score::os::Mman;
using Fcntl = score::os::Fcntl;
using Error = score::os::Error;

using ControlBlock = SharedMemoryResourceTestAttorney::ControlBlock;

using SharedMemoryResourceAllocateTest = SharedMemoryResourceTest;
TEST_F(SharedMemoryResourceAllocateTest, AssociatedMemoryResourceProxyForwardsCallsCorrectly)
{
    RecordProperty("Verifies", "SCR-6223615, SCR-6240703");
    RecordProperty("Description",
                   "The SharedMemoryResource shall return an associated proxy and allocate calls will allocate the "
                   "requested memory.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 5;
    constexpr bool is_read_write = true;

    alignas(std::max_align_t) std::array<std::uint8_t, 300U> dataRegion{};
    auto id = score::cpp::hash_bytes(TestValues::sharedMemorySegmentPath, strlen(TestValues::sharedMemorySegmentPath));
    auto* controlBlock = new (dataRegion.data()) ControlBlock(id);

    // Given a SharedMemoryResource that opens an already existing shared memory region
    expectSharedMemorySuccessfullyOpened(file_descriptor, is_read_write, dataRegion.data());

    auto resource_result = SharedMemoryResourceTestAttorney::Open(TestValues::sharedMemorySegmentPath, is_read_write);
    ASSERT_TRUE(resource_result.has_value());
    auto resource = resource_result.value();

    // When allocating memory through its associated MemoryResourceProxy
    // That we don't receive a nullptr
    ManagedMemoryResourceTestAttorney attorney(*resource);
    EXPECT_NE(attorney.getMemoryResourceProxy()->allocate(5U, 1U), nullptr);
    EXPECT_EQ(controlBlock->alreadyAllocatedBytes, 5U);
}

TEST_F(SharedMemoryResourceAllocateTest, SharedMemoryResourceAllocatesAlignedMemoryDifferentTypes)
{
    RecordProperty("Verifies", "SCR-6241392");
    RecordProperty(
        "Description",
        "SharedMemoryResource shall allocate memory in accordance to the alignment of that CPU architecture.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 5;
    constexpr bool is_read_write = true;

    alignas(std::max_align_t) std::array<std::uint8_t, 300U> dataRegion{};
    auto id = score::cpp::hash_bytes(TestValues::sharedMemorySegmentPath, strlen(TestValues::sharedMemorySegmentPath));
    auto* control_block = new (dataRegion.data()) ControlBlock(id);
    control_block->alreadyAllocatedBytes = sizeof(ControlBlock);

    // Given a SharedMemoryResource that opens an already existing shared memory region
    expectSharedMemorySuccessfullyOpened(file_descriptor, is_read_write, dataRegion.data());

    auto resource_result = SharedMemoryResourceTestAttorney::Open(TestValues::sharedMemorySegmentPath, is_read_write);
    ASSERT_TRUE(resource_result.has_value());
    auto resource = resource_result.value();

    // When constructing two objects of different types
    auto* first_allocation = resource->construct<std::uint8_t>(std::uint8_t{0x42});
    auto* second_allocation = resource->construct<std::uint16_t>(std::uint16_t{0x42});

    // We respect the alignment requirements of both types
    EXPECT_TRUE(is_aligned(first_allocation, alignof(std::uint8_t)));
    EXPECT_TRUE(is_aligned(second_allocation, alignof(std::uint16_t)));
}

TEST_F(SharedMemoryResourceAllocateTest,
       SharedMemoryResourceAllocatesAlignedMemoryDifferentTypesAndReturnCorrectNumberOfAllocatedBytes)
{
    RecordProperty("Verifies", "SCR-6241392");
    RecordProperty(
        "Description",
        "SharedMemoryResource shall allocate memory in accordance to the alignment of that CPU architecture.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 5;
    constexpr bool is_read_write = true;

    alignas(std::max_align_t) std::array<std::uint8_t, 300U> dataRegion{};
    auto id = score::cpp::hash_bytes(TestValues::sharedMemorySegmentPath, strlen(TestValues::sharedMemorySegmentPath));
    auto* control_block = new (dataRegion.data()) ControlBlock(id);
    control_block->alreadyAllocatedBytes = CalculateAlignedSize(sizeof(ControlBlock), alignof(std::max_align_t));

    // Given a SharedMemoryResource that opens an already existing shared memory region
    expectSharedMemorySuccessfullyOpened(file_descriptor, is_read_write, dataRegion.data());

    auto resource_result = SharedMemoryResourceTestAttorney::Open(TestValues::sharedMemorySegmentPath, is_read_write);
    ASSERT_TRUE(resource_result.has_value());
    auto resource = resource_result.value();

    // When constructing two objects of different types
    auto* first_allocation = resource->construct<std::uint8_t>(std::uint8_t{0x42});
    auto* second_allocation = resource->construct<std::uint16_t>(std::uint16_t{0x42});

    // We respect the alignment requirements of both types
    EXPECT_TRUE(is_aligned(first_allocation, alignof(std::uint8_t)));
    EXPECT_TRUE(is_aligned(second_allocation, alignof(std::uint16_t)));

    EXPECT_EQ(resource->GetUserAllocatedBytes(), 4);
}

TEST_F(SharedMemoryResourceAllocateTest, SharedMemoryResourceDeallocateDoesNoDeallocation)
{
    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 1;
    constexpr std::int32_t lock_file_descriptor = 5;
    constexpr std::size_t shared_memory_size{500U};

    // Given that we can successfully create a shared memory region
    alignas(std::max_align_t) std::array<std::uint8_t, shared_memory_size> dataRegion{};
    expectSharedMemorySuccessfullyCreated(file_descriptor, lock_file_descriptor, dataRegion.data());

    // and the memory region is safely unmapped on destruction
    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(file_descriptor));

    // When constructing a SharedMemoryResource with create option
    auto resource_result = SharedMemoryResourceTestAttorney::Create(
        TestValues::sharedMemorySegmentPath, shared_memory_size, emptyInitCallback);
    ASSERT_TRUE(resource_result.has_value());
    auto resource = resource_result.value();

    // and allocating an object in shared memory
    auto* const allocation = resource->construct<std::uint8_t>(std::uint8_t{10});

    // Then the number of allocated bytes should not change after destructing object in shared memory
    const auto already_allocated_bytes = resource->GetUserAllocatedBytes();
    resource->destruct(*allocation);
    EXPECT_EQ(resource->GetUserAllocatedBytes(), already_allocated_bytes);
}

TEST_F(SharedMemoryResourceAllocateTest, WhenAllocatingNumberOfBytesGetUserAllocatedBytesFunctionShallReturnThisNumber)
{
    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 1;
    constexpr std::int32_t lock_file_descriptor = 5;
    constexpr std::size_t number_of_bytes_to_alloc = 16;
    constexpr std::size_t shared_memory_size{500U};

    // Given that we can successfully create a shared memory region
    alignas(std::max_align_t) std::array<std::uint8_t, shared_memory_size> dataRegion{};
    expectSharedMemorySuccessfullyCreated(file_descriptor, lock_file_descriptor, dataRegion.data());

    // and the memory region is safely unmapped on destruction
    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(file_descriptor));

    // When constructing a SharedMemoryResource with create option
    auto resource_result = SharedMemoryResourceTestAttorney::Create(
        TestValues::sharedMemorySegmentPath, shared_memory_size, emptyInitCallback);
    ASSERT_TRUE(resource_result.has_value());
    auto resource = resource_result.value();

    // and allocating an object in shared memory
    score::cpp::ignore = resource->allocate(number_of_bytes_to_alloc);
    EXPECT_EQ(resource->GetUserAllocatedBytes(), number_of_bytes_to_alloc);
}

using SharedMemoryResourceAllocateDeathTest = SharedMemoryResourceAllocateTest;
TEST_F(SharedMemoryResourceAllocateDeathTest, AllocatingBlockLargerThanAllocatedSharedMemoryCausesTermination)
{
    RecordProperty("Verifies", "SCR-6240703");
    RecordProperty("Description",
                   "The process shall terminate when the SharedMemoryResource cannot allocate the requested memory.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 5;
    constexpr bool is_read_write = true;
    constexpr bool is_death_test = true;

    alignas(std::max_align_t) std::array<std::uint8_t, 300U> dataRegion{};
    auto id = score::cpp::hash_bytes(TestValues::sharedMemorySegmentPath, strlen(TestValues::sharedMemorySegmentPath));
    auto* const control_block_addr = new (dataRegion.data()) ControlBlock(id);
    control_block_addr->alreadyAllocatedBytes = sizeof(ControlBlock);

    // Given that the lock file does not exist
    expectOpenLockFileReturns(TestValues::sharedMemorySegmentLockPath,
                              score::cpp::make_unexpected(Error::createFromErrno(ENOENT)));

    // That the shared memory segment is opened read only if not otherwise specified.
    expectShmOpenReturns(TestValues::sharedMemorySegmentPath, file_descriptor, is_read_write, is_death_test);
    expectFstatReturns(file_descriptor);
    expectMmapReturns(dataRegion.data(), file_descriptor, is_read_write, is_death_test);

    // and the memory region is safely unmapped on destruction
    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(file_descriptor));

    auto resource_result = SharedMemoryResourceTestAttorney::Open(TestValues::sharedMemorySegmentPath, is_read_write);
    ASSERT_TRUE(resource_result.has_value());
    auto resource = resource_result.value();

    // When allocating a memory block that is larger than the allocated shared memory segment
    // Then the program terminates
    ManagedMemoryResourceTestAttorney attorney(*resource);
    EXPECT_DEATH(attorney.getMemoryResourceProxy()->allocate(TestValues::some_share_memory_size + 1), ".*");
}

TEST_F(SharedMemoryResourceAllocateDeathTest, AllocatingMultipleBlocksLargerThanAllocatedSharedMemoryCausesTermination)
{
    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 5;
    constexpr bool is_read_write = true;
    constexpr bool is_death_test = true;

    alignas(std::max_align_t) std::array<std::uint8_t, 300U> dataRegion{};
    auto id = score::cpp::hash_bytes(TestValues::sharedMemorySegmentPath, strlen(TestValues::sharedMemorySegmentPath));
    auto* const control_block_addr = new (dataRegion.data()) ControlBlock(id);
    control_block_addr->alreadyAllocatedBytes = sizeof(ControlBlock);

    // Given that the lock file does not exist
    expectOpenLockFileReturns(TestValues::sharedMemorySegmentLockPath,
                              score::cpp::make_unexpected(Error::createFromErrno(ENOENT)));

    // That the shared memory segment is opened read only if not otherwise specified.
    expectShmOpenReturns(TestValues::sharedMemorySegmentPath, file_descriptor, is_read_write, is_death_test);
    expectFstatReturns(file_descriptor);
    expectMmapReturns(dataRegion.data(), file_descriptor, is_read_write, is_death_test);

    // and the memory region is safely unmapped on destruction
    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(file_descriptor));

    auto resource_result = SharedMemoryResourceTestAttorney::Open(TestValues::sharedMemorySegmentPath, is_read_write);
    ASSERT_TRUE(resource_result.has_value());
    auto resource = resource_result.value();

    // When allocating a memory block smaller than the allocated shared memory segment
    const auto memory_to_allocate = (TestValues::some_share_memory_size / 2);
    ManagedMemoryResourceTestAttorney attorney1(*resource);
    EXPECT_NE(attorney1.getMemoryResourceProxy()->allocate(TestValues::some_share_memory_size / 2), nullptr);

    // and then allocating another memory block such that the total memory block allocated is larger than the allocated
    // shared memory segment
    const auto remaining_memory = TestValues::some_share_memory_size - memory_to_allocate;

    // Then the program terminates
    ManagedMemoryResourceTestAttorney attorney2(*resource);
    EXPECT_DEATH(attorney2.getMemoryResourceProxy()->allocate(remaining_memory + 1), ".*");
}

}  // namespace score::memory::shared::test
