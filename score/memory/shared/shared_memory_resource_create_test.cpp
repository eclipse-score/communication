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
#include "score/memory/shared/pointer_arithmetic_util.h"
#include "score/memory/shared/shared_memory_resource.h"
#include "score/memory/shared/shared_memory_test_resources.h"
#include "score/os/utils/acl/access_control_list_mock.h"

#include "gtest/gtest.h"

#include <array>
#include <cstddef>
#include <memory>

using ::testing::_;
using ::testing::AnyOf;
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
using WorldReadable = SharedMemoryResourceTestAttorney::WorldReadable;
using WorldWritable = SharedMemoryResourceTestAttorney::WorldWritable;
using UserPermissionsMap = SharedMemoryResourceTestAttorney::UserPermissionsMap;
using UserPermissions = SharedMemoryResourceTestAttorney::UserPermissions;

using SharedMemoryResourceCreateTest = SharedMemoryResourceTest;
TEST_F(SharedMemoryResourceCreateTest, CreatingSharedMemoryInitializesCorrectly)
{
    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 1;
    constexpr std::int32_t lock_file_descriptor = 5;
    bool isInitialized = false;

    // Given that we can successfully create a shared memory region
    alignas(std::alignment_of<ControlBlock>::value) std::array<std::uint8_t, 500U> dataRegion{};
    expectSharedMemorySuccessfullyCreated(file_descriptor, lock_file_descriptor, dataRegion.data());

    // and the memory region is safely unmapped on destruction
    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(file_descriptor));

    // When constructing a SharedMemoryResource with create option
    auto resource_result =
        SharedMemoryResourceTestAttorney::Create(TestValues::sharedMemorySegmentPath,
                                                 TestValues::some_share_memory_size,
                                                 [&isInitialized](std::shared_ptr<ISharedMemoryResource>) {
                                                     isInitialized = true;
                                                 });
    ASSERT_TRUE(resource_result.has_value());
    auto resource = resource_result.value();

    // That our initialization callback was invoked
    ASSERT_TRUE(isInitialized);
    auto* controlBlock = reinterpret_cast<ControlBlock*>(dataRegion.data());
    EXPECT_EQ(controlBlock->alreadyAllocatedBytes,
              CalculateAlignedSize(sizeof(ControlBlock), alignof(std::max_align_t)));

    // and the resource owner UID was initialized correctly
    SharedMemoryResourceTestAttorney resource_attorney{*resource};
    EXPECT_EQ(resource_attorney.getOwnerUid(), TestValues::our_uid);

    // and that no bytes were allocated.
    EXPECT_EQ(resource->GetUserAllocatedBytes(), 0U);
}

TEST_F(SharedMemoryResourceCreateTest, CreateSharedMemoryFreesResourcesOnDestruction)
{
    RecordProperty("Verifies", "SCR-6367126");
    RecordProperty("Description", "SharedMemoryResource shall free resources only on destruction.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 1;
    constexpr std::int32_t lock_file_descriptor = 5;

    alignas(std::alignment_of<ControlBlock>::value) std::array<std::uint8_t, 500U> dataRegion{};
    expectSharedMemorySuccessfullyCreated(file_descriptor, lock_file_descriptor, dataRegion.data());

    // and cleanup the shared memory
    bool memory_unlinked{false};
    bool memory_unmapped{false};
    bool file_descriptor_closed{false};
    EXPECT_CALL(*mman_mock_, shm_unlink(StrEq(TestValues::sharedMemorySegmentPath)))
        .WillOnce(::testing::InvokeWithoutArgs([&memory_unlinked]() -> score::cpp::expected_blank<score::os::Error> {
            memory_unlinked = true;
            return score::cpp::blank{};
        }));
    EXPECT_CALL(*mman_mock_, munmap(_, _))
        .WillOnce(::testing::InvokeWithoutArgs([&memory_unmapped]() -> score::cpp::expected_blank<score::os::Error> {
            memory_unmapped = true;
            return score::cpp::blank{};
        }));
    EXPECT_CALL(*unistd_mock_, close(1))
        .WillOnce(::testing::InvokeWithoutArgs([&file_descriptor_closed]() -> score::cpp::expected_blank<score::os::Error> {
            file_descriptor_closed = true;
            return score::cpp::blank{};
        }));

    // When constructing a SharedMemoryResource with create option
    {
        auto resource_result = SharedMemoryResourceTestAttorney::Create(
            TestValues::sharedMemorySegmentPath, TestValues::some_share_memory_size, emptyInitCallback);
        ASSERT_TRUE(resource_result.has_value());
        auto resource = resource_result.value();

        // That the shared memory is unlinked
        EXPECT_FALSE(memory_unlinked);
        SharedMemoryResourceTestAttorney resource_attorney{*resource};
        resource_attorney.Remove();
        EXPECT_TRUE(memory_unlinked);

        // and that the managed memory resource is unmapped and closed when all shared ptrs to the SharedMemoryResource
        // are destroyed
        EXPECT_FALSE(memory_unmapped);
        EXPECT_FALSE(file_descriptor_closed);
    }
    EXPECT_TRUE(memory_unmapped);
    EXPECT_TRUE(file_descriptor_closed);
}

TEST_F(SharedMemoryResourceCreateTest, CreateSharedMemoryReturnsAnErrorWhenSomebodyElseGotTheLock)
{
    InSequence sequence{};

    // Given that we cannot create the lock file (it already exists at that point in time)
    expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath,
                                score::cpp::make_unexpected(Error::createFromErrno(EEXIST)));

    // When constructing a SharedMemoryResource with create option
    auto resource_result = SharedMemoryResourceTestAttorney::Create(
        TestValues::sharedMemorySegmentPath, TestValues::some_share_memory_size, emptyInitCallback);

    // Then we get a device or resource busy error
    EXPECT_FALSE(resource_result.has_value());
    EXPECT_EQ(resource_result.error(), Error::Code::kDeviceOrResourceBusy);
}

TEST_F(SharedMemoryResourceCreateTest, SetsMapPermissionsCorrectly)
{
    using ::score::os::Stat;
    constexpr auto readWriteAccessForUser = Stat::Mode::kReadUser | Stat::Mode::kWriteUser;

    os::AccessControlListMock acl_mock{};

    constexpr std::int32_t file_descriptor = 1;
    constexpr std::int32_t lock_file_descriptor = 5;
    alignas(std::alignment_of<ControlBlock>::value) std::array<std::uint8_t, 500U> dataRegion{};

    {
        InSequence sequence{};

        // Given that we can successfully create a lock file for shared-memory creation
        expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath, lock_file_descriptor);
        // Then we can create the shared memory (without giving access to Others) and initialize it
        expectShmOpenWithCreateFlagAndModeReturns(
            TestValues::sharedMemorySegmentPath, readWriteAccessForUser, file_descriptor);
        expectFstatReturns(file_descriptor);
    }

    // Then we can set the requested permissions
    EXPECT_CALL(acl_mock, AllowUser(43, score::os::Acl::Permission::kWrite)).Times(1);
    EXPECT_CALL(acl_mock, AllowUser(42, score::os::Acl::Permission::kRead)).Times(1);

    {
        InSequence sequence{};

        EXPECT_CALL(*unistd_mock_, ftruncate(_, _)).WillOnce(Return(score::cpp::blank{}));
        expectMmapReturns(dataRegion.data(), file_descriptor);

        // and afterwards cleanup the lock file and shared memory
        EXPECT_CALL(*unistd_mock_, close(lock_file_descriptor));
        EXPECT_CALL(*unistd_mock_, unlink(StrEq(TestValues::sharedMemorySegmentLockPath)));

        // and the memory region is safely unmapped on destruction
        EXPECT_CALL(*mman_mock_, munmap(_, _));
        EXPECT_CALL(*unistd_mock_, close(file_descriptor)).Times(1);
    }

    // When constructing a SharedMemoryResource with create option and non-empty Permissions
    UserPermissionsMap permissions{
        {score::os::Acl::Permission::kRead, {42}},
        {score::os::Acl::Permission::kWrite, {43}},
    };

    score::cpp::ignore = SharedMemoryResourceTestAttorney::Create(TestValues::sharedMemorySegmentPath,
                                                           TestValues::some_share_memory_size,
                                                           emptyInitCallback,
                                                           permissions,
                                                           &acl_mock);
}

TEST_F(SharedMemoryResourceCreateTest, CreateSharedMemoryWithAllocateNamedTypedMemoryFails)
{
    using ::score::os::Stat;
    constexpr auto read_write_access_for_user = Stat::Mode::kReadUser | Stat::Mode::kWriteUser;
    os::AccessControlListMock acl_mock{};

    constexpr std::int32_t file_descriptor = 1;
    constexpr std::int32_t lock_file_descriptor = 5;
    alignas(std::alignment_of<ControlBlock>::value) std::array<std::uint8_t, 500U> data_region{};
    auto typedmemory_mock = std::make_shared<score::memory::shared::TypedMemoryMock>();
    const UserPermissionsMap permissions{
        {score::os::Acl::Permission::kRead, {42}},
        {score::os::Acl::Permission::kWrite, {43}},
    };

    {
        InSequence sequence{};

        // Given that we can successfully create a lock file for shared-memory creation
        expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath, lock_file_descriptor);

        // and we get an unexpected error when creating the shared memory using typed memory
        EXPECT_CALL(
            *typedmemory_mock,
            AllocateNamedTypedMemory(
                _, TestValues::sharedMemorySegmentPath, ::testing::VariantWith<UserPermissionsMap>(permissions)))
            .WillOnce(Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOENT))));

        // Then we can create the shared memory (without giving access to Others) and initialize it
        expectShmOpenWithCreateFlagAndModeReturns(
            TestValues::sharedMemorySegmentPath, read_write_access_for_user, file_descriptor);
        expectFstatReturns(file_descriptor);
    }

    // Then we can set the requested permissions
    EXPECT_CALL(acl_mock, AllowUser(43, score::os::Acl::Permission::kWrite)).Times(1);
    EXPECT_CALL(acl_mock, AllowUser(42, score::os::Acl::Permission::kRead)).Times(1);

    {
        InSequence sequence{};

        EXPECT_CALL(*unistd_mock_, ftruncate(_, _)).WillOnce(Return(score::cpp::blank{}));
        expectMmapReturns(data_region.data(), file_descriptor);

        // and afterwards cleanup the lock file and shared memory
        EXPECT_CALL(*unistd_mock_, close(lock_file_descriptor));
        EXPECT_CALL(*unistd_mock_, unlink(StrEq(TestValues::sharedMemorySegmentLockPath)));

        // and the memory region is safely unmapped on destruction
        EXPECT_CALL(*mman_mock_, munmap(_, _));
        EXPECT_CALL(*unistd_mock_, close(file_descriptor)).Times(1);
    }

    // When constructing a SharedMemoryResource with create option and user permissions
    const auto shm_create_result = SharedMemoryResourceTestAttorney::Create(TestValues::sharedMemorySegmentPath,
                                                                            TestValues::some_share_memory_size,
                                                                            emptyInitCallback,
                                                                            permissions,
                                                                            &acl_mock,
                                                                            typedmemory_mock);

    // Then we get a new SharedMemoryResource instance with shared-memory region not located in the typed memory
    ASSERT_TRUE(shm_create_result.has_value());
    EXPECT_FALSE(shm_create_result.value()->IsShmInTypedMemory());
}

TEST_F(SharedMemoryResourceCreateTest, CreateSharedMemoryWithAllocateNamedTypedMemoryDoesNotApplyUserPermission)
{
    using ::score::os::Stat;
    constexpr auto read_write_access_for_user = Stat::Mode::kReadUser | Stat::Mode::kWriteUser;
    constexpr auto oflag = Fcntl::Open::kReadWrite | Fcntl::Open::kExclusive;

    os::AccessControlListMock acl_mock{};

    constexpr std::int32_t file_descriptor = 1;
    constexpr std::int32_t lock_file_descriptor = 5;
    alignas(std::alignment_of<ControlBlock>::value) std::array<std::uint8_t, 500U> data_region{};
    auto typedmemory_mock = std::make_shared<score::memory::shared::TypedMemoryMock>();
    const UserPermissionsMap permissions{
        {score::os::Acl::Permission::kRead, {42}},
        {score::os::Acl::Permission::kWrite, {43}},
    };

    {
        InSequence sequence{};

        // Given that we can successfully create a lock file for shared-memory creation
        expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath, lock_file_descriptor);

        // Then we can create the shared memory using typed memory
        EXPECT_CALL(
            *typedmemory_mock,
            AllocateNamedTypedMemory(
                _, TestValues::sharedMemorySegmentPath, ::testing::VariantWith<UserPermissionsMap>(permissions)))
            .WillOnce(Return(score::cpp::blank{}));

        // Then we can open the shared memory (without giving access to Others) and initialize it
        EXPECT_CALL(*mman_mock_,
                    shm_open(StrEq(TestValues::sharedMemorySegmentPath), oflag, read_write_access_for_user))
            .WillOnce(Return(file_descriptor));
        expectFstatReturns(file_descriptor);
    }

    EXPECT_CALL(acl_mock, AllowUser(_, _)).Times(0);

    {
        InSequence sequence{};

        expectMmapReturns(data_region.data(), file_descriptor);

        // and afterwards cleanup the lock file and shared memory
        EXPECT_CALL(*unistd_mock_, close(lock_file_descriptor));
        EXPECT_CALL(*unistd_mock_, unlink(StrEq(TestValues::sharedMemorySegmentLockPath)));

        // and the memory region is safely unmapped on destruction
        EXPECT_CALL(*mman_mock_, munmap(_, _));
        EXPECT_CALL(*unistd_mock_, close(file_descriptor)).Times(1);
    }

    // When constructing a SharedMemoryResource with create option and user permissions
    const auto shm_create_result = SharedMemoryResourceTestAttorney::Create(TestValues::sharedMemorySegmentPath,
                                                                            TestValues::some_share_memory_size,
                                                                            emptyInitCallback,
                                                                            permissions,
                                                                            &acl_mock,
                                                                            typedmemory_mock);

    // Then we get a new SharedMemoryResource instance with shared-memory region located in the typed memory
    ASSERT_TRUE(shm_create_result.has_value());
    EXPECT_TRUE(shm_create_result.value()->IsShmInTypedMemory());
}

TEST_F(SharedMemoryResourceCreateTest, SetsWorldReadablePermissionsCorrectly)
{
    using ::score::os::Stat;
    constexpr auto readWriteAccessForUser = Stat::Mode::kReadUser | Stat::Mode::kWriteUser;
    constexpr auto readAccessForEveryBody = Stat::Mode::kReadGroup | Stat::Mode::kReadOthers | readWriteAccessForUser;

    InSequence sequence{};
    os::AccessControlListMock acl_mock{};
    constexpr std::int32_t file_descriptor = 1;
    constexpr std::int32_t lock_file_descriptor = 5;

    // Given that we can successfully create a shared memory region
    expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath, lock_file_descriptor);

    // Then we can create the shared memory (giving read/write access to Others) and initialize it
    alignas(std::alignment_of<ControlBlock>::value) std::array<std::uint8_t, 500U> dataRegion{};
    expectShmOpenWithCreateFlagAndModeReturns(
        TestValues::sharedMemorySegmentPath, readAccessForEveryBody, file_descriptor);
    expectFstatReturns(file_descriptor);

    // Then we don't set any permissions
    EXPECT_CALL(acl_mock, AllowUser(_, _)).Times(0);

    EXPECT_CALL(*unistd_mock_, ftruncate(_, _)).WillOnce(Return(score::cpp::blank{}));
    expectMmapReturns(dataRegion.data(), file_descriptor);

    // and afterwards cleanup the lock file and shared memory
    EXPECT_CALL(*unistd_mock_, close(lock_file_descriptor));
    EXPECT_CALL(*unistd_mock_, unlink(StrEq(TestValues::sharedMemorySegmentLockPath)));

    // and the memory region is safely unmapped on destruction
    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(file_descriptor)).Times(1);

    // When constructing a SharedMemoryResource with create option and world readable Permissions
    WorldReadable permissions{};
    score::cpp::ignore = SharedMemoryResourceTestAttorney::Create(TestValues::sharedMemorySegmentPath,
                                                           TestValues::some_share_memory_size,
                                                           emptyInitCallback,
                                                           permissions,
                                                           &acl_mock);
}

TEST_F(SharedMemoryResourceCreateTest, SetsWorldWritablePermissionsCorrectly)
{
    using ::score::os::Stat;
    constexpr auto readWriteAccessForUser = Stat::Mode::kReadUser | Stat::Mode::kWriteUser;
    constexpr auto readWriteAccessForEveryBody = Stat::Mode::kReadGroup | Stat::Mode::kWriteGroup |
                                                 Stat::Mode::kReadOthers | Stat::Mode::kWriteOthers |
                                                 readWriteAccessForUser;

    InSequence sequence{};
    os::AccessControlListMock acl_mock{};
    constexpr std::int32_t file_descriptor = 1;
    constexpr std::int32_t lock_file_descriptor = 5;

    // Given that we can successfully create a shared memory region
    expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath, lock_file_descriptor);

    // Then we can create the shared memory (giving read/write access to Others) and initialize it
    alignas(std::alignment_of<ControlBlock>::value) std::array<std::uint8_t, 500U> dataRegion{};
    expectShmOpenWithCreateFlagAndModeReturns(
        TestValues::sharedMemorySegmentPath, readWriteAccessForEveryBody, file_descriptor);
    expectFstatReturns(file_descriptor);
    EXPECT_CALL(*stat_mock_, fchmod(file_descriptor, readWriteAccessForEveryBody)).WillOnce(Return(score::cpp::blank{}));

    // Then we don't set any permissions
    EXPECT_CALL(acl_mock, AllowUser(_, _)).Times(0);

    EXPECT_CALL(*unistd_mock_, ftruncate(_, _)).WillOnce(Return(score::cpp::blank{}));
    expectMmapReturns(dataRegion.data(), file_descriptor);

    // and afterwards cleanup the lock file and shared memory
    EXPECT_CALL(*unistd_mock_, close(lock_file_descriptor));
    EXPECT_CALL(*unistd_mock_, unlink(StrEq(TestValues::sharedMemorySegmentLockPath)));

    // and the memory region is safely unmapped on destruction
    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(file_descriptor)).Times(1);

    // When constructing a SharedMemoryResource with create option and world writable Permissions
    WorldWritable permissions{};
    score::cpp::ignore = SharedMemoryResourceTestAttorney::Create(TestValues::sharedMemorySegmentPath,
                                                           TestValues::some_share_memory_size,
                                                           emptyInitCallback,
                                                           permissions,
                                                           &acl_mock);
}
TEST_F(SharedMemoryResourceCreateTest, FailingToCompensateUmaskWillNotCrash)
{
    using ::score::os::Stat;
    constexpr auto readWriteAccessForUser = Stat::Mode::kReadUser | Stat::Mode::kWriteUser;
    constexpr auto readWriteAccessForEveryBody = Stat::Mode::kReadGroup | Stat::Mode::kWriteGroup |
                                                 Stat::Mode::kReadOthers | Stat::Mode::kWriteOthers |
                                                 readWriteAccessForUser;

    InSequence sequence{};
    os::AccessControlListMock acl_mock{};
    constexpr std::int32_t file_descriptor = 1;
    constexpr std::int32_t lock_file_descriptor = 5;

    // Given that we can successfully create a shared memory region
    expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath, lock_file_descriptor);

    // Then we can create the shared memory (giving read/write access to Others) and initialize it
    alignas(std::alignment_of<ControlBlock>::value) std::array<std::uint8_t, 500U> dataRegion{};
    expectShmOpenWithCreateFlagAndModeReturns(
        TestValues::sharedMemorySegmentPath, readWriteAccessForEveryBody, file_descriptor);
    expectFstatReturns(file_descriptor);

    // Expecting that the fchmod call fails
    EXPECT_CALL(*stat_mock_, fchmod(file_descriptor, readWriteAccessForEveryBody))
        .WillOnce(Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOENT))));

    // Then we don't set any permissions
    EXPECT_CALL(acl_mock, AllowUser(_, _)).Times(0);

    EXPECT_CALL(*unistd_mock_, ftruncate(_, _)).WillOnce(Return(score::cpp::blank{}));
    expectMmapReturns(dataRegion.data(), file_descriptor);

    // and afterwards cleanup the lock file and shared memory
    EXPECT_CALL(*unistd_mock_, close(lock_file_descriptor));
    EXPECT_CALL(*unistd_mock_, unlink(StrEq(TestValues::sharedMemorySegmentLockPath)));

    // and the memory region is safely unmapped on destruction
    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(file_descriptor)).Times(1);

    // When constructing a SharedMemoryResource with create option and world writable Permissions
    WorldWritable permissions{};
    score::cpp::ignore = SharedMemoryResourceTestAttorney::Create(TestValues::sharedMemorySegmentPath,
                                                           TestValues::some_share_memory_size,
                                                           emptyInitCallback,
                                                           permissions,
                                                           &acl_mock);

    // Then we don't crash
}

TEST_F(SharedMemoryResourceCreateTest, SettingPermissionsErrorDoesNotCrash)
{
    os::AccessControlListMock acl_mock{};
    constexpr std::int32_t file_descriptor = 1;
    constexpr std::int32_t lock_file_descriptor = 5;
    alignas(std::alignment_of<ControlBlock>::value) std::array<std::uint8_t, 500U> dataRegion{};

    {
        InSequence sequence{};

        // Given that we can successfully create a shared memory region
        expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath, lock_file_descriptor);
        // Then we can create the shared memory and initialize it
        expectShmOpenWithCreateFlagReturns(TestValues::sharedMemorySegmentPath, file_descriptor);
        expectFstatReturns(file_descriptor);
    }

    // Then we can set the requested permissions
    EXPECT_CALL(acl_mock, AllowUser(43, score::os::Acl::Permission::kWrite)).Times(1);
    EXPECT_CALL(acl_mock, AllowUser(42, score::os::Acl::Permission::kRead))
        .Times(1)
        .WillOnce(Return(score::cpp::make_unexpected(Error::createFromErrno(ENOENT))));

    {
        InSequence sequence{};

        EXPECT_CALL(*unistd_mock_, ftruncate(_, _)).WillOnce(Return(score::cpp::blank{}));
        expectMmapReturns(dataRegion.data(), file_descriptor);

        // and afterwards cleanup the lock file and shared memory
        EXPECT_CALL(*unistd_mock_, close(lock_file_descriptor));
        EXPECT_CALL(*unistd_mock_, unlink(StrEq(TestValues::sharedMemorySegmentLockPath)));

        // and the memory region is safely unmapped on destruction
        EXPECT_CALL(*mman_mock_, munmap(_, _));
        EXPECT_CALL(*unistd_mock_, close(file_descriptor)).Times(1);
    }

    // When constructing a SharedMemoryResource with create option
    UserPermissionsMap permissions{
        {score::os::Acl::Permission::kRead, {42}},
        {score::os::Acl::Permission::kWrite, {43}},
    };
    score::cpp::ignore = SharedMemoryResourceTestAttorney::Create(TestValues::sharedMemorySegmentPath,
                                                           TestValues::some_share_memory_size,
                                                           emptyInitCallback,
                                                           permissions,
                                                           &acl_mock);
}

TEST_F(SharedMemoryResourceCreateTest, CreatingSharedMemoryFillsRegistryKnownRegions)
{
    // When constructing a SharedMemoryResource for the first time in a process
    EXPECT_EQ(memory_resource_registry_attorney_.known_regions_size(), 0);
    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 1;
    constexpr std::int32_t lock_file_descriptor = 5;

    // Given that we can successfully create a shared memory region
    alignas(std::alignment_of<ControlBlock>::value) std::array<std::uint8_t, 500U> dataRegion{};
    expectSharedMemorySuccessfullyCreated(file_descriptor, lock_file_descriptor, dataRegion.data());

    // and the memory region is safely unmapped on destruction
    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(1)).Times(1);
    auto resource_result = SharedMemoryResourceTestAttorney::Create(
        TestValues::sharedMemorySegmentPath, TestValues::some_share_memory_size, emptyInitCallback);
    ASSERT_TRUE(resource_result.has_value());

    // Then a memory region of the correct size should be inserted into the MemoryResourceRegistry
    const auto known_memory_regions_result =
        MemoryResourceRegistry::getInstance().GetBoundsFromAddress(dataRegion.data());
    ASSERT_TRUE(known_memory_regions_result.has_value());
    const auto& known_memory_regions = known_memory_regions_result.value();
    auto known_memory_region_size = known_memory_regions.GetEndAddress() - known_memory_regions.GetStartAddress();
    EXPECT_EQ(memory_resource_registry_attorney_.known_regions_size(), 1);
    EXPECT_EQ(known_memory_region_size,
              TestValues::some_share_memory_size + SharedMemoryResourceTestAttorney::GetNeededManagementSpace());
}

TEST_F(SharedMemoryResourceCreateTest, UnableToOverwriteSharedMemorySegment)
{
    InSequence sequence{};
    constexpr std::int32_t lock_file_descriptor = 5;

    // Given that we can create the lock file (it did not exist at that point in time)
    expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath, lock_file_descriptor);

    // Then when trying to create the shared memory segment, the shared memory segment has been already been created by
    // another process.
    expectShmOpenWithCreateFlagReturns(TestValues::sharedMemorySegmentPath,
                                       score::cpp::make_unexpected(Error::createFromErrno(EEXIST)));

    // and afterwards cleanup the lock file and shared memory
    EXPECT_CALL(*unistd_mock_, close(lock_file_descriptor));
    EXPECT_CALL(*unistd_mock_, unlink(StrEq(TestValues::sharedMemorySegmentLockPath)));

    // When constructing a SharedMemoryResource with create option
    auto resource_result = SharedMemoryResourceTestAttorney::Create(
        TestValues::sharedMemorySegmentPath, TestValues::some_share_memory_size, emptyInitCallback);

    // Then we get an object already exists error
    ASSERT_FALSE(resource_result.has_value());
    ASSERT_EQ(resource_result.error(), score::os::Error::Code::kObjectExists);
}

using SharedMemoryResourceCreateDeathTest = SharedMemoryResourceCreateTest;
TEST_F(SharedMemoryResourceCreateDeathTest, CreateSharedMemoryTerminatesIfCreationReturnsUnexpectedError)
{
    InSequence sequence{};
    constexpr std::int32_t lock_file_descriptor = 5;
    constexpr bool is_death_test = true;

    // Given that we can create the lock file (it did not exist at that point in time)
    expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath, lock_file_descriptor, is_death_test);

    // and we get an unexpected error when opening the shared memory
    expectShmOpenWithCreateFlagReturns(
        TestValues::sharedMemorySegmentPath, score::cpp::make_unexpected(Error::createFromErrno(ENOENT)), is_death_test);

    // Then the program terminates when constructing a SharedMemoryResource with create option
    EXPECT_DEATH(SharedMemoryResourceTestAttorney::Create(
                     TestValues::sharedMemorySegmentPath, TestValues::some_share_memory_size, emptyInitCallback),
                 ".*");
}

TEST_F(SharedMemoryResourceCreateDeathTest, UnableToTruncateSharedMemoryCausesTermination)
{
    RecordProperty("Verifies", "SCR-6240638");
    RecordProperty("Description", "A process shall terminate, if the truncation of a shared memory segment fails.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 1;
    constexpr std::int32_t lock_file_descriptor = 5;
    constexpr bool is_death_test = true;

    // Given that we can create the lock file (it did not exist at that point in time)
    expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath, lock_file_descriptor, is_death_test);

    // Then we can create the shared memory and initialize it
    expectShmOpenWithCreateFlagReturns(TestValues::sharedMemorySegmentPath, file_descriptor, is_death_test);
    expectFstatReturns(file_descriptor, is_death_test);

    // But ftruncate returns an error when truncating the shared memory
    EXPECT_CALL(*unistd_mock_,
                ftruncate(_,
                          static_cast<long>(TestValues::some_share_memory_size +
                                            SharedMemoryResourceTestAttorney::GetNeededManagementSpace())))
        .Times(AtMost(1))
        .WillRepeatedly(Return(score::cpp::make_unexpected(Error::createFromErrno())));

    // (We mock the rest of the calls to ensure that the process only terminates due to ftruncate(), not due to
    // calling a function on a mock object without an EXPECT_CALL().
    alignas(std::alignment_of<ControlBlock>::value) std::array<std::uint8_t, 500U> dataRegion{};
    expectMmapReturns(dataRegion.data(), file_descriptor, true, is_death_test);

    // Then the program terminates when constructing a SharedMemoryResource with create option
    EXPECT_DEATH(SharedMemoryResourceTestAttorney::Create(
                     TestValues::sharedMemorySegmentPath, TestValues::some_share_memory_size, emptyInitCallback),
                 ".*");
}

}  // namespace score::memory::shared::test
