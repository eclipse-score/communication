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
#include "score/memory/shared/shared_memory_factory.h"
#include "score/os/utils/acl/i_access_control_list.h"

using ::testing::_;
using ::testing::AtMost;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrEq;

using Mman = ::score::os::Mman;
using Fcntl = ::score::os::Fcntl;
using Error = ::score::os::Error;

namespace score::memory::shared::test
{

namespace
{

/// \brief Wrapper around an instance of a mock for IAccessControlList to handle unique_ptr semantics
/// \details Our application APIs expect, that IAccessControlList instances get returned via unique_ptr. Since we
///          generally want in our tests to use one single/central instance of a mock for IAccessControlList, which
///          is then used throughout all tests, we can't directly inject the mock as it would be (due to unique_ptr
///          semantics) destroyed in each test/usage step. So we use this wrapper instead, which can be destroyed, but
///          doesn't forward destruction to our wrapped/global mock.
class IAccessControlListMockWrapper : public score::os::IAccessControlList
{
  public:
    explicit IAccessControlListMockWrapper(score::os::IAccessControlList* mock) : mock_{mock} {};

    score::cpp::expected_blank<score::os::Error> AllowUser(const UserIdentifier uid,
                                                  const score::os::Acl::Permission permission) override
    {
        return mock_->AllowUser(uid, permission);
    }

    score::cpp::expected<bool, score::os::Error> VerifyMaskPermissions(
        const std::vector<::score::os::Acl::Permission>& permissions) const override
    {
        return mock_->VerifyMaskPermissions(permissions);
    }

    score::cpp::expected<std::vector<UserIdentifier>, score::os::Error> FindUserIdsWithPermission(
        const ::score::os::Acl::Permission permission) const noexcept override
    {
        return mock_->FindUserIdsWithPermission(permission);
    }

  private:
    score::os::IAccessControlList* mock_;
};

}  // namespace

constexpr const char* const TestValues::sharedMemorySegmentPath;
constexpr const std::uint64_t TestValues::sharedMemoryResourceIdentifier;
constexpr const char* const TestValues::secondSharedMemorySegmentPath;
constexpr const char* const TestValues::sharedMemorySegmentLockPath;
constexpr const char* const TestValues::secondSharedMemorySegmentLockPath;
constexpr std::size_t TestValues::some_share_memory_size;
constexpr uid_t TestValues::our_uid;
constexpr auto kTypedmemdProcessName = "typed_memory_daemon";
constexpr std::uint32_t kMaxBufferSize = 16384U;

bool is_aligned(const volatile void* const p, const std::size_t n) noexcept
{
    return reinterpret_cast<std::uintptr_t>(p) % n == 0;
}

ManagedMemoryResourceTestAttorney::ManagedMemoryResourceTestAttorney(ManagedMemoryResource& resource) noexcept
    : resource_{resource}
{
}

const void* ManagedMemoryResourceTestAttorney::getEndAddress() const noexcept
{
    return resource_.getEndAddress();
}

SharedMemoryResourceTestAttorney::SharedMemoryResourceTestAttorney(SharedMemoryResource& resource) noexcept
    : resource_{resource}
{
}

score::cpp::expected<std::shared_ptr<SharedMemoryResource>, score::os::Error> SharedMemoryResourceTestAttorney::Create(
    std::string input_path,
    const std::size_t user_space_to_reserve,
    SharedMemoryResource::InitializeCallback initialize_callback,
    const UserPermissions& permissions,
    score::os::IAccessControlList* acl_control_list,
    std::shared_ptr<score::memory::shared::TypedMemory> typed_memory_ptr) noexcept
{
    if (acl_control_list == nullptr)
    {
        return SharedMemoryResource::Create(
            std::move(input_path),
            user_space_to_reserve,
            std::move(initialize_callback),
            permissions,
            [](std::int32_t file_descriptor) mutable noexcept -> std::unique_ptr<score::os::IAccessControlList> {
                return std::make_unique<score::os::AccessControlList>(file_descriptor);
            },
            typed_memory_ptr);
    }
    else
    {
        return SharedMemoryResource::Create(
            std::move(input_path),
            user_space_to_reserve,
            std::move(initialize_callback),
            permissions,
            [acl_control_list](std::int32_t) noexcept -> std::unique_ptr<score::os::IAccessControlList> {
                return std::make_unique<IAccessControlListMockWrapper>(acl_control_list);
            },
            typed_memory_ptr);
    }
}

score::cpp::expected<std::shared_ptr<SharedMemoryResource>, score::os::Error> SharedMemoryResourceTestAttorney::CreateAnonymous(
    std::uint64_t shared_memory_resource_id,
    const std::size_t user_space_to_reserve,
    SharedMemoryResource::InitializeCallback initialize_callback,
    const UserPermissions& permissions,
    score::os::IAccessControlList* acl_control_list,
    std::shared_ptr<score::memory::shared::TypedMemory> typed_memory_ptr) noexcept
{
    if (acl_control_list == nullptr)
    {
        return SharedMemoryResource::CreateAnonymous(
            shared_memory_resource_id,
            user_space_to_reserve,
            std::move(initialize_callback),
            permissions,
            [](std::int32_t file_descriptor) mutable noexcept -> std::unique_ptr<score::os::IAccessControlList> {
                return std::make_unique<score::os::AccessControlList>(file_descriptor);
            },
            std::move(typed_memory_ptr));
    }
    else
    {
        return SharedMemoryResource::CreateAnonymous(
            shared_memory_resource_id,
            user_space_to_reserve,
            std::move(initialize_callback),
            permissions,
            [acl_control_list](std::int32_t) noexcept -> std::unique_ptr<score::os::IAccessControlList> {
                return std::make_unique<IAccessControlListMockWrapper>(acl_control_list);
            },
            std::move(typed_memory_ptr));
    }
}

score::cpp::expected<std::shared_ptr<SharedMemoryResource>, score::os::Error> SharedMemoryResourceTestAttorney::CreateOrOpen(
    std::string input_path,
    const std::size_t user_space_to_reserve,
    SharedMemoryResource::InitializeCallback initialize_callback,
    const UserPermissions& permissions,
    score::os::IAccessControlList* acl_control_list,
    std::shared_ptr<score::memory::shared::TypedMemory> typed_memory_ptr) noexcept
{
    if (acl_control_list == nullptr)
    {
        return SharedMemoryResource::CreateOrOpen(
            std::move(input_path),
            user_space_to_reserve,
            std::move(initialize_callback),
            permissions,
            [](std::int32_t file_descriptor) mutable noexcept -> std::unique_ptr<score::os::IAccessControlList> {
                return std::make_unique<score::os::AccessControlList>(file_descriptor);
            },
            typed_memory_ptr);
    }
    else
    {
        return SharedMemoryResource::CreateOrOpen(
            std::move(input_path),
            user_space_to_reserve,
            std::move(initialize_callback),
            permissions,
            [acl_control_list](std::int32_t) noexcept -> std::unique_ptr<score::os::IAccessControlList> {
                return std::make_unique<IAccessControlListMockWrapper>(acl_control_list);
            },
            typed_memory_ptr);
    }
}

score::cpp::expected<std::shared_ptr<SharedMemoryResource>, score::os::Error> SharedMemoryResourceTestAttorney::Open(
    std::string input_path,
    const bool is_read_write,
    score::os::IAccessControlList* acl_control_list,
    std::shared_ptr<score::memory::shared::TypedMemory> typed_memory_ptr) noexcept
{
    if (acl_control_list == nullptr)
    {
        return SharedMemoryResource::Open(
            std::move(input_path),
            is_read_write,
            [](std::int32_t file_descriptor) mutable noexcept -> std::unique_ptr<score::os::IAccessControlList> {
                return std::make_unique<score::os::AccessControlList>(file_descriptor);
            },
            typed_memory_ptr);
    }
    else
    {
        return SharedMemoryResource::Open(
            std::move(input_path),
            is_read_write,
            [acl_control_list](std::int32_t) noexcept -> std::unique_ptr<score::os::IAccessControlList> {
                return std::make_unique<IAccessControlListMockWrapper>(acl_control_list);
            },
            typed_memory_ptr);
    }
}

SharedMemoryResourceTest::SharedMemoryResourceTest()
    : memory_resource_registry_attorney_{MemoryResourceRegistry::getInstance()}
{
    pwd_.pw_uid = TestValues::typedmemd_uid;
}

void SharedMemoryResourceTest::SetUp()
{
    // By default, do not expect unlink calls. If shm file deletion is requested, it needs to be specified
    // explicitly in the respective test cases.
    SharedMemoryFactory::SetTypedMemoryProvider(typedmemory_mock_);
    EXPECT_CALL(*mman_mock_, shm_unlink(::testing::_)).Times(0);
    EXPECT_CALL(*unistd_mock_, getuid).Times(::testing::AnyNumber()).WillRepeatedly(Return(TestValues::our_uid));
    EXPECT_CALL(*unistd_mock_, getpwnam_r(StrEq(kTypedmemdProcessName), _, _, kMaxBufferSize, _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(pwd_), SetArgPointee<4>(&pwd_), Return(score::cpp::blank{})));
}

void SharedMemoryResourceTest::TearDown()
{
    SharedMemoryFactory::SetTypedMemoryProvider(nullptr);
    MemoryResourceRegistry::getInstance().clear();
    SharedMemoryFactory::Clear();
}

void SharedMemoryResourceTest::expectOpenLockFileReturns(const std::string& lock_path,
                                                         score::cpp::expected_blank<Error> return_value,
                                                         bool is_death_test)
{
    if (is_death_test)
    {
        EXPECT_CALL(*stat_mock_, stat(StrEq(lock_path), _, _))
            .Times(::testing::AtMost(1))
            .WillRepeatedly(Return(return_value));
    }
    else
    {
        EXPECT_CALL(*stat_mock_, stat(StrEq(lock_path), _, _)).WillOnce(Return(return_value));
    }
}

void SharedMemoryResourceTest::expectCreateLockFileReturns(const std::string& lock_path,
                                                           score::cpp::expected<std::int32_t, Error> return_value,
                                                           bool is_death_test)
{
    const auto flags = Fcntl::Open::kReadOnly | Fcntl::Open::kCreate | Fcntl::Open::kExclusive;
    if (is_death_test)
    {
        EXPECT_CALL(*fcntl_mock_, open(StrEq(lock_path), flags, _))
            .Times(::testing::AtMost(1))
            .WillRepeatedly(Return(return_value));
    }
    else
    {
        EXPECT_CALL(*fcntl_mock_, open(StrEq(lock_path), flags, _)).WillOnce(Return(return_value));
    }
}

void SharedMemoryResourceTest::expectShmOpenWithCreateFlagReturns(
    const std::string& shm_path,
    score::cpp::expected<std::int32_t, Error> return_value,
    bool is_death_test,
    const bool prefer_typed_memory,
    const score::cpp::expected_blank<score::os::Error> typed_memory_allocation_ret_value)
{
    auto oflag = Fcntl::Open::kReadWrite | Fcntl::Open::kCreate | Fcntl::Open::kExclusive;
    if (prefer_typed_memory == true)
    {
        if (typed_memory_allocation_ret_value.has_value())
        {
            oflag = score::os::Fcntl::Open::kReadWrite | score::os::Fcntl::Open::kExclusive;
        }
        EXPECT_CALL(*typedmemory_mock_, AllocateNamedTypedMemory(_, TestValues::sharedMemorySegmentPath, _))
            .Times(1)
            .WillOnce(Return(typed_memory_allocation_ret_value));
    }
    if (is_death_test)
    {
        EXPECT_CALL(*mman_mock_, shm_open(StrEq(shm_path), oflag, _))
            .Times(AtMost(1))
            .WillRepeatedly(Return(return_value));
    }
    else
    {
        EXPECT_CALL(*mman_mock_, shm_open(StrEq(shm_path), oflag, _)).WillOnce(Return(return_value));
    }
}

void SharedMemoryResourceTest::expectShmOpenWithCreateFlagAndModeReturns(
    const std::string& shm_path,
    const score::os::Stat::Mode mode,
    score::cpp::expected<std::int32_t, Error> return_value,
    bool is_death_test)
{
    const auto oflag = Fcntl::Open::kReadWrite | Fcntl::Open::kCreate | Fcntl::Open::kExclusive;
    if (is_death_test)
    {
        EXPECT_CALL(*mman_mock_, shm_open(StrEq(shm_path), oflag, mode))
            .Times(AtMost(1))
            .WillRepeatedly(Return(return_value));
    }
    else
    {
        EXPECT_CALL(*mman_mock_, shm_open(StrEq(shm_path), oflag, mode)).WillOnce(Return(return_value));
    }
}

void SharedMemoryResourceTest::expectShmOpenReturns(const std::string& shm_path,
                                                    score::cpp::expected<std::int32_t, Error> return_value,
                                                    bool is_read_write,
                                                    bool is_death_test)
{
    const auto oflags = is_read_write ? Fcntl::Open::kReadWrite : Fcntl::Open::kReadOnly;
    if (is_death_test)
    {
        EXPECT_CALL(*mman_mock_, shm_open(StrEq(shm_path), oflags, _))
            .Times(AtMost(1))
            .WillRepeatedly(Return(return_value));
    }
    else
    {
        EXPECT_CALL(*mman_mock_, shm_open(StrEq(shm_path), oflags, _)).WillOnce(Return(return_value));
    }
}

void SharedMemoryResourceTest::expectFstatReturns(std::int32_t file_descriptor,
                                                  bool is_death_test,
                                                  uid_t st_uid,
                                                  std::int64_t st_size,
                                                  score::cpp::expected_blank<score::os::Error> return_value)
{
    auto fstat_action = [st_uid, st_size, return_value](auto, auto& buf) {
        if (return_value)
        {
            buf.st_uid = st_uid;
            buf.st_size = st_size;
        }
        return return_value;
    };
    if (is_death_test)
    {
        EXPECT_CALL(*stat_mock_, fstat(file_descriptor, _)).Times(AtMost(1)).WillRepeatedly(fstat_action);
    }
    else
    {
        EXPECT_CALL(*stat_mock_, fstat(file_descriptor, _)).WillOnce(fstat_action);
    }
}

void SharedMemoryResourceTest::expectMmapReturns(void* const data_region_start,
                                                 const std::int32_t file_descriptor,
                                                 bool is_read_write,
                                                 bool is_death_test)
{
    const auto prot = is_read_write ? (Mman::Protection::kRead | Mman::Protection::kWrite) : Mman::Protection::kRead;
    const auto flags = Mman::Map::kShared;
    if (is_death_test)
    {
        EXPECT_CALL(*mman_mock_, mmap(nullptr, _, prot, flags, file_descriptor, 0))
            .Times(::testing::AtMost(1))
            .WillRepeatedly(Return(data_region_start));
    }
    else
    {
        EXPECT_CALL(*mman_mock_, mmap(nullptr, _, prot, flags, file_descriptor, 0)).WillOnce(Return(data_region_start));
    }
}

void SharedMemoryResourceTest::expectSharedMemorySuccessfullyOpened(std::int32_t file_descriptor,
                                                                    bool is_read_write,
                                                                    void* const data_region_start,
                                                                    uid_t st_uid)
{
    // Given that the lock file does not exist
    expectOpenLockFileReturns(TestValues::sharedMemorySegmentLockPath,
                              score::cpp::make_unexpected(Error::createFromErrno(ENOENT)));

    // That the shared memory segment is opened read only if not otherwise specified.
    expectShmOpenReturns(TestValues::sharedMemorySegmentPath, file_descriptor, is_read_write);
    expectFstatReturns(file_descriptor, false, st_uid);
    expectMmapReturns(data_region_start, file_descriptor, is_read_write);

    // and the memory region is safely unmapped on destruction
    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(file_descriptor)).Times(1);
}

void SharedMemoryResourceTest::expectSharedMemorySuccessfullyCreated(
    const std::int32_t file_descriptor,
    const std::int32_t lock_file_descriptor,
    void* const data_region_start,
    const bool prefer_typed_memory,
    const score::cpp::expected_blank<score::os::Error> typed_memory_allocation_return_value)
{
    InSequence sequence{};
    // Given that we can create the lock file (it did not exist at that point in time)
    expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath, lock_file_descriptor);

    // Then we can create the shared memory and initialize it
    if (prefer_typed_memory == false)
    {
        expectShmOpenWithCreateFlagReturns(TestValues::sharedMemorySegmentPath, file_descriptor);
        expectFstatReturns(file_descriptor);
        EXPECT_CALL(*unistd_mock_, ftruncate(_, _)).WillOnce(Return(score::cpp::blank{}));
    }
    else
    {
        const auto oflags =
            (!typed_memory_allocation_return_value.has_value())
                ? score::os::Fcntl::Open::kReadWrite | score::os::Fcntl::Open::kCreate | score::os::Fcntl::Open::kExclusive
                : score::os::Fcntl::Open::kReadWrite | score::os::Fcntl::Open::kExclusive;

        if (typedmemory_mock_ == nullptr)
        {
            typedmemory_mock_ = std::make_shared<score::memory::shared::TypedMemoryMock>();
        }
        EXPECT_CALL(*typedmemory_mock_, AllocateNamedTypedMemory(_, TestValues::sharedMemorySegmentPath, _))
            .Times(1)
            .WillOnce(Return(typed_memory_allocation_return_value));

        EXPECT_CALL(*mman_mock_, shm_open(StrEq(TestValues::sharedMemorySegmentPath), oflags, _))
            .WillOnce(Return(file_descriptor));

        expectFstatReturns(file_descriptor);
    }

    expectMmapReturns(data_region_start, file_descriptor);

    // and afterwards cleanup the lock file
    EXPECT_CALL(*unistd_mock_, close(lock_file_descriptor));
    EXPECT_CALL(*unistd_mock_, unlink(StrEq(TestValues::sharedMemorySegmentLockPath)));
}

}  // namespace score::memory::shared::test
