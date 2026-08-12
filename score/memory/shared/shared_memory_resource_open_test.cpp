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

#include "gtest/gtest.h"
#include "score/os/errno.h"
#include "score/result/result.h"

#include "score/os/utils/acl/access_control_list_mock.h"

#include <array>
#include <memory>

namespace score::memory::shared::test
{

using ::testing::_;
using ::testing::AtMost;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrEq;

using Mman = ::score::os::Mman;
using Fcntl = score::os::Fcntl;
using Error = score::os::Error;

using ControlBlock = SharedMemoryResourceTestAttorney::ControlBlock;

using SharedMemoryResourceOpenTest = SharedMemoryResourceTest;

#if defined(__QNX__)
constexpr auto kTypedmemdUserName = "typed_memory_daemon";
constexpr auto kTSHMDeviceName = "/dev/typedshm";
#endif

TEST_F(SharedMemoryResourceOpenTest, OpensSharedMemoryReadOnlyByDefault)
{
    RecordProperty("Verifies", "SCR-5899175, SCR-6240424");
    RecordProperty(
        "Description",
        "Can open shared memory segment read-only. Only opens shared memory segment provided in constructor.");
    RecordProperty("TestType", "requirements-based");  // requirements test
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "requirements-analysis");  // requirements

    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 5;
    constexpr bool is_read_write = false;

    // We can successfully open the shared memory read only
    expectSharedMemorySuccessfullyOpened(file_descriptor, is_read_write);

    // When constructing a SharedMemoryResource
    auto resource_result = SharedMemoryResourceTestAttorney::Open(TestValues::sharedMemorySegmentPath, is_read_write);
    ASSERT_TRUE(resource_result.has_value());
}

TEST_F(SharedMemoryResourceOpenTest, OpeningSharedMemoryFreesResourcesOnDestruction)
{
    RecordProperty("Verifies", "SCR-6367126");
    RecordProperty("Description", "SharedMemoryResource shall free resources only on destruction.");
    RecordProperty("TestType", "requirements-based");  // requirements test
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "requirements-analysis");  // requirements

    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 1;
    constexpr std::int32_t lock_file_descriptor = 10;
    constexpr bool is_read_write = false;

    // Given that the lock file is successfully created
    expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath, lock_file_descriptor);

    // and that we can open the shared memory region
    expectShmOpenReturns(TestValues::sharedMemorySegmentPath, file_descriptor, is_read_write);
    expectFstatReturns(file_descriptor);
    expectMmapReturns(reinterpret_cast<void*>(1), file_descriptor, is_read_write);

    // and the lock file is cleaned up after Open completes
    EXPECT_CALL(*unistd_mock_, close(lock_file_descriptor));
    EXPECT_CALL(*unistd_mock_, unlink(StrEq(TestValues::sharedMemorySegmentLockPath)));

    bool memory_unmapped{false};
    bool file_descriptor_closed{false};
    EXPECT_CALL(*mman_mock_, shm_unlink(StrEq(TestValues::sharedMemorySegmentPath))).Times(0);
    EXPECT_CALL(*mman_mock_, munmap(_, _))
        .WillOnce(::testing::InvokeWithoutArgs([&memory_unmapped]() -> score::cpp::expected_blank<score::os::Error> {
            memory_unmapped = true;
            return score::cpp::blank{};
        }));
    EXPECT_CALL(*unistd_mock_, close(file_descriptor))
        .WillOnce(
            ::testing::InvokeWithoutArgs([&file_descriptor_closed]() -> score::cpp::expected_blank<score::os::Error> {
                file_descriptor_closed = true;
                return score::cpp::blank{};
            }));

    // When constructing a SharedMemoryResource with Open option
    {
        auto resource_result =
            SharedMemoryResourceTestAttorney::Open(TestValues::sharedMemorySegmentPath, is_read_write);
        ASSERT_TRUE(resource_result.has_value());

        // and that the opened managed memory resource is unmapped and closed but not unlinked
        EXPECT_FALSE(memory_unmapped);
        EXPECT_FALSE(file_descriptor_closed);
    }
    EXPECT_TRUE(memory_unmapped);
    EXPECT_TRUE(file_descriptor_closed);
}

TEST_F(SharedMemoryResourceOpenTest, OpensSharedMemoryWillWaitUntilLockFileIsGone)
{
    RecordProperty("Verifies", "SCR-5899175");
    RecordProperty("Description", "Can open shared memory segment read-only after a lock was created");
    RecordProperty("TestType", "requirements-based");  // requirements test
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "requirements-analysis");  // requirements

    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 5;
    constexpr std::int32_t lock_file_descriptor = 10;
    constexpr bool is_read_write = false;

    // Given that the lock file creation first fails (another process is creating)
    expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath,
                                score::cpp::make_unexpected(Error::createFromErrno(EEXIST)));

    // And the lock file exists initially, then disappears
    EXPECT_CALL(*stat_mock_, stat(StrEq(TestValues::sharedMemorySegmentLockPath), _, _))
        .WillOnce(Return(score::cpp::blank{}))
        .WillOnce(Return(score::cpp::make_unexpected(Error::createFromErrno(ENOENT))));

    // Then the lock file creation retry succeeds
    expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath, lock_file_descriptor);

    // That the shared memory segment is opened read only if not otherwise specified.
    expectShmOpenReturns(TestValues::sharedMemorySegmentPath, file_descriptor, is_read_write);
    expectFstatReturns(file_descriptor);
    expectMmapReturns(reinterpret_cast<void*>(1), file_descriptor, is_read_write);

    // and the lock file is cleaned up after Open completes
    EXPECT_CALL(*unistd_mock_, close(lock_file_descriptor));
    EXPECT_CALL(*unistd_mock_, unlink(StrEq(TestValues::sharedMemorySegmentLockPath)));

    // and the memory region is safely unmapped on destruction
    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(file_descriptor)).Times(1);

    // When constructing a SharedMemoryResource
    auto resource_result = SharedMemoryResourceTestAttorney::Open(TestValues::sharedMemorySegmentPath, is_read_write);
    ASSERT_TRUE(resource_result.has_value());
}

TEST_F(SharedMemoryResourceOpenTest, OpensSharedMemoryErrorOnLockFileHandleGracefully)
{
    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 5;
    constexpr std::int32_t lock_file_descriptor = 10;
    constexpr bool is_read_write = false;

    // Given that the lock file is successfully created
    expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath, lock_file_descriptor);

    // That the shared memory segment is opened read only if not otherwise specified.
    expectShmOpenReturns(TestValues::sharedMemorySegmentPath, file_descriptor, is_read_write);
    expectFstatReturns(file_descriptor);
    expectMmapReturns(reinterpret_cast<void*>(1), file_descriptor, is_read_write);

    // and the lock file is cleaned up after Open completes
    EXPECT_CALL(*unistd_mock_, close(lock_file_descriptor));
    EXPECT_CALL(*unistd_mock_, unlink(StrEq(TestValues::sharedMemorySegmentLockPath)));

    // and the memory region is safely unmapped on destruction
    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(file_descriptor)).Times(1);

    // When constructing a SharedMemoryResource
    auto resource_result = SharedMemoryResourceTestAttorney::Open(TestValues::sharedMemorySegmentPath, is_read_write);
    ASSERT_TRUE(resource_result.has_value());
}

TEST_F(SharedMemoryResourceOpenTest, OpensSharedMemoryReadWrite)
{
    RecordProperty("Verifies", "SCR-5899175, SCR-6240424");
    RecordProperty(
        "Description",
        "Can open shared memory segment read-write. Only opens shared memory segment provided in constructor.");
    RecordProperty("TestType", "requirements-based");  // requirements test
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "requirements-analysis");  // requirements

    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 5;
    constexpr bool is_read_write = false;

    // We can successfully open the shared memory read only
    expectSharedMemorySuccessfullyOpened(file_descriptor, is_read_write);

    // When constructing a SharedMemoryResource
    auto resource_result = SharedMemoryResourceTestAttorney::Open(TestValues::sharedMemorySegmentPath, is_read_write);
    ASSERT_TRUE(resource_result.has_value());
}

TEST_F(SharedMemoryResourceOpenTest, OpeningResourceThatDoesNotExistWillReturnError)
{
    RecordProperty("Verifies", "SCR-32158471");
    RecordProperty("Description",
                   "Checks that Open will return an error when the underlying resource cannot be found.");
    RecordProperty("TestType", "requirements-based");  // requirements test
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "requirements-analysis");  // requirements

    constexpr bool is_read_write = false;
    constexpr std::int32_t lock_file_descriptor = 10;

    // Given that the lock file is successfully created
    expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath, lock_file_descriptor);

    // and that when the shared memory segment is opened, it fails with an error that no such file or directory exists
    expectShmOpenReturns(TestValues::sharedMemorySegmentPath,
                         score::cpp::make_unexpected(Error::createFromErrno(ENOENT)),
                         is_read_write);

    // and the lock file is cleaned up when the function returns
    EXPECT_CALL(*unistd_mock_, close(lock_file_descriptor));
    EXPECT_CALL(*unistd_mock_, unlink(StrEq(TestValues::sharedMemorySegmentLockPath)));

    // When constructing a SharedMemoryResource
    auto resource_result = SharedMemoryResourceTestAttorney::Open(TestValues::sharedMemorySegmentPath, is_read_write);

    // Then a no such file or directory error will be returned
    ASSERT_FALSE(resource_result.has_value());
    EXPECT_EQ(resource_result.error(), os::Error::Code::kNoSuchFileOrDirectory);
}

TEST_F(SharedMemoryResourceOpenTest, OpeningResourceWithoutTheRequiredACLsWillReturnError)
{
    RecordProperty("Verifies", "SCR-32158471");
    RecordProperty("Description",
                   "Checks that Open will return an error when the process doesn't have the correct permissions to "
                   "open the underlying resource.");
    RecordProperty("TestType", "requirements-based");  // requirements test
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "requirements-analysis");  // requirements

    constexpr bool is_read_write = false;
    constexpr std::int32_t lock_file_descriptor = 10;

    // Given that the lock file is successfully created
    expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath, lock_file_descriptor);

    // and that when the shared memory segment is opened, it fails with a permission denied error
    expectShmOpenReturns(TestValues::sharedMemorySegmentPath,
                         score::cpp::make_unexpected(Error::createFromErrno(EACCES)),
                         is_read_write);

    // and the lock file is cleaned up when the function returns
    EXPECT_CALL(*unistd_mock_, close(lock_file_descriptor));
    EXPECT_CALL(*unistd_mock_, unlink(StrEq(TestValues::sharedMemorySegmentLockPath)));

    // When constructing a SharedMemoryResource
    auto resource_result = SharedMemoryResourceTestAttorney::Open(TestValues::sharedMemorySegmentPath, is_read_write);

    // Then a permission denied error will be returned
    ASSERT_FALSE(resource_result.has_value());
    EXPECT_EQ(resource_result.error(), os::Error::Code::kPermissionDenied);
}

TEST_F(SharedMemoryResourceOpenTest, SameUnitIsEqual)
{
    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 5;
    constexpr bool is_read_write = false;

    // Given a SharedMemoryResource
    expectSharedMemorySuccessfullyOpened(file_descriptor, is_read_write);

    auto resource_result = SharedMemoryResourceTestAttorney::Open(TestValues::sharedMemorySegmentPath, is_read_write);
    ASSERT_TRUE(resource_result.has_value());
    auto resource = resource_result.value();

    // When checking equality with itself
    // That it is equal
    ASSERT_TRUE(resource->is_equal(*resource));
}

TEST_F(SharedMemoryResourceOpenTest, DifferentChildClassIsNotEqual)
{
    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 5;
    constexpr bool is_read_write = false;

    // Given a SharedMemoryResource
    expectSharedMemorySuccessfullyOpened(file_descriptor, is_read_write);

    auto resource_result = SharedMemoryResourceTestAttorney::Open(TestValues::sharedMemorySegmentPath, is_read_write);
    ASSERT_TRUE(resource_result.has_value());
    auto resource = resource_result.value();

    // When checking equality with a different resource type
    // That they are not equal
    MyMemoryResource otherResource{};
    ASSERT_FALSE(resource->is_equal(otherResource));
}

// typed memory daemon is only running on the QNX, so these tests will only pass on the QNX
#if defined(__QNX__)
TEST_F(SharedMemoryResourceOpenTest, IsShmInTypedMemoryReturnsTrueWhenOpenTypedSharedMemorySuccess)
{
    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 1;
    constexpr std::int32_t lock_file_descriptor = 10;
    constexpr bool is_read_write = false;
    constexpr bool is_death_test = false;
    auto acl_control_list_mock = std::make_unique<score::os::AccessControlListMock>();
    score::os::IAccessControlList* acl_control_list = acl_control_list_mock.get();
    std::vector<score::os::IAccessControlList::UserIdentifier> users_with_exec_permission = {2025U};
    auto typedmemory_mock = std::make_shared<score::memory::shared::TypedMemoryMock>();

    // Given that the lock file is successfully created
    expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath, lock_file_descriptor);

    // and that we can open the shared memory region
    expectShmOpenReturns(TestValues::sharedMemorySegmentPath, file_descriptor, is_read_write);

    // Expecting that fstat returns the typedmem UID indicating that the shared memory region is in typed memory.
    expectFstatReturns(
        file_descriptor, is_death_test, static_cast<uid_t>(TestValues::typedmemd_uid), static_cast<std::int64_t>(1));

    // and that the "/dev/typedshm" device exists
    EXPECT_CALL(*stat_mock_, stat(StrEq(kTSHMDeviceName), _, _)).WillOnce(Return(score::cpp::blank{}));

    // and that the creator UID is set
    EXPECT_CALL(*typedmemory_mock, GetCreatorUid(StrEq(TestValues::sharedMemorySegmentPath)))
        .WillOnce(Return(users_with_exec_permission.front()));

    expectMmapReturns(reinterpret_cast<void*>(1), file_descriptor, is_read_write);

    // and the lock file is cleaned up after Open completes
    EXPECT_CALL(*unistd_mock_, close(lock_file_descriptor));
    EXPECT_CALL(*unistd_mock_, unlink(StrEq(TestValues::sharedMemorySegmentLockPath)));

    // and the shm file descriptor is closed on destruction
    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(file_descriptor));

    // and given the shared memory region is opened
    const auto resource_result = SharedMemoryResourceTestAttorney::Open(
        TestValues::sharedMemorySegmentPath, is_read_write, acl_control_list, typedmemory_mock);
    ASSERT_TRUE(resource_result.has_value());

    // When checking if the shared memory region is in typed memory
    const auto is_in_typed_memory = resource_result.value()->IsShmInTypedMemory();

    // Then the result is true
    EXPECT_TRUE(is_in_typed_memory);
}

TEST_F(SharedMemoryResourceOpenTest, IsShmInTypedMemoryReturnsFalseWhenAcquireTypedMemoryDaemonUidFail)
{
    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 1;
    constexpr std::int32_t lock_file_descriptor = 10;
    constexpr bool is_read_write = false;
    auto acl_control_list_mock = std::make_unique<score::os::AccessControlListMock>();
    score::os::IAccessControlList* acl_control_list = acl_control_list_mock.get();
    auto typedmemory_mock = std::make_shared<score::memory::shared::TypedMemoryMock>();

    // Given that the lock file is successfully created
    expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath, lock_file_descriptor);

    // That the shared memory segment is opened read only if not otherwise specified.
    expectShmOpenReturns(TestValues::sharedMemorySegmentPath, file_descriptor, is_read_write);
    expectFstatReturns(file_descriptor);

    // and that the "/dev/typedshm" device exists
    EXPECT_CALL(*stat_mock_, stat(StrEq(kTSHMDeviceName), _, _)).WillOnce(Return(score::cpp::blank{}));

    // and that the acquire typedmemd UID failed
    EXPECT_CALL(*unistd_mock_, getpwnam_r(StrEq(kTypedmemdUserName), _, _, _, _))
        .WillOnce(Return(score::cpp::make_unexpected(Error::createFromErrno(ENOENT))));

    // and that the creator UID is not called
    EXPECT_CALL(*typedmemory_mock, GetCreatorUid(StrEq(TestValues::sharedMemorySegmentPath))).Times(0);

    expectMmapReturns(reinterpret_cast<void*>(1), file_descriptor, is_read_write);

    // and the lock file is cleaned up after Open completes
    EXPECT_CALL(*unistd_mock_, close(lock_file_descriptor));
    EXPECT_CALL(*unistd_mock_, unlink(StrEq(TestValues::sharedMemorySegmentLockPath)));

    // and the shm file descriptor is closed on destruction
    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(file_descriptor));

    // and given the shared memory region is opened
    const auto resource_result = SharedMemoryResourceTestAttorney::Open(
        TestValues::sharedMemorySegmentPath, is_read_write, acl_control_list, typedmemory_mock);
    ASSERT_TRUE(resource_result.has_value());

    // When checking if the shared memory region is in typed memory
    const auto is_in_typed_memory = resource_result.value()->IsShmInTypedMemory();

    // Then the result is false
    EXPECT_FALSE(is_in_typed_memory);
}

TEST_F(SharedMemoryResourceOpenTest, IsShmInTypedMemoryReturnsFalseWhenAcquireTypedMemoryDaemonUidUserDoesNotExist)
{
    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 1;
    constexpr std::int32_t lock_file_descriptor = 10;
    constexpr bool is_read_write = false;
    auto acl_control_list_mock = std::make_unique<score::os::AccessControlListMock>();
    score::os::IAccessControlList* acl_control_list = acl_control_list_mock.get();
    auto typedmemory_mock = std::make_shared<score::memory::shared::TypedMemoryMock>();
    passwd* pwd = nullptr;

    // Given that the lock file is successfully created
    expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath, lock_file_descriptor);

    // That the shared memory segment is opened read only if not otherwise specified.
    expectShmOpenReturns(TestValues::sharedMemorySegmentPath, file_descriptor, is_read_write);
    expectFstatReturns(file_descriptor);

    // and that the "/dev/typedshm" device exists
    EXPECT_CALL(*stat_mock_, stat(StrEq(kTSHMDeviceName), _, _)).WillOnce(Return(score::cpp::blank{}));

    // and that the acquire typedmemd UID user does not exist
    EXPECT_CALL(*unistd_mock_, getpwnam_r(StrEq(kTypedmemdUserName), _, _, _, _))
        .WillOnce((DoAll(SetArgPointee<4>(pwd), Return(score::cpp::blank{}))));

    // and that the creator UID is not called
    EXPECT_CALL(*typedmemory_mock, GetCreatorUid(StrEq(TestValues::sharedMemorySegmentPath))).Times(0);

    expectMmapReturns(reinterpret_cast<void*>(1), file_descriptor, is_read_write);

    // and the lock file is cleaned up after Open completes
    EXPECT_CALL(*unistd_mock_, close(lock_file_descriptor));
    EXPECT_CALL(*unistd_mock_, unlink(StrEq(TestValues::sharedMemorySegmentLockPath)));

    // and the shm file descriptor is closed on destruction
    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(file_descriptor));

    // and given the shared memory region is opened
    const auto resource_result = SharedMemoryResourceTestAttorney::Open(
        TestValues::sharedMemorySegmentPath, is_read_write, acl_control_list, typedmemory_mock);
    ASSERT_TRUE(resource_result.has_value());

    // When checking if the shared memory region is in typed memory
    const auto is_in_typed_memory = resource_result.value()->IsShmInTypedMemory();

    // Then the result is false
    EXPECT_FALSE(is_in_typed_memory);
}

TEST_F(SharedMemoryResourceOpenTest, IsShmInTypedMemoryReturnsFalseWhenTypedshmDeviceDoesNotExist)
{
    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 1;
    constexpr std::int32_t lock_file_descriptor = 10;
    constexpr bool is_read_write = false;
    auto acl_control_list_mock = std::make_unique<score::os::AccessControlListMock>();
    score::os::IAccessControlList* acl_control_list = acl_control_list_mock.get();
    auto typedmemory_mock = std::make_shared<score::memory::shared::TypedMemoryMock>();

    // Given that the lock file is successfully created
    expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath, lock_file_descriptor);

    // That the shared memory segment is opened read only if not otherwise specified.
    expectShmOpenReturns(TestValues::sharedMemorySegmentPath, file_descriptor, is_read_write);
    expectFstatReturns(file_descriptor);

    // and that the "/dev/typedshm" device does not exists
    EXPECT_CALL(*stat_mock_, stat(StrEq(kTSHMDeviceName), _, _))
        .WillOnce(Return(score::cpp::make_unexpected(Error::createFromErrno(ENOENT))));

    // and that the acquire typedmemd UID user is not called
    EXPECT_CALL(*unistd_mock_, getpwnam_r(StrEq(kTypedmemdUserName), _, _, _, _)).Times(0);

    // and that the creator UID is not called
    EXPECT_CALL(*typedmemory_mock, GetCreatorUid(StrEq(TestValues::sharedMemorySegmentPath))).Times(0);

    expectMmapReturns(reinterpret_cast<void*>(1), file_descriptor, is_read_write);

    // and the lock file is cleaned up after Open completes
    EXPECT_CALL(*unistd_mock_, close(lock_file_descriptor));
    EXPECT_CALL(*unistd_mock_, unlink(StrEq(TestValues::sharedMemorySegmentLockPath)));

    // and the shm file descriptor is closed on destruction
    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(file_descriptor));

    // and given the shared memory region is opened
    const auto resource_result = SharedMemoryResourceTestAttorney::Open(
        TestValues::sharedMemorySegmentPath, is_read_write, acl_control_list, typedmemory_mock);
    ASSERT_TRUE(resource_result.has_value());

    // When checking if the shared memory region is in typed memory
    const auto is_in_typed_memory = resource_result.value()->IsShmInTypedMemory();

    // Then the result is false
    EXPECT_FALSE(is_in_typed_memory);
}

TEST_F(SharedMemoryResourceOpenTest, IsShmInTypedMemoryReturnsFalseWhenTypedshmDeviceIsNotAccessible)
{
    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 1;
    constexpr std::int32_t lock_file_descriptor = 10;
    constexpr bool is_read_write = false;
    auto acl_control_list_mock = std::make_unique<score::os::AccessControlListMock>();
    score::os::IAccessControlList* acl_control_list = acl_control_list_mock.get();
    auto typedmemory_mock = std::make_shared<score::memory::shared::TypedMemoryMock>();

    // Given that the lock file is successfully created
    expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath, lock_file_descriptor);

    // That the shared memory segment is opened read only if not otherwise specified.
    expectShmOpenReturns(TestValues::sharedMemorySegmentPath, file_descriptor, is_read_write);
    expectFstatReturns(file_descriptor);

    // and that the "/dev/typedshm" device is not accessible
    EXPECT_CALL(*stat_mock_, stat(StrEq(kTSHMDeviceName), _, _))
        .WillOnce(Return(score::cpp::make_unexpected(Error::createFromErrno(EACCES))));

    // and that the acquire typedmemd UID user is not called
    EXPECT_CALL(*unistd_mock_, getpwnam_r(StrEq(kTypedmemdUserName), _, _, _, _)).Times(0);

    // and that the creator UID is not called
    EXPECT_CALL(*typedmemory_mock, GetCreatorUid(StrEq(TestValues::sharedMemorySegmentPath))).Times(0);

    expectMmapReturns(reinterpret_cast<void*>(1), file_descriptor, is_read_write);

    // and the lock file is cleaned up after Open completes
    EXPECT_CALL(*unistd_mock_, close(lock_file_descriptor));
    EXPECT_CALL(*unistd_mock_, unlink(StrEq(TestValues::sharedMemorySegmentLockPath)));

    // and the shm file descriptor is closed on destruction
    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(file_descriptor));

    // and given the shared memory region is opened
    const auto resource_result = SharedMemoryResourceTestAttorney::Open(
        TestValues::sharedMemorySegmentPath, is_read_write, acl_control_list, typedmemory_mock);
    ASSERT_TRUE(resource_result.has_value());

    // When checking if the shared memory region is in typed memory
    const auto is_in_typed_memory = resource_result.value()->IsShmInTypedMemory();

    // Then the result is false
    EXPECT_FALSE(is_in_typed_memory);
}
#endif

TEST_F(SharedMemoryResourceOpenTest, IsShmInTypedMemoryReturnsFalseWhenOpenTypedSharedMemoryFail)
{
    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 1;
    constexpr std::int32_t lock_file_descriptor = 10;
    constexpr bool is_read_write = false;
    constexpr bool is_death_test = false;
    auto acl_control_list_mock = std::make_unique<score::os::AccessControlListMock>();
    score::os::IAccessControlList* acl_control_list = acl_control_list_mock.get();

    // Given that the lock file is successfully created
    expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath, lock_file_descriptor);

    // and that we can open the shared memory region
    expectShmOpenReturns(TestValues::sharedMemorySegmentPath, file_descriptor, is_read_write);

    constexpr auto kInvalidtypedmemUid = 0xffU;
    // Expecting that fstat returns a UID which is different to the typedmem UID indicating that the shared memory
    // region is not in typed memory.
    expectFstatReturns(
        file_descriptor, is_death_test, static_cast<uid_t>(kInvalidtypedmemUid), static_cast<std::int64_t>(0));

    expectMmapReturns(reinterpret_cast<void*>(1), file_descriptor, is_read_write);

    // and the lock file is cleaned up after Open completes
    EXPECT_CALL(*unistd_mock_, close(lock_file_descriptor));
    EXPECT_CALL(*unistd_mock_, unlink(StrEq(TestValues::sharedMemorySegmentLockPath)));

    // and the shm file descriptor is closed on destruction
    EXPECT_CALL(*unistd_mock_, close(file_descriptor)).Times(1);

    // and given the shared memory region is opened
    const auto resource_result =
        SharedMemoryResourceTestAttorney::Open(TestValues::sharedMemorySegmentPath, is_read_write, acl_control_list);
    // When checking if the shared memory region is in typed memory
    const auto is_in_typed_memory = resource_result.value()->IsShmInTypedMemory();

    // Then the result is false
    EXPECT_FALSE(is_in_typed_memory);
}

TEST_F(SharedMemoryResourceOpenTest, DifferentInstancesAreNotEqual)
{
    InSequence sequence{};
    constexpr bool is_read_write = false;
    constexpr std::int32_t file_descriptor = 5;
    constexpr auto fileDescriptor2 = 6;
    constexpr std::int32_t lock_file_descriptor = 10;

    void* const baseAddress0 = reinterpret_cast<void*>(10);
    void* const baseAddress1 = static_cast<void*>(static_cast<std::uint8_t*>(baseAddress0) +
                                                  TestValues::some_share_memory_size + sizeof(ControlBlock) + 1U);

    // Given a SharedMemoryResource
    expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath, lock_file_descriptor);
    expectShmOpenReturns(TestValues::sharedMemorySegmentPath, file_descriptor, is_read_write);
    expectFstatReturns(file_descriptor);
    expectMmapReturns(baseAddress0, file_descriptor, is_read_write);

    // and the first lock file is cleaned up after Open completes
    EXPECT_CALL(*unistd_mock_, close(lock_file_descriptor));
    EXPECT_CALL(*unistd_mock_, unlink(StrEq(TestValues::sharedMemorySegmentLockPath)));

    auto resource_result = SharedMemoryResourceTestAttorney::Open(TestValues::sharedMemorySegmentPath, is_read_write);
    ASSERT_TRUE(resource_result.has_value());
    auto resource = resource_result.value();

    expectCreateLockFileReturns(TestValues::secondSharedMemorySegmentLockPath, lock_file_descriptor);
    expectShmOpenReturns(TestValues::secondSharedMemorySegmentPath, fileDescriptor2, is_read_write);
    expectFstatReturns(fileDescriptor2);
    expectMmapReturns(baseAddress1, fileDescriptor2, is_read_write);

    // and the second lock file is cleaned up after Open completes
    EXPECT_CALL(*unistd_mock_, close(lock_file_descriptor));
    EXPECT_CALL(*unistd_mock_, unlink(StrEq(TestValues::secondSharedMemorySegmentLockPath)));

    auto resource_result2 =
        SharedMemoryResourceTestAttorney::Open(TestValues::secondSharedMemorySegmentPath, is_read_write);
    ASSERT_TRUE(resource_result2.has_value());
    auto resource2 = resource_result2.value();

    // and the memory regions are safely unmapped on destruction
    EXPECT_CALL(*mman_mock_, munmap(_, _)).Times(1);
    EXPECT_CALL(*unistd_mock_, close(fileDescriptor2)).Times(1);
    EXPECT_CALL(*mman_mock_, munmap(_, _)).Times(1);
    EXPECT_CALL(*unistd_mock_, close(file_descriptor)).Times(1);

    // When checking equality with another instance
    // That it is not equal
    ASSERT_FALSE(resource->is_equal(*resource2));
}

TEST_F(SharedMemoryResourceOpenTest, OpeningSharedMemoryFillsRegistryKnownRegions)
{
    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 5;
    constexpr bool is_read_write = false;

    // When opening a SharedMemoryResource for the first time in a process
    EXPECT_EQ(memory_resource_registry_attorney_.known_regions_size(), 0);

    std::array<std::uint8_t, 500U> dataRegion{};
    // Given that the shared memory segment is opened read only
    expectSharedMemorySuccessfullyOpened(file_descriptor, is_read_write, dataRegion.data());
    auto resource_result = SharedMemoryResourceTestAttorney::Open(TestValues::sharedMemorySegmentPath, is_read_write);
    ASSERT_TRUE(resource_result.has_value());

    // Then a memory region of the correct size should be inserted into the MemoryResourceRegistry
    const auto known_memory_regions_result =
        MemoryResourceRegistry::getInstance().GetBoundsFromAddress(dataRegion.data());
    ASSERT_TRUE(known_memory_regions_result.has_value());
    const auto& known_memory_regions = known_memory_regions_result.value();
    auto known_memory_region_size = known_memory_regions.GetEndAddress() - known_memory_regions.GetStartAddress();
    EXPECT_EQ(memory_resource_registry_attorney_.known_regions_size(), 1);
    EXPECT_EQ(known_memory_region_size, TestValues::some_share_memory_size);
}

// Regression test: Open() must hold the lock file across the entire shm_open + fstat + mmap sequence.
// Without this, a concurrent Create() could start initializing the SHM object after Open's lock check
// but before Open's shm_open, causing Open to read partially-initialized memory.
TEST_F(SharedMemoryResourceOpenTest, LockFileIsHeldDuringEntireOpenSequence)
{
    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 5;
    constexpr std::int32_t lock_file_descriptor = 10;
    constexpr bool is_read_write = false;

    // 1. Lock file is acquired BEFORE any SHM operation
    expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath, lock_file_descriptor);

    // 2. SHM operations happen while the lock is held
    expectShmOpenReturns(TestValues::sharedMemorySegmentPath, file_descriptor, is_read_write);
    expectFstatReturns(file_descriptor);
    expectMmapReturns(reinterpret_cast<void*>(1), file_descriptor, is_read_write);

    // 3. Lock file is released only AFTER mmap completes
    EXPECT_CALL(*unistd_mock_, close(lock_file_descriptor));
    EXPECT_CALL(*unistd_mock_, unlink(StrEq(TestValues::sharedMemorySegmentLockPath)));

    // Cleanup
    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(file_descriptor));

    auto result = SharedMemoryResourceTestAttorney::Open(TestValues::sharedMemorySegmentPath, is_read_write);
    ASSERT_TRUE(result.has_value());
}

using SharedMemoryResourceOpenDeathTest = SharedMemoryResourceOpenTest;
TEST_F(SharedMemoryResourceOpenDeathTest, OpensSharedMemoryTerminatesProcessIfLockfileIsAlwaysThere)
{
    InSequence sequence{};
    constexpr bool is_death_test = true;
    constexpr bool is_read_write = false;

    // Given that lock file creation fails (another process holds it)
    expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath,
                                score::cpp::make_unexpected(Error::createFromErrno(EEXIST)),
                                is_death_test);
    // And the lock file is always present (never goes away)
    expectOpenLockFileReturns(TestValues::sharedMemorySegmentLockPath, score::cpp::blank{}, is_death_test);

    // That the shared memory segment is opened read only if not otherwise specified.
    // When constructing a SharedMemoryResource
    EXPECT_DEATH(SharedMemoryResourceTestAttorney::Open(TestValues::sharedMemorySegmentPath, is_read_write), ".*");
}

TEST_F(SharedMemoryResourceOpenDeathTest, UnableToMemoryMapCausesTermination)
{
    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 5;
    constexpr bool is_read_write = false;
    constexpr bool is_death_test = true;

    // Given that we successfully acquire the lock file
    constexpr std::int32_t lock_file_descriptor = 10;
    expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath, lock_file_descriptor, is_death_test);

    // That the shared memory segment is opened read only if not otherwise specified.
    expectShmOpenReturns(TestValues::sharedMemorySegmentPath, file_descriptor, is_read_write, is_death_test);
    expectFstatReturns(file_descriptor, is_death_test);
    EXPECT_CALL(*mman_mock_, mmap(nullptr, _, Mman::Protection::kRead, Mman::Map::kShared, file_descriptor, 0))
        .WillRepeatedly(Return(score::cpp::make_unexpected(Error::createFromErrno(EOF))));

    EXPECT_DEATH(SharedMemoryResourceTestAttorney::Open(TestValues::sharedMemorySegmentPath, is_read_write), ".*");
}

TEST_F(SharedMemoryResourceOpenDeathTest, OpensSharedMemoryErrorOnFstatCausesTermination)
{
    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 5;
    constexpr bool is_read_write = false;
    constexpr bool is_death_test = true;

    // Given that we successfully acquire the lock file
    constexpr std::int32_t lock_file_descriptor = 10;
    expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath, lock_file_descriptor, is_death_test);

    // and that we can open the shared memory region
    expectShmOpenReturns(TestValues::sharedMemorySegmentPath, file_descriptor, is_read_write, is_death_test);

    // and that the fstat fails for any reason
    expectFstatReturns(file_descriptor,
                       is_death_test,
                       static_cast<uid_t>(1),
                       static_cast<std::int64_t>(1),
                       score::cpp::make_unexpected(Error::createFromErrno(EBADF)));

    // When Opening a SharedMemoryResource
    EXPECT_DEATH(SharedMemoryResourceTestAttorney::Open(TestValues::sharedMemorySegmentPath, is_read_write), ".*");
}

TEST_F(SharedMemoryResourceOpenDeathTest, OpensSharedMemoryEAGAINOnFstatCausesTermination)
{
    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 5;
    constexpr bool is_read_write = false;
    constexpr bool is_death_test = true;

    // Given that we successfully acquire the lock file
    constexpr std::int32_t lock_file_descriptor = 10;
    expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath, lock_file_descriptor, is_death_test);

    // and that we can open the shared memory region
    expectShmOpenReturns(TestValues::sharedMemorySegmentPath, file_descriptor, is_read_write, is_death_test);

    // and that the fstat fails for any reason
    expectFstatReturns(file_descriptor,
                       is_death_test,
                       static_cast<uid_t>(1),
                       static_cast<std::int64_t>(1),
                       score::cpp::make_unexpected(Error::createFromErrno(EAGAIN)));

    // When Opening a SharedMemoryResource
    EXPECT_DEATH(SharedMemoryResourceTestAttorney::Open(TestValues::sharedMemorySegmentPath, is_read_write), ".*");
}

TEST_F(SharedMemoryResourceOpenDeathTest, OpenTypedSharedMemoryTerminatesWhenGetCreatorUidFails)
{
    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 5;
    constexpr bool is_read_write = false;
    constexpr bool is_death_test = true;
    auto acl_control_list_mock = std::make_unique<score::os::AccessControlListMock>();
    score::os::IAccessControlList* acl_control_list = acl_control_list_mock.get();
    auto typedmemory_mock = std::make_shared<score::memory::shared::TypedMemoryMock>();

    // Given that we successfully acquire the lock file
    constexpr std::int32_t lock_file_descriptor = 10;
    expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath, lock_file_descriptor, is_death_test);

    // and that we can open the shared memory region
    expectShmOpenReturns(TestValues::sharedMemorySegmentPath, file_descriptor, is_read_write, is_death_test);

    // and that the fstat returns typedmemd uid
    expectFstatReturns(
        file_descriptor, is_death_test, static_cast<uid_t>(TestValues::typedmemd_uid), static_cast<std::int64_t>(1));

    // and that the creator UID fails
    EXPECT_CALL(*typedmemory_mock, GetCreatorUid(StrEq(TestValues::sharedMemorySegmentPath)))
        .Times(AtMost(1))
        .WillRepeatedly(Return(score::cpp::make_unexpected(Error::createFromErrno(ENOENT))));

    // When Opening a SharedMemoryResource
    // Then the program terminates
    EXPECT_DEATH(SharedMemoryResourceTestAttorney::Open(
                     TestValues::sharedMemorySegmentPath, is_read_write, acl_control_list, typedmemory_mock),
                 ".*");
}

}  // namespace score::memory::shared::test
