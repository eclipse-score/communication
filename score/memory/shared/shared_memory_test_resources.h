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
#ifndef SCORE_LIB_MEMORY_SHARED_SHAREDMEMORYTESTRESOURCES_H
#define SCORE_LIB_MEMORY_SHARED_SHAREDMEMORYTESTRESOURCES_H

#include "score/memory/shared/memory_resource_registry.h"
#include "score/memory/shared/offset_ptr.h"
#include "score/memory/shared/sealedshm/sealedshm_wrapper/sealed_shm_mock.h"
#include "score/memory/shared/shared_memory_resource.h"
#include "score/memory/shared/typedshm/typedshm_wrapper/test/typed_memory_mock.h"
#include "score/os/mocklib/fcntl_mock.h"
#include "score/os/mocklib/mman_mock.h"
#include "score/os/mocklib/stat_mock.h"
#include "score/os/mocklib/unistdmock.h"
#include "score/os/utils/acl/access_control_list.h"

#include <score/expected.hpp>

#include <functional>
#include <memory>
#include <string>

namespace score::memory::shared::test
{

const auto emptyInitCallback = [](std::shared_ptr<ISharedMemoryResource>) {};

/// \brief Checks, whether given pointer/address is accordingly aligned.
/// \param p pointer/address to be checked
/// \param n alignment
/// \return true, in case p is aligned to n, else false
bool is_aligned(const volatile void* const p, const std::size_t n) noexcept;

struct TestValues
{
    static constexpr const char* const sharedMemorySegmentPath = "/my_shm";
    static constexpr const std::uint64_t sharedMemoryResourceIdentifier = 9533397;
    static constexpr const char* const secondSharedMemorySegmentPath = "/my_shm2";
#if defined(__QNX__)
    static constexpr const char* const sharedMemorySegmentLockPath = "/dev/shmem/my_shm_lock";
    static constexpr const char* const secondSharedMemorySegmentLockPath = "/dev/shmem/my_shm2_lock";
#else
    static constexpr const char* const sharedMemorySegmentLockPath = "/tmp/my_shm_lock";
    static constexpr const char* const secondSharedMemorySegmentLockPath = "/tmp/my_shm2_lock";
#endif
    static constexpr std::size_t some_share_memory_size{65535U};
    static constexpr uid_t our_uid{99};
    static constexpr uid_t typedmemd_uid{3020U};
};

class ManagedMemoryResourceTestAttorney
{
  public:
    explicit ManagedMemoryResourceTestAttorney(ManagedMemoryResource& resource) noexcept;

    const void* getEndAddress() const noexcept;

  private:
    ManagedMemoryResource& resource_;
};

class SharedMemoryResourceTestAttorney
{
  public:
    using ControlBlock = SharedMemoryResource::ControlBlock;
    using WorldReadable = SharedMemoryResource::WorldReadable;
    using WorldWritable = SharedMemoryResource::WorldWritable;
    using UserPermissionsMap = SharedMemoryResource::UserPermissionsMap;
    using UserPermissions = SharedMemoryResource::UserPermissions;

    SharedMemoryResourceTestAttorney(SharedMemoryResource& resource) noexcept;

    static score::cpp::expected<std::shared_ptr<SharedMemoryResource>, score::os::Error> Create(
        std::string input_path,
        const std::size_t user_space_to_reserve,
        SharedMemoryResource::InitializeCallback initialize_callback,
        const UserPermissions& permissions = {},
        score::os::IAccessControlList* acl_control_list = nullptr,
        std::shared_ptr<score::memory::shared::TypedMemory> typed_memory_ptr = nullptr) noexcept;

    static score::cpp::expected<std::shared_ptr<SharedMemoryResource>, score::os::Error> CreateAnonymous(
        std::uint64_t shared_memory_resource_id,
        const std::size_t user_space_to_reserve,
        SharedMemoryResource::InitializeCallback initialize_callback,
        const UserPermissions& permissions = {},
        score::os::IAccessControlList* acl_control_list = nullptr,
        std::shared_ptr<score::memory::shared::TypedMemory> typed_memory_ptr = nullptr) noexcept;

    static score::cpp::expected<std::shared_ptr<SharedMemoryResource>, score::os::Error> CreateOrOpen(
        std::string input_path,
        const std::size_t user_space_to_reserve,
        SharedMemoryResource::InitializeCallback initialize_callback,
        const UserPermissions& permissions = {},
        score::os::IAccessControlList* acl_control_list = nullptr,
        std::shared_ptr<score::memory::shared::TypedMemory> typed_memory_ptr = nullptr) noexcept;

    static score::cpp::expected<std::shared_ptr<SharedMemoryResource>, score::os::Error> Open(
        std::string input_path,
        const bool is_read_write,
        score::os::IAccessControlList* acl_control_list = nullptr,
        std::shared_ptr<score::memory::shared::TypedMemory> typed_memory_ptr = nullptr) noexcept;

    static std::string GetLockFilePath(const std::string& input_path) noexcept
    {
        return SharedMemoryResource::GetLockFilePath(input_path);
    }

    void Remove() noexcept
    {
        return resource_.UnlinkFilesystemEntry();
    }

    void* do_allocate(std::size_t bytes, std::size_t alignment)
    {
        return resource_.do_allocate(bytes, alignment);
    }

    static std::size_t GetNeededManagementSpace()
    {
        return SharedMemoryResource::GetNeededManagementSpace();
    };

    void mapMemoryIntoProcess()
    {
        resource_.mapMemoryIntoProcess();
    }

    uid_t getOwnerUid()
    {
        return resource_.getOwnerUid();
    }

    std::uint64_t getMemoryIdentifier() const
    {
        return resource_.memory_identifier_;
    }

    std::shared_ptr<SharedMemoryResource> getSharedPtr()
    {
        return resource_.getSharedPtr();
    }

  private:
    SharedMemoryResource& resource_;
};

class MemoryResourceRegistryAttorney
{
  public:
    MemoryResourceRegistryAttorney(const MemoryResourceRegistry& memory_resource_registry_in)
        : memory_resource_registry{memory_resource_registry_in}
    {
    }

    std::size_t known_regions_size() const
    {
        return memory_resource_registry.region_map_.GetSize();
    }

  private:
    const MemoryResourceRegistry& memory_resource_registry;
};

// Test suit is parametrized to enable test cases to run the test either with assuming that the shared-memory object
// is to be allocated in typed memory (true = GetParam()) or in System-OS (false = GetParam())
class SharedMemoryResourceTest : public ::testing::TestWithParam<bool>
{
  protected:
    SharedMemoryResourceTest();

    void SetUp() override;
    void TearDown() override;

    void expectOpenLockFileReturns(const std::string& lock_path,
                                   score::cpp::expected_blank<score::os::Error> return_value,
                                   bool is_death_test = false);
    void expectCreateLockFileReturns(const std::string& lock_path,
                                     score::cpp::expected<std::int32_t, score::os::Error> return_value,
                                     bool is_death_test = false);
    void expectShmOpenWithCreateFlagReturns(
        const std::string& shm_path,
        score::cpp::expected<std::int32_t, score::os::Error> return_value,
        bool is_death_test = false,
        const bool prefer_typed_memory = false,
        const score::cpp::expected_blank<score::os::Error> typed_memory_allocation_ret_value = score::cpp::blank{});
    void expectShmOpenWithCreateFlagAndModeReturns(const std::string& shm_path,
                                                   const score::os::Stat::Mode mode,
                                                   score::cpp::expected<std::int32_t, score::os::Error> return_value,
                                                   bool is_death_test = false);
    void expectShmOpenReturns(const std::string& shm_path,
                              score::cpp::expected<std::int32_t, score::os::Error> return_value,
                              bool is_read_write = true,
                              bool is_death_test = false);

    void expectFstatReturns(std::int32_t file_descriptor,
                            bool is_death_test = false,
                            uid_t st_uid = TestValues::our_uid,
                            std::int64_t st_size = TestValues::some_share_memory_size,
                            score::cpp::expected_blank<score::os::Error> return_value = score::cpp::blank{});

    void expectMmapReturns(void* const data_region_start,
                           const std::int32_t file_descriptor,
                           bool is_read_write = true,
                           bool is_death_test = false);

    void expectSharedMemorySuccessfullyOpened(std::int32_t file_descriptor,
                                              bool is_read_write,
                                              void* const data_region_start = reinterpret_cast<void*>(1),
                                              uid_t st_uid = TestValues::our_uid);

    void expectSharedMemorySuccessfullyCreated(
        const std::int32_t file_descriptor,
        const std::int32_t lock_file_descriptor,
        void* const data_region_start = reinterpret_cast<void*>(1),
        const bool prefer_typed_memory = false,
        const score::cpp::expected_blank<score::os::Error> typed_memory_allocation_return_value = score::cpp::blank{});

    os::MockGuard<os::StatMock> stat_mock_{};
    os::MockGuard<os::FcntlMock> fcntl_mock_{};
    os::MockGuard<os::UnistdMock> unistd_mock_{};
    os::MockGuard<os::MmanMock> mman_mock_{};
    std::shared_ptr<score::memory::shared::TypedMemoryMock> typedmemory_mock_ =
        std::make_shared<score::memory::shared::TypedMemoryMock>();
    score::memory::shared::SealedShmMock sealedshm_mock_{};

    MemoryResourceRegistryAttorney memory_resource_registry_attorney_;
};

}  // namespace score::memory::shared::test

#endif  // SCORE_LIB_MEMORY_SHARED_SHAREDMEMORYTESTRESOURCES_H
