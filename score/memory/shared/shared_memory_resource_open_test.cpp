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
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrEq;

using Mman = ::score::os::Mman;
using Fcntl = score::os::Fcntl;
using Error = score::os::Error;

using ControlBlock = SharedMemoryResourceTestAttorney::ControlBlock;

using SharedMemoryResourceOpenTest = SharedMemoryResourceTest;
TEST_F(SharedMemoryResourceOpenTest, OpensSharedMemoryReadOnlyByDefault)
{
    RecordProperty("Verifies", "SCR-5899175, SCR-6240424");
    RecordProperty(
        "Description",
        "Can open shared memory segment read-only. Only opens shared memory segment provided in constructor.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

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
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 1;
    constexpr bool is_read_write = false;

    // Given that the lock file does not exist
    expectOpenLockFileReturns(TestValues::sharedMemorySegmentLockPath,
                              score::cpp::make_unexpected(Error::createFromErrno(ENOENT)));

    // and that we can open the shared memory region
    expectShmOpenReturns(TestValues::sharedMemorySegmentPath, file_descriptor, is_read_write);
    expectFstatReturns(file_descriptor);
    expectMmapReturns(reinterpret_cast<void*>(1), file_descriptor, is_read_write);

    bool memory_unmapped{false};
    bool file_descriptor_closed{false};
    EXPECT_CALL(*mman_mock_, shm_unlink(StrEq(TestValues::sharedMemorySegmentPath))).Times(0);
    EXPECT_CALL(*mman_mock_, munmap(_, _))
        .WillOnce(::testing::InvokeWithoutArgs([&memory_unmapped]() -> score::cpp::expected_blank<score::os::Error> {
            memory_unmapped = true;
            return score::cpp::blank{};
        }));
    EXPECT_CALL(*unistd_mock_, close(file_descriptor))
        .WillOnce(::testing::InvokeWithoutArgs([&file_descriptor_closed]() -> score::cpp::expected_blank<score::os::Error> {
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
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 5;
    constexpr bool is_read_write = false;

    // Given that the lock file is not gone at the beginning of the execution
    EXPECT_CALL(*stat_mock_, stat(StrEq(TestValues::sharedMemorySegmentLockPath), _, _))
        .WillOnce(Return(score::cpp::blank{}))
        .WillOnce(Return(score::cpp::make_unexpected(Error::createFromErrno(ENOENT))));

    // That the shared memory segment is opened read only if not otherwise specified.
    expectShmOpenReturns(TestValues::sharedMemorySegmentPath, file_descriptor, is_read_write);
    expectFstatReturns(file_descriptor);
    expectMmapReturns(reinterpret_cast<void*>(1), file_descriptor, is_read_write);

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
    constexpr bool is_read_write = false;

    // Given that the querying the lockfile throws an unexpected error
    expectOpenLockFileReturns(TestValues::sharedMemorySegmentLockPath,
                              score::cpp::make_unexpected(Error::createFromErrno(EIO)));

    // That the shared memory segment is opened read only if not otherwise specified.
    expectShmOpenReturns(TestValues::sharedMemorySegmentPath, file_descriptor, is_read_write);
    expectFstatReturns(file_descriptor);
    expectMmapReturns(reinterpret_cast<void*>(1), file_descriptor, is_read_write);

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
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

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
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    constexpr bool is_read_write = false;

    // Given that the lock file does not exist
    expectOpenLockFileReturns(TestValues::sharedMemorySegmentLockPath,
                              score::cpp::make_unexpected(Error::createFromErrno(ENOENT)));

    // and that when the shared memory segment is opened, it fails with an error that no such file or directory exists
    expectShmOpenReturns(
        TestValues::sharedMemorySegmentPath, score::cpp::make_unexpected(Error::createFromErrno(ENOENT)), is_read_write);

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
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    constexpr bool is_read_write = false;

    // Given that the lock file does not exist
    expectOpenLockFileReturns(TestValues::sharedMemorySegmentLockPath,
                              score::cpp::make_unexpected(Error::createFromErrno(ENOENT)));

    // and that when the shared memory segment is opened, it fails with a permission denied error
    expectShmOpenReturns(
        TestValues::sharedMemorySegmentPath, score::cpp::make_unexpected(Error::createFromErrno(EACCES)), is_read_write);

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

TEST_F(SharedMemoryResourceOpenTest, OpenTypedSharedMemorySuccessWhenOnlyOneUserHasExecutePermission)
{
    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 1;
    constexpr bool is_read_write = false;
    constexpr bool is_death_test = false;
    auto acl_control_list_mock = std::make_unique<score::os::AccessControlListMock>();
    score::os::IAccessControlList* acl_control_list = acl_control_list_mock.get();
    std::vector<score::os::IAccessControlList::UserIdentifier> users_with_exec_permission = {2025U};

    // Given that the lock file does not exist
    expectOpenLockFileReturns(TestValues::sharedMemorySegmentLockPath,
                              score::cpp::make_unexpected(Error::createFromErrno(ENOENT)));

    // and that we can open the shared memory region
    expectShmOpenReturns(TestValues::sharedMemorySegmentPath, file_descriptor, is_read_write);

    // and that the shared memory is in typed memory region
    expectFstatReturns(
        file_descriptor, is_death_test, static_cast<uid_t>(TestValues::typedmemd_uid), static_cast<std::int64_t>(1));

    // and that the execute permission is set for only one user in the eACL
    EXPECT_CALL(*acl_control_list_mock, FindUserIdsWithPermission(score::os::Acl::Permission::kExecute))
        .WillOnce(Return(users_with_exec_permission));

    expectMmapReturns(reinterpret_cast<void*>(1), file_descriptor, is_read_write);

    // and the memory regions are safely unmapped on destruction
    EXPECT_CALL(*mman_mock_, munmap(_, _)).Times(1);
    EXPECT_CALL(*unistd_mock_, close(_)).Times(1);

    const auto resource_result =
        SharedMemoryResourceTestAttorney::Open(TestValues::sharedMemorySegmentPath, is_read_write, acl_control_list);
    ASSERT_TRUE(resource_result.has_value());
}

TEST_F(SharedMemoryResourceOpenTest,
       IsShmInTypedMemoryReturnsTrueWhenOpenTypedSharedMemorySuccessWhenOnlyOneUserHasExecutePermission)
{
    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 1;
    constexpr bool is_read_write = false;
    constexpr bool is_death_test = false;
    auto acl_control_list_mock = std::make_unique<score::os::AccessControlListMock>();
    score::os::IAccessControlList* acl_control_list = acl_control_list_mock.get();
    std::vector<score::os::IAccessControlList::UserIdentifier> users_with_exec_permission = {2025U};

    // Given that the lock file does not exist
    expectOpenLockFileReturns(TestValues::sharedMemorySegmentLockPath,
                              score::cpp::make_unexpected(Error::createFromErrno(ENOENT)));

    // and that we can open the shared memory region
    expectShmOpenReturns(TestValues::sharedMemorySegmentPath, file_descriptor, is_read_write);

    // Expecting that fstat returns the typedmem UID indicating that the shared memory region is in typed memory.
    expectFstatReturns(
        file_descriptor, is_death_test, static_cast<uid_t>(TestValues::typedmemd_uid), static_cast<std::int64_t>(1));

    // and that the execute permission is set for only one user in the eACL
    EXPECT_CALL(*acl_control_list_mock, FindUserIdsWithPermission(score::os::Acl::Permission::kExecute))
        .WillOnce(Return(users_with_exec_permission));

    expectMmapReturns(reinterpret_cast<void*>(1), file_descriptor, is_read_write);

    EXPECT_CALL(*unistd_mock_, close(_)).Times(1);

    // and given the shared memory region is opened
    const auto resource_result =
        SharedMemoryResourceTestAttorney::Open(TestValues::sharedMemorySegmentPath, is_read_write, acl_control_list);

    // When checking if the shared memory region is in typed memory
    const auto is_in_typed_memory = resource_result.value()->IsShmInTypedMemory();

    // Then the result is true
    EXPECT_TRUE(is_in_typed_memory);
}

TEST_F(SharedMemoryResourceOpenTest,
       IsShmInTypedMemoryReturnsFalseWhenOpenTypedSharedMemoryFailWhenOnlyOneUserHasExecutePermission)
{
    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 1;
    constexpr bool is_read_write = false;
    constexpr bool is_death_test = false;
    auto acl_control_list_mock = std::make_unique<score::os::AccessControlListMock>();
    score::os::IAccessControlList* acl_control_list = acl_control_list_mock.get();
    std::vector<score::os::IAccessControlList::UserIdentifier> users_with_exec_permission = {2025U};

    // Given that the lock file does not exist
    expectOpenLockFileReturns(TestValues::sharedMemorySegmentLockPath,
                              score::cpp::make_unexpected(Error::createFromErrno(ENOENT)));

    // and that we can open the shared memory region
    expectShmOpenReturns(TestValues::sharedMemorySegmentPath, file_descriptor, is_read_write);

    constexpr auto kInvalidtypedmemUid = 0xffU;
    // Expecting that fstat returns a UID which is different to the typedmem UID indicating that the shared memory
    // region is not in typed memory.
    expectFstatReturns(
        file_descriptor, is_death_test, static_cast<uid_t>(kInvalidtypedmemUid), static_cast<std::int64_t>(0));

    expectMmapReturns(reinterpret_cast<void*>(1), file_descriptor, is_read_write);

    EXPECT_CALL(*unistd_mock_, close(_)).Times(1);

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

    void* const baseAddress0 = reinterpret_cast<void*>(10);
    void* const baseAddress1 = static_cast<void*>(static_cast<std::uint8_t*>(baseAddress0) +
                                                  TestValues::some_share_memory_size + sizeof(ControlBlock) + 1U);

    // Given a SharedMemoryResource
    expectOpenLockFileReturns(TestValues::sharedMemorySegmentLockPath,
                              score::cpp::make_unexpected(Error::createFromErrno(ENOENT)));
    expectShmOpenReturns(TestValues::sharedMemorySegmentPath, file_descriptor, is_read_write);
    expectFstatReturns(file_descriptor);
    expectMmapReturns(baseAddress0, file_descriptor, is_read_write);

    auto resource_result = SharedMemoryResourceTestAttorney::Open(TestValues::sharedMemorySegmentPath, is_read_write);
    ASSERT_TRUE(resource_result.has_value());
    auto resource = resource_result.value();

    expectOpenLockFileReturns(TestValues::secondSharedMemorySegmentLockPath,
                              score::cpp::make_unexpected(Error::createFromErrno(ENOENT)));
    expectShmOpenReturns(TestValues::secondSharedMemorySegmentPath, fileDescriptor2, is_read_write);
    expectFstatReturns(fileDescriptor2);
    expectMmapReturns(baseAddress1, fileDescriptor2, is_read_write);

    auto resource_result2 =
        SharedMemoryResourceTestAttorney::Open(TestValues::secondSharedMemorySegmentPath, is_read_write);
    ASSERT_TRUE(resource_result2.has_value());
    auto resource2 = resource_result2.value();

    // and the memory regions are safely unmapped on destruction
    EXPECT_CALL(*mman_mock_, munmap(_, _)).Times(1);
    EXPECT_CALL(*unistd_mock_, close(_)).Times(1);
    EXPECT_CALL(*mman_mock_, munmap(_, _)).Times(1);
    EXPECT_CALL(*unistd_mock_, close(_)).Times(1);

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

using SharedMemoryResourceOpenDeathTest = SharedMemoryResourceOpenTest;
TEST_F(SharedMemoryResourceOpenDeathTest, OpensSharedMemoryTerminatesProcessIfLockfileIsAlwaysThere)
{
    InSequence sequence{};
    constexpr bool is_death_test = true;
    constexpr bool is_read_write = false;

    // Given that the lock file will not be removed
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

    // Given that the lock file does not exist
    expectOpenLockFileReturns(
        TestValues::sharedMemorySegmentLockPath, score::cpp::make_unexpected(Error::createFromErrno(EOF)), is_death_test);

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

    // Given that the lock file does not exist
    expectOpenLockFileReturns(
        TestValues::sharedMemorySegmentLockPath, score::cpp::make_unexpected(Error::createFromErrno(ENOENT)), is_death_test);

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

    // Given that the lock file does not exist
    expectOpenLockFileReturns(
        TestValues::sharedMemorySegmentLockPath, score::cpp::make_unexpected(Error::createFromErrno(ENOENT)), is_death_test);

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

TEST_F(SharedMemoryResourceOpenDeathTest, OpenTypedSharedMemoryErrorOnRetrievingListOfUserIDsWithExecutePermission)
{
    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 5;
    constexpr bool is_read_write = false;
    constexpr bool is_death_test = true;
    auto acl_control_list_mock = std::make_unique<score::os::AccessControlListMock>();
    score::os::IAccessControlList* acl_control_list = acl_control_list_mock.get();

    // Given that the lock file does not exist
    expectOpenLockFileReturns(
        TestValues::sharedMemorySegmentLockPath, score::cpp::make_unexpected(Error::createFromErrno(ENOENT)), is_death_test);

    // and that we can open the shared memory region
    expectShmOpenReturns(TestValues::sharedMemorySegmentPath, file_descriptor, is_read_write, is_death_test);

    // and that the fstat returns typedmemd uid
    expectFstatReturns(
        file_descriptor, is_death_test, static_cast<uid_t>(TestValues::typedmemd_uid), static_cast<std::int64_t>(1));

    // and that the finding user id with execute permission fails
    EXPECT_CALL(*acl_control_list_mock, FindUserIdsWithPermission(score::os::Acl::Permission::kExecute))
        .Times(::testing::AtMost(1))
        .WillOnce(Return(score::cpp::make_unexpected(Error::createFromErrno(ENOENT))));

    // When Opening a SharedMemoryResource
    EXPECT_DEATH(
        SharedMemoryResourceTestAttorney::Open(TestValues::sharedMemorySegmentPath, is_read_write, acl_control_list),
        ".*");
}

TEST_F(SharedMemoryResourceOpenDeathTest, OpenTypedSharedMemoryErrorInvalidNumberOfUsersWithExecutePermission)
{
    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 5;
    constexpr bool is_read_write = false;
    constexpr bool is_death_test = true;
    auto acl_control_list_mock = std::make_unique<score::os::AccessControlListMock>();
    score::os::IAccessControlList* acl_control_list = acl_control_list_mock.get();
    std::vector<score::os::IAccessControlList::UserIdentifier> users_with_exec_permission = {2025U, 6025U};

    // Given that the lock file does not exist
    expectOpenLockFileReturns(
        TestValues::sharedMemorySegmentLockPath, score::cpp::make_unexpected(Error::createFromErrno(ENOENT)), is_death_test);

    // and that we can open the shared memory region
    expectShmOpenReturns(TestValues::sharedMemorySegmentPath, file_descriptor, is_read_write, is_death_test);

    // and that the fstat returns typedmemd uid
    expectFstatReturns(
        file_descriptor, is_death_test, static_cast<uid_t>(TestValues::typedmemd_uid), static_cast<std::int64_t>(1));

    // and that the finding user id with execute permission fails
    EXPECT_CALL(*acl_control_list_mock, FindUserIdsWithPermission(score::os::Acl::Permission::kExecute))
        .Times(::testing::AtMost(1))
        .WillRepeatedly(Return(users_with_exec_permission));

    // When Opening a SharedMemoryResource
    EXPECT_DEATH(
        SharedMemoryResourceTestAttorney::Open(TestValues::sharedMemorySegmentPath, is_read_write, acl_control_list),
        ".*");
}

TEST_F(SharedMemoryResourceOpenDeathTest, OpenTypedSharedMemoryErrorWhenNoUserHasExecutePermission)
{
    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 5;
    constexpr bool is_read_write = false;
    constexpr bool is_death_test = true;
    auto acl_control_list_mock = std::make_unique<score::os::AccessControlListMock>();
    score::os::IAccessControlList* acl_control_list = acl_control_list_mock.get();
    std::vector<score::os::IAccessControlList::UserIdentifier> users_with_exec_permission = {};

    // Given that the lock file does not exist
    expectOpenLockFileReturns(
        TestValues::sharedMemorySegmentLockPath, score::cpp::make_unexpected(Error::createFromErrno(ENOENT)), is_death_test);

    // and that we can open the shared memory region
    expectShmOpenReturns(TestValues::sharedMemorySegmentPath, file_descriptor, is_read_write, is_death_test);

    // and that the fstat returns typedmemd uid
    expectFstatReturns(
        file_descriptor, is_death_test, static_cast<uid_t>(TestValues::typedmemd_uid), static_cast<std::int64_t>(1));

    // and that the execute permission is set for no user at all
    EXPECT_CALL(*acl_control_list_mock, FindUserIdsWithPermission(score::os::Acl::Permission::kExecute))
        .Times(::testing::AtMost(1))
        .WillRepeatedly(Return(users_with_exec_permission));

    // When Opening a SharedMemoryResource
    EXPECT_DEATH(
        SharedMemoryResourceTestAttorney::Open(TestValues::sharedMemorySegmentPath, is_read_write, acl_control_list),
        ".*");
}

}  // namespace score::memory::shared::test
