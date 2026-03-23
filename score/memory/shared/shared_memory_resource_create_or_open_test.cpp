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

#include "gtest/gtest.h"

#include <array>
#include <memory>

using ::testing::_;
using ::testing::AtMost;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrEq;

using Mman = ::score::os::Mman;
using Fcntl = score::os::Fcntl;
using Error = score::os::Error;

namespace score::memory::shared::test
{

using ControlBlock = SharedMemoryResourceTestAttorney::ControlBlock;

using SharedMemoryResourceCreateOrOpenTest = SharedMemoryResourceTest;
TEST_F(SharedMemoryResourceCreateOrOpenTest, OpeningAlreadyCreatedSharedMemorySucceeds)
{
    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 5;
    constexpr bool is_read_write = true;

    // We can successfully open the shared memory when it already exists
    expectSharedMemorySuccessfullyOpened(file_descriptor, is_read_write);

    // When creating or opening a shared memory region with CreateOrOpen
    auto resource_result = SharedMemoryResourceTestAttorney::CreateOrOpen(
        TestValues::sharedMemorySegmentPath, TestValues::some_share_memory_size, emptyInitCallback);
    ASSERT_TRUE(resource_result.has_value());
}

TEST_F(SharedMemoryResourceCreateOrOpenTest, SharedMemoryCreatedWhenSharedMemoryDoesNotAlreadyExist)
{
    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 5;
    constexpr std::int32_t lock_file_descriptor = 1;
    constexpr bool is_read_write = true;
    bool isInitialized = false;

    // Given that the lock file does not exist
    expectOpenLockFileReturns(TestValues::sharedMemorySegmentLockPath,
                              score::cpp::make_unexpected(Error::createFromErrno(ENOENT)));

    // And the shared memory region also doesn't exist
    expectShmOpenReturns(
        TestValues::sharedMemorySegmentPath, score::cpp::make_unexpected(Error::createFromErrno(ENOENT)), is_read_write);

    // Then we successfully create the lock
    // the data region, where mmap shall place the mapping (which would in reality ALWAYS PAGE aligned) should be
    // max-aligned
    alignas(alignof(std::max_align_t)) std::array<std::uint8_t, 500U> dataRegion{};
    expectSharedMemorySuccessfullyCreated(file_descriptor, lock_file_descriptor, dataRegion.data());

    // and the memory region is safely unmapped on destruction
    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(file_descriptor));

    // When creating or opening a shared memory region with CreateOrOpen
    auto resource_result =
        SharedMemoryResourceTestAttorney::CreateOrOpen(TestValues::sharedMemorySegmentPath,
                                                       TestValues::some_share_memory_size,
                                                       [&isInitialized](std::shared_ptr<ManagedMemoryResource>) {
                                                           isInitialized = true;
                                                       });
    ASSERT_TRUE(resource_result.has_value());

    // That our initialization callback was invoked
    ASSERT_TRUE(isInitialized);
    auto* controlBlock = reinterpret_cast<ControlBlock*>(dataRegion.data());
    // and that the SharedMemoryResource has already allocated the bytes for the control block plus eventually some
    // padding, to make sure that user-data allocation starts at a worst-case aligned address.
    // (see SharedMemoryResource::initializeControlBlock())
    EXPECT_GE(controlBlock->alreadyAllocatedBytes, sizeof(ControlBlock));
    EXPECT_LT(controlBlock->alreadyAllocatedBytes, (sizeof(ControlBlock) + alignof(std::max_align_t)));
}

TEST_F(SharedMemoryResourceCreateOrOpenTest, SharedMemoryOpenedWhenSharedMemoryIsFinallyCreatedByOtherProcess)
{
    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 5;
    constexpr std::int32_t lock_file_descriptor = 1;
    constexpr bool is_read_write = true;
    bool isInitialized = false;

    // Given that the lock file does not exist
    expectOpenLockFileReturns(TestValues::sharedMemorySegmentLockPath,
                              score::cpp::make_unexpected(Error::createFromErrno(ENOENT)));

    // And the shared memory region doesn't exist when we first try to open it
    expectShmOpenReturns(
        TestValues::sharedMemorySegmentPath, score::cpp::make_unexpected(Error::createFromErrno(ENOENT)), is_read_write);

    // And we can create the lock file
    expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath, lock_file_descriptor);

    // But the shared memory region now exists when we try to create it
    expectShmOpenWithCreateFlagReturns(TestValues::sharedMemorySegmentPath,
                                       score::cpp::make_unexpected(Error::createFromErrno(EEXIST)));

    // and afterwards cleanup the lock file and shared memory
    EXPECT_CALL(*unistd_mock_, close(lock_file_descriptor));
    EXPECT_CALL(*unistd_mock_, unlink(StrEq(TestValues::sharedMemorySegmentLockPath)));

    // Then we successfully open it when we try again
    expectSharedMemorySuccessfullyOpened(file_descriptor, is_read_write);

    // When creating or opening a shared memory region with CreateOrOpen
    auto resource_result =
        SharedMemoryResourceTestAttorney::CreateOrOpen(TestValues::sharedMemorySegmentPath,
                                                       TestValues::some_share_memory_size,
                                                       [&isInitialized](std::shared_ptr<ManagedMemoryResource>) {
                                                           isInitialized = true;
                                                       });
    ASSERT_TRUE(resource_result.has_value());

    // And the initialization callback will not be called
    EXPECT_FALSE(isInitialized);
}

using SharedMemoryResourceCreateOrOpenDeathTest = SharedMemoryResourceCreateOrOpenTest;
TEST_F(SharedMemoryResourceCreateOrOpenDeathTest, OpeningSharedMemoryWithUnknownErrorTerminates)
{
    InSequence sequence{};
    constexpr bool is_death_test = true;

    // Given that the lock file does not exist
    expectOpenLockFileReturns(
        TestValues::sharedMemorySegmentLockPath, score::cpp::make_unexpected(Error::createFromErrno(ENOENT)), is_death_test);

    // And we get an unknown error when trying to open the shared memory region
    expectShmOpenReturns(
        TestValues::sharedMemorySegmentPath, score::cpp::make_unexpected(Error::createFromErrno(EOF)), true, is_death_test);

    // Then the program terminates when creating or opening a shared memory region with CreateOrOpen
    EXPECT_DEATH(SharedMemoryResourceTestAttorney::CreateOrOpen(
                     TestValues::sharedMemorySegmentPath, TestValues::some_share_memory_size, emptyInitCallback),
                 ".*");
}

}  // namespace score::memory::shared::test
