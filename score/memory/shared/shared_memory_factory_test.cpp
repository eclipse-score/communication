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
#include "score/memory/shared/shared_memory_factory.h"
#include "score/memory/shared/sealedshm/sealedshm_wrapper/sealed_shm.h"
#include "score/memory/shared/shared_memory_test_resources.h"

#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <utility>

#if defined(__QNXNTO__)
constexpr auto kTypedSharedMemoryPathPrefix = "/dev/shmem";
#endif

namespace score::memory::shared::test
{

using namespace ::testing;
using Error = ::score::os::Error;

static constexpr std::size_t kSharedMemorySize{4096};
static constexpr std::int32_t kLockFileDescriptor = 5;
static constexpr std::int32_t kFileDescriptor = 1;

static constexpr uid_t kOurUid = 99;
static constexpr uid_t kTypedmemdUid = 3020;
static constexpr auto kTypedmemdUserName = "typed_memory_daemon";
static constexpr auto kTSHMDeviceName = "/dev/typedshm";
static_assert(kOurUid == TestValues::our_uid, "mock/test UID values mismatch");
static constexpr uid_t kNotOurUid = 1;
static constexpr uid_t kMatchingProviders[] = {1, 2};
static constexpr uid_t kMatchingProviders2[] = {3, 1};
static constexpr uid_t kNonMatchingProviders[] = {2, 3};

namespace
{
class InterVMMemoryProviderScope final
{
  public:
    explicit InterVMMemoryProviderScope(std::shared_ptr<TypedMemory> intervm_memory_ptr) noexcept
    {
        SharedMemoryFactory::SetInterVMMemoryProvider(std::move(intervm_memory_ptr));
    }

    ~InterVMMemoryProviderScope()
    {
        SharedMemoryFactory::SetInterVMMemoryProvider(nullptr);
    }
};

void RunNThreadsToCompletion(const std::function<void()>& function, const std::size_t num_threads)
{
    std::vector<std::thread> threads{};
    for (std::size_t i = 0; i < num_threads; ++i)
    {
        threads.emplace_back(function);
    }
    std::for_each(threads.begin(), threads.end(), [](std::thread& thread) {
        thread.join();
    });
}

int CountNonNullResources(const std::vector<std::shared_ptr<ManagedMemoryResource>>& resources)
{
    int non_null_count{0};
    std::for_each(
        resources.begin(), resources.end(), [&non_null_count](const std::shared_ptr<ManagedMemoryResource>& value) {
            value != nullptr ? ++non_null_count : non_null_count;
        });
    return non_null_count;
}
#if defined(__QNXNTO__)
std::string GetShmFilePath(const std::string& input_path) noexcept
{
    return std::string{kTypedSharedMemoryPathPrefix} + input_path;
}
#endif
}  // namespace

using SharedMemoryFactoryTest = SharedMemoryResourceTest;

INSTANTIATE_TEST_SUITE_P(SharedMemoryFactoryTests, SharedMemoryFactoryTest, ::testing::Values(true, false));

TEST_P(SharedMemoryFactoryTest, ReturnExistingResourceOnReopening)
{
    InSequence sequence{};

    // Given that we can successfully create a shared memory region
    std::array<std::uint8_t, kSharedMemorySize> dataRegion{};

    const bool typed_memory_parameter = GetParam();

    expectSharedMemorySuccessfullyCreated(
        kFileDescriptor, kLockFileDescriptor, dataRegion.data(), typed_memory_parameter);

    // and the memory region is safely unmapped on destruction
    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(kFileDescriptor));

    // Given a resource that has been created and opened
    std::shared_ptr<ManagedMemoryResource> created_resource = SharedMemoryFactory::Create(
        TestValues::sharedMemorySegmentPath,
        [](std::shared_ptr<ISharedMemoryResource>) {},
        kSharedMemorySize,
        {},
        typed_memory_parameter);

    // when requesting the very same resource in the same process
    std::shared_ptr<ManagedMemoryResource> opened_resource =
        SharedMemoryFactory::Open(TestValues::sharedMemorySegmentPath, false);

    // then the already-existing resource is being returned
    ASSERT_EQ(created_resource, opened_resource);
    ASSERT_EQ(created_resource.use_count(), 2);
}

TEST_P(SharedMemoryFactoryTest, CallingRemoveOnNamedResourceWillUnlinkSharedMemoryFile)
{
    InSequence sequence{};

    // Given that we can successfully create a named shared memory resource
    std::array<std::uint8_t, kSharedMemorySize> dataRegion{};

    const bool typed_memory_parameter = GetParam();

    expectSharedMemorySuccessfullyCreated(
        kFileDescriptor, kLockFileDescriptor, dataRegion.data(), typed_memory_parameter);
    // Expecting that the memory region is safely unlinked once from SharedMemoryFactory::Remove()
    if (typed_memory_parameter)
    {
        EXPECT_CALL(*typedmemory_mock_, Unlink(StrEq(TestValues::sharedMemorySegmentPath)))
            .WillOnce(Return(score::cpp::blank{}));
    }
    else
    {
        EXPECT_CALL(*mman_mock_, shm_unlink(StrEq(TestValues::sharedMemorySegmentPath)));
    }

    // and afterwards cleanup the shm file
    EXPECT_CALL(*unistd_mock_, close(kFileDescriptor));

    std::shared_ptr<ManagedMemoryResource> created_resource = SharedMemoryFactory::Create(
        TestValues::sharedMemorySegmentPath,
        [](std::shared_ptr<ISharedMemoryResource>) {},
        kSharedMemorySize,
        {},
        typed_memory_parameter);

    // When removing the resource
    SharedMemoryFactory::Remove(TestValues::sharedMemorySegmentPath);
}

TEST_F(SharedMemoryFactoryTest, CallingRemoveOnTypedNamedResourceWillNotCrashWhenUnlinkSharedMemoryFileFailed)
{
    InSequence sequence{};

    // Given that we can successfully create a named shared memory resource
    std::array<std::uint8_t, kSharedMemorySize> dataRegion{};

    const bool typed_memory_parameter{true};

    expectSharedMemorySuccessfullyCreated(
        kFileDescriptor, kLockFileDescriptor, dataRegion.data(), typed_memory_parameter);

    // Expecting that the memory region is not safely unlinked due to any error
    EXPECT_CALL(*typedmemory_mock_, Unlink(StrEq(TestValues::sharedMemorySegmentPath)))
        .WillOnce(Return(score::cpp::make_unexpected(Error::createFromErrno(ENOENT))));

    // and afterwards cleanup the shm file
    EXPECT_CALL(*unistd_mock_, close(kFileDescriptor));

    std::shared_ptr<ManagedMemoryResource> created_resource = SharedMemoryFactory::Create(
        TestValues::sharedMemorySegmentPath, [](auto) {}, kSharedMemorySize, {}, typed_memory_parameter);

    // When removing the resource
    // Then the program does not crash
    SharedMemoryFactory::Remove(TestValues::sharedMemorySegmentPath);
}

TEST_F(SharedMemoryFactoryTest, WhenRemoveIsCalledOnInvalidPathThenItWillNotCrash)
{
    testing::internal::CaptureStdout();
    constexpr const char* const sharedMemoryInvalidPath = "/InvalidPath";
    // When Remove() is called on invalid path
    SharedMemoryFactory::Remove(sharedMemoryInvalidPath);

    // Then it will not crash
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_THAT(output, ::testing::HasSubstr("Unexpected error while trying to remove"));
}

TEST_P(SharedMemoryFactoryTest, DroppingAfterCreationWillRecreate)
{
    InSequence sequence{};

    // Given that we can successfully create a shared memory region
    std::array<std::uint8_t, kSharedMemorySize> dataRegion{};
    std::array<std::uint8_t, kSharedMemorySize> dataRegion2{};

    const bool typed_memory_parameter = GetParam();

    expectSharedMemorySuccessfullyCreated(
        kFileDescriptor, kLockFileDescriptor, dataRegion.data(), typed_memory_parameter);
    // and the memory region is safely unlinked once from SharedMemoryFactory::Remove()
    if (typed_memory_parameter)
    {
        EXPECT_CALL(*typedmemory_mock_, Unlink(StrEq(TestValues::sharedMemorySegmentPath)))
            .WillOnce(Return(score::cpp::blank{}));
    }
    else
    {
        EXPECT_CALL(*mman_mock_, shm_unlink(StrEq(TestValues::sharedMemorySegmentPath)));
    }

    // and the memory region is safely unmapped on destruction
    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(kFileDescriptor));

    // When creating a resource
    std::shared_ptr<ManagedMemoryResource> created_resource = SharedMemoryFactory::Create(
        TestValues::sharedMemorySegmentPath,
        [](std::shared_ptr<ISharedMemoryResource>) {},
        kSharedMemorySize,
        {},
        typed_memory_parameter);

    // and then unlinking and destroying the resource
    SharedMemoryFactory::Remove(TestValues::sharedMemorySegmentPath);
    created_resource.reset();

    // and then we can recreate the same shared memory region

    expectSharedMemorySuccessfullyCreated(
        kFileDescriptor, kLockFileDescriptor, dataRegion2.data(), typed_memory_parameter);

    // and afterwards cleanup the new memory region
    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(kFileDescriptor));

    // and then creating it again
    std::shared_ptr<ManagedMemoryResource> recreated_resource = SharedMemoryFactory::Create(
        TestValues::sharedMemorySegmentPath,
        [](std::shared_ptr<ISharedMemoryResource>) {},
        kSharedMemorySize,
        {},
        typed_memory_parameter);

    // then a new resource will be returned
    ASSERT_EQ(recreated_resource.use_count(), 1);
}

TEST_P(SharedMemoryFactoryTest, RecreatingWillNotReturnAnInstance)
{
    InSequence sequence{};

    // Given that we can successfully create a shared memory region
    std::array<std::uint8_t, kSharedMemorySize> dataRegion{};

    const bool typed_memory_parameter = GetParam();

    expectSharedMemorySuccessfullyCreated(
        kFileDescriptor, kLockFileDescriptor, dataRegion.data(), typed_memory_parameter);

    // and the memory region is safely unmapped on destruction
    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(1));

    // Given a resource that has already been created
    std::shared_ptr<ManagedMemoryResource> created_resource = SharedMemoryFactory::Create(
        TestValues::sharedMemorySegmentPath,
        [](std::shared_ptr<ISharedMemoryResource>) {},
        kSharedMemorySize,
        {},
        typed_memory_parameter);

    // when creating the same resource again
    std::shared_ptr<ManagedMemoryResource> recreated_resource = SharedMemoryFactory::Create(
        TestValues::sharedMemorySegmentPath,
        [](std::shared_ptr<ISharedMemoryResource>) {},
        kSharedMemorySize,
        {},
        typed_memory_parameter);

    // then we do not get a new instance since it's an already existing path
    ASSERT_EQ(recreated_resource, nullptr);
}

TEST_P(SharedMemoryFactoryTest, SharedMemoryResourceIsCreatedWithCorrectPath)
{
    RecordProperty("Verifies", "SCR-6223575");
    RecordProperty("Description",
                   "The SharedMemoryFactory shall return the Shared Memory Resource associated with the given path.");
    RecordProperty("TestType", "requirements-based");  // requirements test
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "requirements-analysis");  // requirements

    InSequence sequence{};
    // Given that we can successfully create a shared memory region
    std::array<std::uint8_t, kSharedMemorySize> dataRegion{};
    const bool typed_memory_parameter = GetParam();

    expectSharedMemorySuccessfullyCreated(
        kFileDescriptor, kLockFileDescriptor, dataRegion.data(), typed_memory_parameter);

    // and the memory region is safely unmapped on destruction
    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(1));

    // Given a resource that has been created and opened
    auto created_resource = SharedMemoryFactory::Create(
        TestValues::sharedMemorySegmentPath,
        [](std::shared_ptr<ISharedMemoryResource>) {},
        kSharedMemorySize,
        {},
        typed_memory_parameter);

    ASSERT_NE(created_resource, nullptr);
    EXPECT_EQ(*created_resource->getPath(), TestValues::sharedMemorySegmentPath);
    EXPECT_EQ(created_resource->IsShmInTypedMemory(), typed_memory_parameter);
}

TEST_F(SharedMemoryFactoryTest, SharedMemoryResourceFallbackToSystemMemory)
{
    RecordProperty("Verifies", "SCR-6223575");
    RecordProperty("Description",
                   "The SharedMemoryFactory shall return the Shared Memory Resource associated with the given path.");
    RecordProperty("TestType", "requirements-based");  // requirements test
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "requirements-analysis");  // requirements

    InSequence sequence{};
    std::array<std::uint8_t, kSharedMemorySize> dataRegion{};
    // Given that allocation in typed-memory fails
    const auto in_typed_memory_allocated_return_value =
        score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOENT));
    const bool typed_memory_parameter = true;
    expectSharedMemorySuccessfullyCreated(kFileDescriptor,
                                          kLockFileDescriptor,
                                          dataRegion.data(),
                                          typed_memory_parameter,
                                          in_typed_memory_allocated_return_value);

    // and the memory region is safely unmapped on destruction
    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(1));

    // when we create a shared memory object with preference in typed-memory
    auto created_resource = SharedMemoryFactory::Create(
        TestValues::sharedMemorySegmentPath,
        [](std::shared_ptr<ISharedMemoryResource>) {},
        kSharedMemorySize,
        {},
        typed_memory_parameter);

    // expect, that we have a valid resource
    ASSERT_NE(created_resource, nullptr);
    // and that the path is correct
    EXPECT_EQ(*created_resource->getPath(), TestValues::sharedMemorySegmentPath);
    // and that it is NOT residing in typed-memory
    EXPECT_EQ(created_resource->IsShmInTypedMemory(), false);
}

TEST_F(SharedMemoryFactoryTest, SharedMemoryResourceIsOpenedWithCorrectPath)
{
    RecordProperty("Verifies", "SCR-6223575");
    RecordProperty("Description",
                   "The SharedMemoryFactory shall return the Shared Memory Resource associated with the given path.");
    RecordProperty("TestType", "requirements-based");  // requirements test
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "requirements-analysis");  // requirements

    InSequence sequence{};
    constexpr bool is_read_write = false;

    // Given that the shared memory segment is opened read only
    expectSharedMemorySuccessfullyOpened(kFileDescriptor, is_read_write);

    // Given a resource that has been created and opened
    auto opened_resource = SharedMemoryFactory::Open(TestValues::sharedMemorySegmentPath, is_read_write);
    ASSERT_NE(opened_resource, nullptr);

    // Then the SharedMemoryResource's path is the same as that passed to Open()
    EXPECT_EQ(*opened_resource->getPath(), TestValues::sharedMemorySegmentPath);
}

TEST_P(SharedMemoryFactoryTest, FailureToCreateSharedMemoryReturnsNullPtr)
{
    InSequence sequence{};

    const bool typed_memory_parameter = GetParam();

    // When the shared memory resource cannot be created
    expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath,
                                score::cpp::make_unexpected(os::Error::createFromErrno(ENOENT)));

    // When trying to create the shared memory region via the SharedMemoryFactory
    std::shared_ptr<ManagedMemoryResource> created_resource = SharedMemoryFactory::Create(
        TestValues::sharedMemorySegmentPath,
        [](std::shared_ptr<ISharedMemoryResource>) {},
        kSharedMemorySize,
        {},
        typed_memory_parameter);

    // Then the resource returned by the SharedMemoryFactory should be a nullptr
    EXPECT_EQ(created_resource, nullptr);
}

TEST_P(SharedMemoryFactoryTest, FailureToCreateOrOpenSharedMemoryReturnsNullPtr)
{
    InSequence sequence{};
    constexpr std::int32_t lock_file_descriptor = 1;
    constexpr std::int32_t open_lock_file_descriptor = 10;
    constexpr bool is_read_write = true;
    const auto typed_memory_allocation_return_value =
        score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOENT));
    const bool typed_memory_parameter = GetParam();

    // Given that the shared memory resource cannot be created or opened:

    // We successfully acquire the lock file for the first open attempt
    expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath, open_lock_file_descriptor);

    // And the shared memory region doesn't exist when we first try to open it
    expectShmOpenReturns(TestValues::sharedMemorySegmentPath,
                         score::cpp::make_unexpected(Error::createFromErrno(ENOENT)),
                         is_read_write);

    // and the lock file is cleaned up after the failed open attempt
    EXPECT_CALL(*unistd_mock_, close(open_lock_file_descriptor));
    EXPECT_CALL(*unistd_mock_, unlink(StrEq(TestValues::sharedMemorySegmentLockPath)));

    // And we can create the lock file
    expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath, lock_file_descriptor);

    // But the shared memory region now exists when we try to create it
    expectShmOpenWithCreateFlagReturns(TestValues::sharedMemorySegmentPath,
                                       score::cpp::make_unexpected(Error::createFromErrno(EEXIST)),
                                       false,
                                       typed_memory_parameter,
                                       typed_memory_allocation_return_value);

    // and afterwards cleanup the lock file and shared memory
    EXPECT_CALL(*unistd_mock_, close(lock_file_descriptor));
    EXPECT_CALL(*unistd_mock_, unlink(StrEq(TestValues::sharedMemorySegmentLockPath)));

    // Then we fail to open the shared memory region again - acquire lock file for second open attempt
    expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath, open_lock_file_descriptor);

    // And the shared memory region also doesn't exist
    expectShmOpenReturns(
        TestValues::sharedMemorySegmentPath, score::cpp::make_unexpected(Error::createFromErrno(ENOENT)), true);

    // and the lock file is cleaned up after the second failed open attempt
    EXPECT_CALL(*unistd_mock_, close(open_lock_file_descriptor));
    EXPECT_CALL(*unistd_mock_, unlink(StrEq(TestValues::sharedMemorySegmentLockPath)));

    // When creating or opening a shared memory region with CreateOrOpen via the SharedMemoryFactory
    auto created_or_opened_resource = SharedMemoryFactory::CreateOrOpen(
        TestValues::sharedMemorySegmentPath,
        [](std::shared_ptr<ISharedMemoryResource>) {},
        kSharedMemorySize,
        {{}, {}},
        typed_memory_parameter);

    // Then the returned resource should be a nullptr
    EXPECT_EQ(created_or_opened_resource, nullptr);
}

TEST_F(SharedMemoryFactoryTest, FailureToOpenSharedMemoryReturnsNullPtr)
{
    InSequence sequence{};
    constexpr std::int32_t lock_file_descriptor = 10;

    // When the shared memory resource cannot be opened
    expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath, lock_file_descriptor);
    expectShmOpenReturns(
        TestValues::sharedMemorySegmentPath, score::cpp::make_unexpected(Error::createFromErrno(ENOENT)), true);

    // and the lock file is cleaned up after the failed open attempt
    EXPECT_CALL(*unistd_mock_, close(lock_file_descriptor));
    EXPECT_CALL(*unistd_mock_, unlink(StrEq(TestValues::sharedMemorySegmentLockPath)));

    // When trying to open the shared memory region via the SharedMemoryFactory
    std::shared_ptr<ManagedMemoryResource> opened_resource =
        SharedMemoryFactory::Open(TestValues::sharedMemorySegmentPath, true);

    // Then the returned resource should be a nullptr
    EXPECT_EQ(opened_resource, nullptr);
}

TEST_F(SharedMemoryFactoryTest, PreventsToOpenSameFileTwice)
{
    InSequence sequence{};
    constexpr bool is_read_write = false;

    // Given that the shared memory segment is opened read only
    expectSharedMemorySuccessfullyOpened(kFileDescriptor, is_read_write);

    auto unit = SharedMemoryFactory::Open(TestValues::sharedMemorySegmentPath, is_read_write);
    auto other = SharedMemoryFactory::Open(TestValues::sharedMemorySegmentPath, is_read_write);

    EXPECT_EQ(unit, other);
}

TEST_F(SharedMemoryFactoryTest, AllowsAccessToMatchingProvidersPreventsNonMatching)
{
    RecordProperty("Verifies", "SCR-33047276");
    RecordProperty("Description",
                   "Checks that SharedMemoryFactory::Open will return a nullptr if the provided of the "
                   "SharedMemoryResource to be opened is not in the passed list of allowed providers.");
    RecordProperty("TestType", "requirements-based");  // requirements test
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "requirements-analysis");  // requirements

    InSequence sequence{};
    constexpr bool is_read_write = false;

    std::array<std::uint8_t, kSharedMemorySize> data_region{};

    // Given that the shared region is opened (only once), with Owner UID different from ours
    expectSharedMemorySuccessfullyOpened(kFileDescriptor, is_read_write, data_region.data(), kNotOurUid);

    // When trying to access it specifying allowed providers lists
    auto matching_open =
        SharedMemoryFactory::Open(TestValues::sharedMemorySegmentPath, is_read_write, kMatchingProviders);
    auto non_matching_open =
        SharedMemoryFactory::Open(TestValues::sharedMemorySegmentPath, is_read_write, kNonMatchingProviders);
    auto matching_create_or_open = SharedMemoryFactory::CreateOrOpen(TestValues::sharedMemorySegmentPath,
                                                                     [](std::shared_ptr<ISharedMemoryResource>) {},
                                                                     kSharedMemorySize,
                                                                     {{}, {kMatchingProviders2}});
    auto non_matching_create_or_open = SharedMemoryFactory::CreateOrOpen(TestValues::sharedMemorySegmentPath,
                                                                         [](std::shared_ptr<ISharedMemoryResource>) {},
                                                                         kSharedMemorySize,
                                                                         {{}, {kNonMatchingProviders}});
    auto null_providers_create_or_open =
        SharedMemoryFactory::CreateOrOpen(TestValues::sharedMemorySegmentPath,
                                          [](std::shared_ptr<ISharedMemoryResource>) {},
                                          kSharedMemorySize,
                                          {{}, std::nullopt});

    // We get the same resource if its owner is in our requested providers list
    EXPECT_NE(matching_create_or_open, nullptr);
    EXPECT_NE(matching_open, nullptr);
    EXPECT_EQ(matching_open, matching_create_or_open);
    // We get the same resource if requested provider list is nullopt, that means no any restrictions for access to
    // resource
    EXPECT_NE(null_providers_create_or_open, nullptr);
    EXPECT_EQ(null_providers_create_or_open, matching_create_or_open);
    // Otherwise we get nullptr
    EXPECT_EQ(non_matching_open, nullptr);
    EXPECT_EQ(non_matching_create_or_open, nullptr);
}

TEST_F(SharedMemoryFactoryTest, AllowsAccessToOwnResourceWithNonMatchingProvidersList)
{
    InSequence sequence{};
    constexpr bool is_read_write = false;

    std::array<std::uint8_t, kSharedMemorySize> data_region{};

    // Given that the shared region is opened with our own Owner UID
    expectSharedMemorySuccessfullyOpened(kFileDescriptor, is_read_write, data_region.data(), kOurUid);

    // When requesting the resource with non-matching providers list
    std::shared_ptr<ManagedMemoryResource> opened_resource =
        SharedMemoryFactory::Open(TestValues::sharedMemorySegmentPath, false, kNonMatchingProviders);

    // We still get the resource
    EXPECT_NE(opened_resource, nullptr);
}

TEST_F(SharedMemoryFactoryTest, DisallowsAccessToResourceWithEmptyProvidersList)
{
    InSequence sequence{};
    constexpr bool is_read_write = false;

    std::array<std::uint8_t, kSharedMemorySize> data_region{};
    std::array<uid_t, 0> empty_provider_list{};

    // Given that the opened shared memory region was NOT created by our own UID
    expectSharedMemorySuccessfullyOpened(kFileDescriptor, is_read_write, data_region.data(), kNotOurUid);

    // When requesting the resource with non-null but empty provider list
    std::shared_ptr<ManagedMemoryResource> opened_resource =
        SharedMemoryFactory::Open(TestValues::sharedMemorySegmentPath, false, std::make_optional(empty_provider_list));

    // Get nullptr resource
    EXPECT_EQ(opened_resource, nullptr);
}

TEST_F(SharedMemoryFactoryTest, AllowsAccessToResourceWithNullProvidersList)
{
    InSequence sequence{};
    constexpr bool is_read_write = false;

    std::array<std::uint8_t, kSharedMemorySize> data_region{};
    std::nullopt_t null_provider_list{std::nullopt};

    // Given that the opened shared memory region was NOT created by our own UID
    expectSharedMemorySuccessfullyOpened(kFileDescriptor, is_read_write, data_region.data(), kNotOurUid);

    // When requesting the resource with non-null but empty provider list
    std::shared_ptr<ManagedMemoryResource> opened_resource =
        SharedMemoryFactory::Open(TestValues::sharedMemorySegmentPath, false, null_provider_list);

    // We still get the resource, because no checks provided
    EXPECT_NE(opened_resource, nullptr);
}

TEST_P(SharedMemoryFactoryTest, RecreatingDeletedSharedMemoryWorks)
{
    InSequence sequence{};

    // Given that we can successfully create a shared memory region
    std::array<std::uint8_t, kSharedMemorySize> dataRegion{};

    const bool typed_memory_parameter = GetParam();

    expectSharedMemorySuccessfullyCreated(
        kFileDescriptor, kLockFileDescriptor, dataRegion.data(), typed_memory_parameter);

    // and the memory region is safely unmapped on destruction
    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(1));

    // When we create a resource
    std::shared_ptr<ManagedMemoryResource> created_resource = SharedMemoryFactory::Create(
        TestValues::sharedMemorySegmentPath,
        [](std::shared_ptr<ISharedMemoryResource>) {},
        kSharedMemorySize,
        {},
        typed_memory_parameter);

    // And then the resource is destroyed
    created_resource.reset();

    // We can recreate the shared memory region
    expectSharedMemorySuccessfullyCreated(
        kFileDescriptor, kLockFileDescriptor, dataRegion.data(), typed_memory_parameter);

    // and the memory region is again safely unmapped on destruction
    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(1));

    // and we recreate the same resource again
    std::shared_ptr<ManagedMemoryResource> recreated_resource = SharedMemoryFactory::Create(
        TestValues::sharedMemorySegmentPath,
        [](std::shared_ptr<ISharedMemoryResource>) {},
        kSharedMemorySize,
        {},
        typed_memory_parameter);

    // Then we get the recreated resource
    ASSERT_NE(recreated_resource, nullptr);
}

TEST_P(SharedMemoryFactoryTest, ConcurrentlyCreatingSharedMemoryOnlyCreatesResourceOnce)
{
    InSequence sequence{};

    std::vector<std::shared_ptr<ManagedMemoryResource>> resources;
    std::array<std::uint8_t, kSharedMemorySize> data_region{};
    std::mutex mutex_{};

    const bool typed_memory_parameter = GetParam();

    // A shared memory region will only be created once
    expectSharedMemorySuccessfullyCreated(
        kFileDescriptor, kLockFileDescriptor, data_region.data(), typed_memory_parameter);

    // and the memory region will be safely unmapped on destruction
    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(1));

    auto create_activity = [&mutex_, &resources, &typed_memory_parameter] {
        // When a thread tries to create a shared memory region via the SharedMemoryFactory
        std::shared_ptr<ManagedMemoryResource> created_resource = SharedMemoryFactory::Create(
            TestValues::sharedMemorySegmentPath,
            [](std::shared_ptr<ISharedMemoryResource>) {},
            kSharedMemorySize,
            {},
            typed_memory_parameter);

        std::lock_guard<std::mutex> lock{mutex_};
        resources.push_back(created_resource);
    };
    const int num_threads{10};
    RunNThreadsToCompletion(create_activity, num_threads);
    const auto non_null_count = CountNonNullResources(resources);

    // And only one non-null resource will be received amongst all threads trying to create a shared memory region.
    EXPECT_EQ(non_null_count, 1);

    resources.clear();
}

TEST_F(SharedMemoryFactoryTest, ConcurrentlyOpeningSharedMemoryOnlyOpensResourceOnce)
{
    InSequence sequence{};

    const bool is_read_write = true;
    std::vector<std::shared_ptr<ManagedMemoryResource>> resources;
    std::array<std::uint8_t, kSharedMemorySize> data_region{};
    std::mutex mutex_{};

    // A shared memory region will only be opened once
    expectSharedMemorySuccessfullyOpened(kFileDescriptor, is_read_write, data_region.data());

    auto open_activity = [&mutex_, &resources] {
        // When a thread tries to open a shared memory region via the SharedMemoryFactory
        std::shared_ptr<ManagedMemoryResource> created_resource =
            SharedMemoryFactory::Open(TestValues::sharedMemorySegmentPath, is_read_write);

        std::lock_guard<std::mutex> lock{mutex_};
        resources.push_back(created_resource);
    };
    const int num_threads{10};
    RunNThreadsToCompletion(open_activity, num_threads);
    const auto non_null_count = CountNonNullResources(resources);

    // And each thread trying to open a shared memory region will receive a non-null resource.
    EXPECT_EQ(non_null_count, num_threads);
}

TEST_P(SharedMemoryFactoryTest,
       ConcurrentlyCreatingOrOpeningSharedMemoryOnlyCreatesResourceOnceWhenResourceDoesNotExist)
{
    InSequence sequence{};

    std::vector<std::shared_ptr<ManagedMemoryResource>> resources;
    std::array<std::uint8_t, kSharedMemorySize> data_region{};
    std::mutex mutex_{};
    const int num_threads{5};

    const bool typed_memory_parameter = GetParam();

    constexpr std::int32_t open_lock_file_descriptor = 10;

    // Given that we successfully acquire the lock file for the open attempt
    expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath, open_lock_file_descriptor);

    // And the shared memory region also doesn't exist
    expectShmOpenReturns(
        TestValues::sharedMemorySegmentPath, score::cpp::make_unexpected(Error::createFromErrno(ENOENT)), true);

    // and the lock file is cleaned up after the failed open attempt
    EXPECT_CALL(*unistd_mock_, close(open_lock_file_descriptor));
    EXPECT_CALL(*unistd_mock_, unlink(StrEq(TestValues::sharedMemorySegmentLockPath)));

    // A shared memory region will only be created once
    expectSharedMemorySuccessfullyCreated(
        kFileDescriptor, kLockFileDescriptor, data_region.data(), typed_memory_parameter);

    // and the memory region will be safely unmapped on destruction
    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(1));

    auto create_or_open_activity = [&mutex_, &resources, &typed_memory_parameter] {
        // When a thread tries to create or open a shared memory region via the SharedMemoryFactory
        auto created_resource = SharedMemoryFactory::CreateOrOpen(
            TestValues::sharedMemorySegmentPath,
            [](std::shared_ptr<ISharedMemoryResource>) {},
            kSharedMemorySize,
            {{}, {}},
            typed_memory_parameter);

        std::lock_guard<std::mutex> lock{mutex_};
        resources.push_back(created_resource);
        EXPECT_EQ(created_resource->IsShmInTypedMemory(), typed_memory_parameter);
    };

    RunNThreadsToCompletion(create_or_open_activity, num_threads);
    const auto non_null_count = CountNonNullResources(resources);

    // And each thread trying to create or open a shared memory region will receive a non-null resource.
    EXPECT_EQ(non_null_count, num_threads);

    resources.clear();
}

TEST_P(SharedMemoryFactoryTest, ConcurrentlyCreatingOrOpeningSharedMemoryOnlyOpensResourceOnceWhenResourceExists)
{
    InSequence sequence{};

    std::vector<std::shared_ptr<ManagedMemoryResource>> resources;
    std::array<std::uint8_t, kSharedMemorySize> data_region{};
    std::mutex mutex_{};
    const int num_threads{10};

    const bool typed_memory_parameter = GetParam();

    // A shared memory region will only be opened once
    expectSharedMemorySuccessfullyOpened(kFileDescriptor, true, data_region.data(), TestValues::our_uid);

    auto create_or_open_activity = [&mutex_, &resources, &typed_memory_parameter] {
        // When a thread tries to create or open a shared memory region via the SharedMemoryFactory
        auto created_resource = SharedMemoryFactory::CreateOrOpen(
            TestValues::sharedMemorySegmentPath,
            [](std::shared_ptr<ISharedMemoryResource>) {},
            kSharedMemorySize,
            {{}, {}},
            typed_memory_parameter);

        std::lock_guard<std::mutex> lock{mutex_};
        resources.push_back(created_resource);
    };

    RunNThreadsToCompletion(create_or_open_activity, num_threads);
    const auto non_null_count = CountNonNullResources(resources);

    // And each thread trying to create or open a shared memory region will receive a non-null resource.
    EXPECT_EQ(non_null_count, num_threads);

    resources.clear();
}

TEST_F(SharedMemoryFactoryTest, CreatingOrOpeningSharedMemoryInTypedMemoryFailedNoTypedMemoryProvided)
{
    InSequence sequence{};
    constexpr bool create_in_typed_memory = true;

    // given, we have NO typed-memory-provider given to the SharedMemoryFactory
    SharedMemoryFactory::SetTypedMemoryProvider(nullptr);

    // when we try to createOrOpen shared-memory object in typed memory
    auto created_or_opend_resource = SharedMemoryFactory::CreateOrOpen(
        TestValues::sharedMemorySegmentPath,
        [](std::shared_ptr<ISharedMemoryResource>) {},
        kSharedMemorySize,
        {{}, {}},
        create_in_typed_memory);

    // when we try to create shared-memory object in typed memory
    auto created_resource = SharedMemoryFactory::Create(
        TestValues::sharedMemorySegmentPath,
        [](std::shared_ptr<ISharedMemoryResource>) {},
        kSharedMemorySize,
        {},
        create_in_typed_memory);

    // expect, that both are NULL
    EXPECT_EQ(created_or_opend_resource, nullptr);
    EXPECT_EQ(created_resource, nullptr);
}

TEST(SharedMemoryFactoryRemoveStaleArtefactsTest, CallingRemoveStaleArtefactsWillUnlinkAnOldLockFile)
{
    os::MockGuard<os::UnistdMock> unistd_mock{};
    os::MockGuard<os::MmanMock> mman_mock{};
    os::MockGuard<os::StatMock> stat_mock{};
    passwd pwd{};
    pwd.pw_uid = kTypedmemdUid;

    const std::string dummy_input_path{"/my_shared_memory_path"};
    const auto lock_file_path = SharedMemoryResourceTestAttorney::GetLockFilePath(dummy_input_path);

    EXPECT_CALL(*stat_mock, stat(StrEq(kTSHMDeviceName), _, _))
        .Times(AtMost(1))
        .WillRepeatedly(Return(score::cpp::blank{}));
    EXPECT_CALL(*unistd_mock, getpwnam_r(StrEq(kTypedmemdUserName), _, _, _, _))
        .Times(AtMost(1))
        .WillRepeatedly((DoAll(SetArgPointee<1>(pwd), SetArgPointee<4>(&pwd), Return(score::cpp::blank{}))));
    EXPECT_CALL(*stat_mock, stat(HasSubstr(dummy_input_path), _, _))
        .Times(AtMost(1))
        .WillRepeatedly(Return(score::cpp::blank{}));
    EXPECT_CALL(*unistd_mock, unlink(StrEq(lock_file_path.data())));

    SharedMemoryFactory::RemoveStaleArtefacts(dummy_input_path);
}

TEST(SharedMemoryFactoryRemoveStaleArtefactsTest, CallingRemoveStaleArtefactsWillUnlinkAnOldSharedMemoryRegion)
{
    os::MockGuard<os::UnistdMock> unistd_mock{};
    os::MockGuard<os::MmanMock> mman_mock{};
    os::MockGuard<os::StatMock> stat_mock{};
    passwd pwd{};
    pwd.pw_uid = kTypedmemdUid;

    const std::string dummy_input_path{"/my_shared_memory_path"};

    EXPECT_CALL(*stat_mock, stat(StrEq(kTSHMDeviceName), _, _))
        .Times(AtMost(1))
        .WillRepeatedly(Return(score::cpp::blank{}));
    EXPECT_CALL(*unistd_mock, getpwnam_r(StrEq(kTypedmemdUserName), _, _, _, _))
        .Times(AtMost(1))
        .WillRepeatedly((DoAll(SetArgPointee<1>(pwd), SetArgPointee<4>(&pwd), Return(score::cpp::blank{}))));
    EXPECT_CALL(*stat_mock, stat(HasSubstr(dummy_input_path), _, _))
        .Times(AtMost(1))
        .WillRepeatedly(Return(score::cpp::blank{}));
    EXPECT_CALL(*mman_mock, shm_unlink(StrEq(dummy_input_path.data())));

    SharedMemoryFactory::RemoveStaleArtefacts(dummy_input_path);
}

// typed memory daemon is only running on the QNX, so these tests will only pass on the QNX
#if defined(__QNXNTO__)
TEST(SharedMemoryFactoryRemoveStaleArtefactsTest,
     CallingRemoveStaleArtefactsWillUnlinkAnOldSharedMemoryRegionWhenAcquireTmdUidFailed)
{
    os::MockGuard<os::UnistdMock> unistd_mock{};
    os::MockGuard<os::MmanMock> mman_mock{};
    os::MockGuard<os::StatMock> stat_mock{};

    const std::string dummy_input_path{"/my_shared_memory_path"};

    EXPECT_CALL(*stat_mock, stat(StrEq(kTSHMDeviceName), _, _)).WillOnce(Return(score::cpp::blank{}));
    EXPECT_CALL(*unistd_mock, getpwnam_r(StrEq(kTypedmemdUserName), _, _, _, _))
        .WillOnce(Return(score::cpp::make_unexpected(Error::createFromErrno(ENOENT))));
    EXPECT_CALL(*mman_mock, shm_unlink(StrEq(dummy_input_path.data())));

    SharedMemoryFactory::RemoveStaleArtefacts(dummy_input_path);
}

TEST(SharedMemoryFactoryRemoveStaleArtefactsTest,
     CallingRemoveStaleArtefactsWillUnlinkAnOldSharedMemoryRegionWhenAcquireTmdUidUserDoesNotExist)
{
    os::MockGuard<os::UnistdMock> unistd_mock{};
    os::MockGuard<os::MmanMock> mman_mock{};
    os::MockGuard<os::StatMock> stat_mock{};
    passwd* pwd = nullptr;
    const std::string dummy_input_path{"/my_shared_memory_path"};

    EXPECT_CALL(*stat_mock, stat(StrEq(kTSHMDeviceName), _, _)).WillOnce(Return(score::cpp::blank{}));
    EXPECT_CALL(*unistd_mock, getpwnam_r(StrEq(kTypedmemdUserName), _, _, _, _))
        .WillOnce((DoAll(SetArgPointee<4>(pwd), Return(score::cpp::blank{}))));
    EXPECT_CALL(*mman_mock, shm_unlink(StrEq(dummy_input_path.data())));

    SharedMemoryFactory::RemoveStaleArtefacts(dummy_input_path);
}

TEST(SharedMemoryFactoryRemoveStaleArtefactsTest,
     CallingRemoveStaleArtefactsWillUnlinkAnOldSharedMemoryRegionWhenTypedshmDeviceDoesNotExist)
{
    os::MockGuard<os::UnistdMock> unistd_mock{};
    os::MockGuard<os::MmanMock> mman_mock{};
    os::MockGuard<os::StatMock> stat_mock{};
    const std::string dummy_input_path{"/my_shared_memory_path"};

    EXPECT_CALL(*stat_mock, stat(StrEq(kTSHMDeviceName), _, _))
        .WillOnce(Return(score::cpp::make_unexpected(Error::createFromErrno(ENOENT))));
    EXPECT_CALL(*unistd_mock, getpwnam_r(StrEq(kTypedmemdUserName), _, _, _, _)).Times(0);
    EXPECT_CALL(*mman_mock, shm_unlink(StrEq(dummy_input_path.data())));

    SharedMemoryFactory::RemoveStaleArtefacts(dummy_input_path);
}

TEST(SharedMemoryFactoryRemoveStaleArtefactsTest,
     CallingRemoveStaleArtefactsWillUnlinkAnOldSharedMemoryRegionWhenTypedshmDeviceInNotAccessible)
{
    os::MockGuard<os::UnistdMock> unistd_mock{};
    os::MockGuard<os::MmanMock> mman_mock{};
    os::MockGuard<os::StatMock> stat_mock{};
    const std::string dummy_input_path{"/my_shared_memory_path"};

    EXPECT_CALL(*stat_mock, stat(StrEq(kTSHMDeviceName), _, _))
        .WillOnce(Return(score::cpp::make_unexpected(Error::createFromErrno(EACCES))));
    EXPECT_CALL(*unistd_mock, getpwnam_r(StrEq(kTypedmemdUserName), _, _, _, _)).Times(0);
    EXPECT_CALL(*mman_mock, shm_unlink(StrEq(dummy_input_path.data())));

    SharedMemoryFactory::RemoveStaleArtefacts(dummy_input_path);
}

TEST_F(SharedMemoryFactoryTest, CallingRemoveStaleArtefactsWillUnlinkAnOldTypedSharedMemoryRegion)
{
    const std::string dummy_input_path{"/my_shared_memory_path"};
    const auto shm_file_path = GetShmFilePath(dummy_input_path);
    score::os::StatBuffer stat_buffer{};
    stat_buffer.st_uid = kTypedmemdUid;

    // Given that the typed shm object has been allocated via typedmemd
    ON_CALL(*stat_mock_, stat(StrEq(shm_file_path.data()), _, _))
        .WillByDefault(DoAll(SetArgReferee<1>(stat_buffer), Return(score::cpp::blank())));

    // and that the Unlink success
    EXPECT_CALL(*typedmemory_mock_, Unlink(StrEq(dummy_input_path))).WillOnce(Return(score::cpp::blank{}));

    // When calling RemoveStaleArtefacts
    SharedMemoryFactory::RemoveStaleArtefacts(dummy_input_path);
}

TEST_F(SharedMemoryFactoryTest, CallingRemoveStaleArtefactsWillNotCrashWhenUnlinkFailed)
{
    const std::string dummy_input_path{"/my_shared_memory_path"};
    const auto shm_file_path = GetShmFilePath(dummy_input_path);
    score::os::StatBuffer stat_buffer{};
    stat_buffer.st_uid = kTypedmemdUid;

    // Given that the shm object has been allocated via typedmemd
    ON_CALL(*stat_mock_, stat(StrEq(shm_file_path.data()), _, _))
        .WillByDefault(DoAll(SetArgReferee<1>(stat_buffer), Return(score::cpp::blank())));

    // and that the Unlink fails
    EXPECT_CALL(*typedmemory_mock_, Unlink(StrEq(dummy_input_path)))
        .WillOnce(Return(score::cpp::make_unexpected(Error::createFromErrno(ENOENT))));

    // When calling RemoveStaleArtefacts
    // Then the program does not crash
    SharedMemoryFactory::RemoveStaleArtefacts(dummy_input_path);
}
#endif

TEST_F(SharedMemoryFactoryTest, CallingRemoveStaleArtefactsAfterCreatingWillTerminate)
{
    const auto remove_stale_artefacts_after_creating = [this] {
        InSequence sequence{};

        // Given that we can successfully create a shared memory region
        std::array<std::uint8_t, kSharedMemorySize> dataRegion{};

        const bool typed_memory_parameter = GetParam();

        expectSharedMemorySuccessfullyCreated(
            kFileDescriptor, kLockFileDescriptor, dataRegion.data(), typed_memory_parameter);

        // and the memory region is safely unmapped on destruction
        EXPECT_CALL(*mman_mock_, munmap(_, _));
        EXPECT_CALL(*unistd_mock_, close(kFileDescriptor));

        // Given a resource that has been created and opened
        std::shared_ptr<ManagedMemoryResource> created_resource = SharedMemoryFactory::Create(
            TestValues::sharedMemorySegmentPath,
            [](std::shared_ptr<ISharedMemoryResource>) {},
            kSharedMemorySize,
            {},
            typed_memory_parameter);
        ASSERT_NE(created_resource, nullptr);

        SharedMemoryFactory::RemoveStaleArtefacts(TestValues::sharedMemorySegmentPath);
    };
    EXPECT_DEATH(remove_stale_artefacts_after_creating(), ".*");
}

TEST_F(SharedMemoryFactoryTest, CreatingAnonymousSharedMemoryInTypedMemory)
{
    InSequence sequence{};
    const score::cpp::expected<int, score::os::Error> typed_memory_allocation_return_value{kFileDescriptor};
    std::array<std::uint8_t, kSharedMemorySize> data_region{};
    constexpr bool create_in_typed_memory = true;
    bool isInitialized = false;

    EXPECT_CALL(*typedmemory_mock_, AllocateAndOpenAnonymousTypedMemory(_))
        .Times(1)
        .WillOnce(Return(typed_memory_allocation_return_value));

    expectFstatReturns(kFileDescriptor);

    expectMmapReturns(data_region.data(), kFileDescriptor);

    SharedMemoryFactory::SetTypedMemoryProvider(std::move(typedmemory_mock_));

    auto created_resource = SharedMemoryFactory::CreateAnonymous(
        TestValues::sharedMemoryResourceIdentifier,
        [&isInitialized](auto) {
            isInitialized = true;
        },
        kSharedMemorySize,
        {},
        create_in_typed_memory);

    EXPECT_NE(nullptr, created_resource);
    EXPECT_TRUE(created_resource->IsShmInTypedMemory());
    EXPECT_TRUE(isInitialized);
}

TEST_F(SharedMemoryFactoryTest, CreatingAnonymousSharedMemoryInTypedMemoryFailsWhenTypedMemoryProvidedSetToNullptr)
{
    InSequence sequence{};
    constexpr bool create_in_typed_memory = true;

    SharedMemoryFactory::SetTypedMemoryProvider(nullptr);

    auto created_resource = SharedMemoryFactory::CreateAnonymous(
        TestValues::sharedMemoryResourceIdentifier,
        [](std::shared_ptr<ISharedMemoryResource>) {},
        kSharedMemorySize,
        {},
        create_in_typed_memory);

    EXPECT_EQ(nullptr, created_resource);
}

TEST_F(SharedMemoryFactoryTest, CreatingAnonymousSharedMemoryInSystemMemory)
{
    InSequence sequence{};
    constexpr bool prefer_system_memory = false;
    std::array<std::uint8_t, kSharedMemorySize> data_region{};
    bool isInitialized = false;
    const score::cpp::expected<std::int32_t, score::os::Error> open_anonymous_return_value{kFileDescriptor};
    const score::cpp::expected_blank<score::os::Error> seal_return_value{};

    // No typed memory allocation is expected since prefer_typed_memory is false
    EXPECT_CALL(*typedmemory_mock_, AllocateAndOpenAnonymousTypedMemory(_)).Times(0);

    score::memory::shared::SealedShm::InjectMock(&sealedshm_mock_);
    EXPECT_CALL(sealedshm_mock_, OpenAnonymous(_)).Times(1).WillOnce(Return(open_anonymous_return_value));

    expectFstatReturns(kFileDescriptor);

    EXPECT_CALL(sealedshm_mock_, Seal(kFileDescriptor, _)).Times(1).WillOnce(Return(seal_return_value));

    expectMmapReturns(data_region.data(), kFileDescriptor);

    // When creating anonymous shared memory without typed memory
    auto created_resource = SharedMemoryFactory::CreateAnonymous(
        TestValues::sharedMemoryResourceIdentifier,
        [&isInitialized](auto) {
            isInitialized = true;
        },
        kSharedMemorySize,
        {},
        prefer_system_memory);

    score::memory::shared::SealedShm::InjectMock(nullptr);

    // Then a valid resource is returned and it is not in typed memory
    EXPECT_NE(nullptr, created_resource);
    EXPECT_FALSE(created_resource->IsShmInTypedMemory());
    EXPECT_TRUE(isInitialized);
}

using SharedMemoryFactoryDeathTest = SharedMemoryFactoryTest;

INSTANTIATE_TEST_SUITE_P(SharedMemoryFactoryDeathTests, SharedMemoryFactoryDeathTest, ::testing::Values(true, false));

TEST_P(SharedMemoryFactoryDeathTest, CreatingSharedMemoryTerminate)
{
    InSequence sequence{};
    auto oflags =
        score::os::Fcntl::Open::kReadWrite | score::os::Fcntl::Open::kCreate | score::os::Fcntl::Open::kExclusive;
    score::cpp::expected<std::int32_t, Error> ret_value = score::cpp::make_unexpected(Error::createFromErrno(EBADF));
    const bool typed_memory_parameter = GetParam();

    if (typed_memory_parameter == true)
    {
        oflags = score::os::Fcntl::Open::kReadWrite | score::os::Fcntl::Open::kExclusive;

        const score::cpp::expected_blank<score::os::Error> memory_is_allocated_in_typed_memory{};

        EXPECT_CALL(*typedmemory_mock_, AllocateNamedTypedMemory(_, TestValues::sharedMemorySegmentPath, _))
            .WillRepeatedly(Return(memory_is_allocated_in_typed_memory));
    }

    EXPECT_CALL(*mman_mock_, shm_open(StrEq(TestValues::sharedMemorySegmentPath), oflags, _))
        .Times(AtMost(1))
        .WillRepeatedly(Return(ret_value));

    EXPECT_DEATH(SharedMemoryFactory::Create(
                     TestValues::sharedMemorySegmentPath,
                     [](std::shared_ptr<ISharedMemoryResource>) {},
                     kSharedMemorySize,
                     {},
                     typed_memory_parameter),
                 ".*");
}

TEST_F(SharedMemoryFactoryDeathTest, FailingToInsertResourceIntoRegistryTerminates)
{
    InSequence sequence{};
    constexpr bool is_read_write = false;

    // Given that we successfully acquire the lock file
    constexpr std::int32_t lock_file_descriptor = 10;
    expectCreateLockFileReturns(TestValues::sharedMemorySegmentLockPath, lock_file_descriptor);

    // and the shared memory segment is opened.
    expectShmOpenReturns(TestValues::sharedMemorySegmentPath, kFileDescriptor, is_read_write);
    // expect fstat call returning shared-mem-object size of shm-object file.
    expectFstatReturns(kFileDescriptor);

    // and that the "/dev/typedshm" device exists
    EXPECT_CALL(*stat_mock_, stat(StrEq(kTSHMDeviceName), _, _))
        .Times(AtMost(1))
        .WillRepeatedly(Return(score::cpp::blank{}));

    // and the memory region is mapped into the process
    expectMmapReturns(reinterpret_cast<void*>(1), kFileDescriptor, is_read_write, true);

    // and the lock file is cleaned up after Open completes
    EXPECT_CALL(*unistd_mock_, close(lock_file_descriptor));
    EXPECT_CALL(*unistd_mock_, unlink(StrEq(TestValues::sharedMemorySegmentLockPath)));

    // and the memory region is mapped a second time by the additional call to mapMemoryIntroProcess
    expectMmapReturns(reinterpret_cast<void*>(1), -1, is_read_write, true);

    // and the memory region will not necessarily be safely unmapped on destruction (to hide gmock warning)
    EXPECT_CALL(*mman_mock_, munmap(_, _)).Times(AtMost(1));
    EXPECT_CALL(*unistd_mock_, close(kFileDescriptor)).Times(AtMost(1));

    // Given a resource that has been opened and added to the SharedMemoryFactory's internal map
    std::shared_ptr<SharedMemoryResource> opened_resource = std::dynamic_pointer_cast<SharedMemoryResource>(
        SharedMemoryFactory::Open(TestValues::sharedMemorySegmentPath, is_read_write));
    ASSERT_NE(opened_resource, nullptr);

    // Trying to insert the memory region into the SharedMemoryFactory's internal map a second time causes the program
    // to terminate.
    SharedMemoryResourceTestAttorney resource_attorney{*opened_resource};
    EXPECT_DEATH(resource_attorney.mapMemoryIntoProcess(), ".*");
}

TEST_F(SharedMemoryFactoryTest, OpenWithInterVmPathReturnsNullptrWhenProviderNotSet)
{
    // Given a shared memory path using the inter-VM prefix
    const std::string inter_vm_path{"/intervm-shared-shmem/lola-data-0000000000004660-00001"};

    // And no InterVM memory provider has been set

    // When opening shared memory using this path
    // Then nullptr is returned because InterVM provider is required but not set
    const auto result = SharedMemoryFactory::Open(inter_vm_path, false);
    EXPECT_EQ(result, nullptr);
}

TEST_F(SharedMemoryFactoryTest, CreateWithInterVmPathReturnsNullptrWhenProviderNotSet)
{
    // Given a shared memory path using the inter-VM prefix
    const std::string inter_vm_path{"/intervm-shared-shmem/lola-ctl-0000000000004660-00001-b"};

    // And no InterVM memory provider has been set

    // When creating shared memory using this path
    // Then nullptr is returned because InterVM provider is required but not set
    const auto result = SharedMemoryFactory::Create(
        inter_vm_path, [](std::shared_ptr<ISharedMemoryResource>) {}, kSharedMemorySize, {}, false);
    EXPECT_EQ(result, nullptr);
}

TEST_F(SharedMemoryFactoryTest, CreateOrOpenWithInterVmPathReturnsNullptrWhenProviderNotSet)
{
    // Given a shared memory path using the inter-VM prefix
    const std::string inter_vm_path{"/intervm-shared-shmem/lola-methods-0000000000004660-00001-00002-00003"};

    // And no InterVM memory provider has been set

    // When creating or opening shared memory using this path
    // Then nullptr is returned because InterVM provider is required but not set
    const auto result = SharedMemoryFactory::CreateOrOpen(
        inter_vm_path,
        [](std::shared_ptr<ISharedMemoryResource>) {},
        kSharedMemorySize,
        SharedMemoryResource::AccessControl{{}, {}},
        false);
    EXPECT_EQ(result, nullptr);
}

TEST_F(SharedMemoryFactoryTest, OpenWithInterVmPathSucceedsWhenProviderIsSet)
{
    InSequence sequence{};

    // Given a shared memory path using the inter-VM prefix
    const std::string inter_vm_path{"/intervm-shared-shmem/lola-data-0000000000004660-00001"};
    const auto lock_file_path = SharedMemoryResourceTestAttorney::GetLockFilePath(inter_vm_path);

    // And an InterVM memory provider has been set
    auto intervm_mock = std::make_shared<TypedMemoryMock>();
    InterVMMemoryProviderScope intervm_memory_provider_scope{intervm_mock};
    SharedMemoryFactory::SetTypedMemoryProvider(nullptr);

    // And the shared memory can be opened successfully
    constexpr bool is_read_write = false;
    expectCreateLockFileReturns(lock_file_path, kLockFileDescriptor);
    expectShmOpenReturns(inter_vm_path, kFileDescriptor, is_read_write);
    expectFstatReturns(kFileDescriptor);
    expectMmapReturns(reinterpret_cast<void*>(1), kFileDescriptor, is_read_write);
    EXPECT_CALL(*unistd_mock_, close(kLockFileDescriptor));
    EXPECT_CALL(*unistd_mock_, unlink(StrEq(lock_file_path)));
    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(kFileDescriptor));

    // When opening shared memory using this path
    const auto result = SharedMemoryFactory::Open(inter_vm_path, is_read_write);

    // Then the resource is successfully opened
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*result->getPath(), inter_vm_path);
    EXPECT_FALSE(result->IsShmInTypedMemory());
}

TEST_F(SharedMemoryFactoryTest, CreateWithInterVmPathSucceedsWhenProviderIsSet)
{
    InSequence sequence{};

    // Given a shared memory path using the inter-VM prefix
    const std::string inter_vm_path{"/intervm-shared-shmem/lola-ctl-0000000000004660-00001-b"};
    const auto lock_file_path = SharedMemoryResourceTestAttorney::GetLockFilePath(inter_vm_path);

    // And an InterVM memory provider has been set
    auto intervm_mock = std::make_shared<TypedMemoryMock>();
    InterVMMemoryProviderScope intervm_memory_provider_scope{intervm_mock};
    SharedMemoryFactory::SetTypedMemoryProvider(nullptr);

    // And shared memory can be created successfully using the InterVM provider
    std::array<std::uint8_t, kSharedMemorySize> dataRegion{};
    const bool prefer_typed_memory = false;
    const auto oflags = score::os::Fcntl::Open::kReadWrite | score::os::Fcntl::Open::kExclusive;
    expectCreateLockFileReturns(lock_file_path, kLockFileDescriptor);
    EXPECT_CALL(*intervm_mock, AllocateNamedTypedMemory(_, inter_vm_path, _)).WillOnce(Return(score::cpp::blank{}));
    EXPECT_CALL(*mman_mock_, shm_open(StrEq(inter_vm_path), oflags, _)).WillOnce(Return(kFileDescriptor));
    expectFstatReturns(kFileDescriptor);
    EXPECT_CALL(*stat_mock_, stat(StrEq(kTSHMDeviceName), _, _))
        .Times(::testing::AtMost(1))
        .WillRepeatedly(Return(score::cpp::blank{}));
    expectMmapReturns(dataRegion.data(), kFileDescriptor);
    EXPECT_CALL(*unistd_mock_, close(kLockFileDescriptor));
    EXPECT_CALL(*unistd_mock_, unlink(StrEq(lock_file_path)));

    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(kFileDescriptor));

    // When creating shared memory using this path
    const auto result = SharedMemoryFactory::Create(
        inter_vm_path, [](std::shared_ptr<ISharedMemoryResource>) {}, kSharedMemorySize, {}, prefer_typed_memory);

    // Then the resource is successfully created
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*result->getPath(), inter_vm_path);
    EXPECT_TRUE(result->IsShmInTypedMemory());
}

TEST_F(SharedMemoryFactoryTest, CreateOrOpenWithInterVmPathSucceedsWhenProviderIsSet)
{
    InSequence sequence{};

    // Given a shared memory path using the inter-VM prefix
    const std::string inter_vm_path{"/intervm-shared-shmem/lola-methods-0000000000004660-00001-00002-00003"};
    const auto lock_file_path = SharedMemoryResourceTestAttorney::GetLockFilePath(inter_vm_path);

    // And an InterVM memory provider has been set
    auto intervm_mock = std::make_shared<TypedMemoryMock>();
    InterVMMemoryProviderScope intervm_memory_provider_scope{intervm_mock};
    SharedMemoryFactory::SetTypedMemoryProvider(nullptr);

    // And shared memory can be created successfully (CreateOrOpen tries Create after Open fails)
    std::array<std::uint8_t, kSharedMemorySize> dataRegion{};
    const bool prefer_typed_memory = false;
    const auto oflags = score::os::Fcntl::Open::kReadWrite | score::os::Fcntl::Open::kExclusive;

    // CreateOrOpen will first try to Open (which fails), then Create
    constexpr std::int32_t open_lock_file_descriptor = 10;
    constexpr bool is_read_write = true;

    // Expect the Open attempt to fail
    expectCreateLockFileReturns(lock_file_path, open_lock_file_descriptor);
    expectShmOpenReturns(inter_vm_path, score::cpp::make_unexpected(Error::createFromErrno(ENOENT)), is_read_write);
    EXPECT_CALL(*unistd_mock_, close(open_lock_file_descriptor));
    EXPECT_CALL(*unistd_mock_, unlink(StrEq(lock_file_path)));

    // Then expect the Create to succeed
    expectCreateLockFileReturns(lock_file_path, kLockFileDescriptor);
    EXPECT_CALL(*intervm_mock, AllocateNamedTypedMemory(_, inter_vm_path, _)).WillOnce(Return(score::cpp::blank{}));
    EXPECT_CALL(*mman_mock_, shm_open(StrEq(inter_vm_path), oflags, _)).WillOnce(Return(kFileDescriptor));
    expectFstatReturns(kFileDescriptor);
    EXPECT_CALL(*stat_mock_, stat(StrEq(kTSHMDeviceName), _, _))
        .Times(::testing::AtMost(1))
        .WillRepeatedly(Return(score::cpp::blank{}));
    expectMmapReturns(dataRegion.data(), kFileDescriptor);
    EXPECT_CALL(*unistd_mock_, close(kLockFileDescriptor));
    EXPECT_CALL(*unistd_mock_, unlink(StrEq(lock_file_path)));

    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(kFileDescriptor));

    // When creating or opening shared memory using this path
    const auto result = SharedMemoryFactory::CreateOrOpen(
        inter_vm_path,
        [](std::shared_ptr<ISharedMemoryResource>) {},
        kSharedMemorySize,
        SharedMemoryResource::AccessControl{{}, {}},
        prefer_typed_memory);

    // Then the resource is successfully created or opened
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*result->getPath(), inter_vm_path);
    EXPECT_TRUE(result->IsShmInTypedMemory());
}

}  // namespace score::memory::shared::test
