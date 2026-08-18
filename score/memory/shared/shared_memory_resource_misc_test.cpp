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
#include "score/memory/shared/sealedshm/sealedshm_wrapper/sealed_shm.h"
#include "score/memory/shared/shared_memory_test_resources.h"

#include "fake/my_memory_resource.h"

#include "score/hash.hpp"

#include "gtest/gtest.h"

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

using SharedMemoryResourceMiscTest = SharedMemoryResourceTest;
TEST_F(SharedMemoryResourceMiscTest, GettingUsableBaseAddressWithValidControlBlockReturnsAddressAfterControlBlock)
{
    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 1;
    constexpr std::int32_t lock_file_descriptor = 5;

    // Given that we can successfully create a shared memory region
    // the data region, where mmap shall place the mapping (which would in reality ALWAYS be PAGE aligned) should be
    // max-aligned
    alignas(alignof(std::max_align_t)) std::array<std::uint8_t, 500U> dataRegion{};
    expectSharedMemorySuccessfullyCreated(file_descriptor, lock_file_descriptor, dataRegion.data());

    // and the memory region is safely unmapped on destruction
    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(file_descriptor));

    // When constructing a SharedMemoryResource with create option
    auto resource_result = SharedMemoryResourceTestAttorney::Create(
        TestValues::sharedMemorySegmentPath, TestValues::some_share_memory_size, emptyInitCallback);
    ASSERT_TRUE(resource_result.has_value());
    auto resource = resource_result.value();
    SharedMemoryResourceTestAttorney resource_attorney{*resource};

    std::size_t bytes{8};
    std::size_t alignment{8};
    resource_attorney.do_allocate(bytes, alignment);

    // expect, that the mapping of the SharedMemoryResource starts at the given region
    EXPECT_EQ(resource->getBaseAddress(), dataRegion.data());
    // and that the usable base address is behind the control block plus some eventual padding/alignment
    // (see SharedMemoryResource::initializeControlBlock())
    EXPECT_EQ(resource->getUsableBaseAddress(),
              dataRegion.data() + SharedMemoryResourceTestAttorney::GetNeededManagementSpace());
    // and that the usable base address is worst-case aligned
    EXPECT_TRUE(is_aligned(resource->getUsableBaseAddress(), alignof(std::max_align_t)));
}

TEST_F(SharedMemoryResourceMiscTest, GettingBaseAddressWithControlBlockReturnsCorrectAddress)
{
    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 1;
    constexpr std::int32_t lock_file_descriptor = 5;

    // Given that we can successfully create a shared memory region
    alignas(std::alignment_of<ControlBlock>::value) std::array<std::uint8_t, 500U> dataRegion{};
    expectSharedMemorySuccessfullyCreated(file_descriptor, lock_file_descriptor, dataRegion.data());

    // and the memory region is safely unmapped on destruction
    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(file_descriptor));

    // When constructing a SharedMemoryResource with create option
    auto resource_result = SharedMemoryResourceTestAttorney::Create(
        TestValues::sharedMemorySegmentPath, TestValues::some_share_memory_size, emptyInitCallback);
    ASSERT_TRUE(resource_result.has_value());
    auto resource = resource_result.value();

    EXPECT_EQ(resource->getBaseAddress(), dataRegion.data());
}

TEST_F(SharedMemoryResourceMiscTest, GetMemoryIdentifierReturnsCorrectly)
{
    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 1;
    constexpr std::int32_t lock_file_descriptor = 5;

    // Given that we can successfully create a shared memory region
    alignas(std::alignment_of<ControlBlock>::value) std::array<std::uint8_t, 500U> dataRegion{};
    expectSharedMemorySuccessfullyCreated(file_descriptor, lock_file_descriptor, dataRegion.data());

    // and the memory region is safely unmapped on destruction
    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(file_descriptor));

    // When constructing a SharedMemoryResource with create option
    auto resource_result = SharedMemoryResourceTestAttorney::Create(
        TestValues::sharedMemorySegmentPath, TestValues::some_share_memory_size, emptyInitCallback);
    ASSERT_TRUE(resource_result.has_value());
    auto resource = resource_result.value();

    // Then the memory identifier should be created from the path passed into the SharedMemoryResource constructor.
    SharedMemoryResourceTestAttorney resource_attorney{*resource};
    const std::string* path_ptr = resource->getPath();
    ASSERT_TRUE(nullptr != path_ptr);
    ASSERT_EQ(score::cpp::hash_bytes(path_ptr->data(), path_ptr->size()), resource_attorney.getMemoryIdentifier());
}

TEST_F(SharedMemoryResourceMiscTest, GetIdentifierOnNamedResourceReturnsPath)
{
    // Given we can successfully create a shared-memory region
    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 1;
    constexpr std::int32_t lock_file_descriptor = 5;

    alignas(std::alignment_of<ControlBlock>::value) std::array<std::uint8_t, 500U> data_region{};
    expectSharedMemorySuccessfullyCreated(file_descriptor, lock_file_descriptor, data_region.data());

    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(file_descriptor));

    // Given we can successfully construct a named SharedMemoryResource
    auto named_resource_result = SharedMemoryResourceTestAttorney::Create(
        TestValues::sharedMemorySegmentPath, TestValues::some_share_memory_size, emptyInitCallback);
    ASSERT_TRUE(named_resource_result.has_value());
    auto named_resource = named_resource_result.value();
    ASSERT_NE(nullptr, named_resource);

    // When getting the identifier of the named shared memory resource
    const auto named_identifier = named_resource->GetIdentifier();

    // Then the identifier has the path format
    EXPECT_EQ(named_identifier, std::string("file: ") + TestValues::sharedMemorySegmentPath);
}

TEST_F(SharedMemoryResourceMiscTest, GetIdentifierOnAnonymousResourceReturnsIdString)
{
    // Given we can successfully construct an anonymous SharedMemoryResource
    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 1;
    score::memory::shared::SealedShm::InjectMock(&sealedshm_mock_);
    const score::cpp::expected<std::int32_t, score::os::Error> create_anonymous_return_value{file_descriptor};
    const score::cpp::expected_blank<score::os::Error> seal_return_value{};
    std::array<std::uint8_t, 500U> data_region{};

    ON_CALL(sealedshm_mock_, OpenAnonymous(_)).WillByDefault(Return(create_anonymous_return_value));
    expectFstatReturns(file_descriptor);
    ON_CALL(sealedshm_mock_, Seal(file_descriptor, _)).WillByDefault(Return(seal_return_value));
    expectMmapReturns(data_region.data(), file_descriptor);

    const auto resource_result = SharedMemoryResourceTestAttorney::CreateAnonymous(
        TestValues::sharedMemoryResourceIdentifier, TestValues::some_share_memory_size, emptyInitCallback);
    ASSERT_TRUE(resource_result.has_value());
    auto anonymous_resource = resource_result.value();
    ASSERT_NE(nullptr, anonymous_resource);

    // When getting the identifier of the named shared memory resource
    const auto anonymous_identifier = anonymous_resource->GetIdentifier();

    // Then the identifier has the path format
    ASSERT_EQ(anonymous_identifier, std::string("id: ") + std::to_string(TestValues::sharedMemoryResourceIdentifier));
}

TEST_F(SharedMemoryResourceMiscTest, GettingSharedPtrToSharedMemoryResourceDestructsResourceOnce)
{
    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 1;
    constexpr std::int32_t lock_file_descriptor = 5;

    // Given that we can successfully create a shared memory region
    alignas(std::alignment_of<ControlBlock>::value) std::array<std::uint8_t, 500U> dataRegion{};
    expectSharedMemorySuccessfullyCreated(file_descriptor, lock_file_descriptor, dataRegion.data());

    // and the memory region is safely unmapped on destruction
    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(file_descriptor));

    // When constructing a SharedMemoryResource with create option
    auto resource_result = SharedMemoryResourceTestAttorney::Create(
        TestValues::sharedMemorySegmentPath, TestValues::some_share_memory_size, emptyInitCallback);
    ASSERT_TRUE(resource_result.has_value());
    auto resource = resource_result.value();

    // When creating shared pointers to the resource
    // Then the pointers should share the same control block as the shared_ptr created from
    // SharedMemoryResource::CreateInstance. This is reflected in the use count.
    SharedMemoryResourceTestAttorney resource_attorney{*resource};
    const auto shared_ptr_0 = resource_attorney.getSharedPtr();

    // The resource_result and resource each
    //  hold a shared_ptr to the resource, so creating a new shared_ptr will have a use_count of 3
    EXPECT_EQ(shared_ptr_0.use_count(), 3);

    const auto shared_ptr_1 = resource_attorney.getSharedPtr();
    EXPECT_EQ(shared_ptr_1.use_count(), 4);

    // And when the shared_ptrs and SharedMemoryResource are destroyed, the destructor is only called once, and we don't
    // crash
}

TEST_F(SharedMemoryResourceMiscTest, CallingGetFileDescriptorReturnsFileDescriptorOfShmRegion)
{
    InSequence sequence{};
    constexpr std::int32_t file_descriptor = 1;
    constexpr std::int32_t lock_file_descriptor = 5;

    // Given that we can successfully create a shared memory region with a specific file descriptor
    alignas(std::alignment_of<ControlBlock>::value) std::array<std::uint8_t, 500U> dataRegion{};
    expectSharedMemorySuccessfullyCreated(file_descriptor, lock_file_descriptor, dataRegion.data());

    // and the memory region is safely unmapped on destruction
    EXPECT_CALL(*mman_mock_, munmap(_, _));
    EXPECT_CALL(*unistd_mock_, close(file_descriptor));

    auto resource_result = SharedMemoryResourceTestAttorney::Create(
        TestValues::sharedMemorySegmentPath, TestValues::some_share_memory_size, emptyInitCallback);

    // Then GetFileDescriptor should return the file descriptor of the shared memory object
    EXPECT_EQ(resource_result.value()->GetFileDescriptor(), file_descriptor);
}

using SharedMemoryResourceMiscDeathTest = SharedMemoryResourceMiscTest;
TEST(CalculateAlignedSizeTest, SizeEqualsAlignment)
{
    std::size_t size = 4U;
    std::size_t alignment = size;

    EXPECT_EQ(CalculateAlignedSize(size, alignment), size);
}

TEST(CalculateAlignedSizeTest, SizeIsIntegerMultipleOfAlignment)
{
    std::size_t alignment = 4U;
    std::size_t size = 3U * alignment;

    EXPECT_EQ(CalculateAlignedSize(size, alignment), size);
}

TEST(CalculateAlignedSizeTest, SizeIsSmallerThanAlignment)
{
    std::size_t alignment = 4U;
    std::size_t size = alignment - 1U;

    EXPECT_EQ(CalculateAlignedSize(size, alignment), alignment);
}

TEST(CalculateAlignedSizeTest, SizeIsSlightlyBiggerThanAlignment)
{
    std::size_t alignment = 4U;
    std::size_t size = alignment + 1U;

    EXPECT_EQ(CalculateAlignedSize(size, alignment), 2U * alignment);
}

TEST(CalculateAlignedSizeTest, AssertDeathWhenAlignmentIsZero)
{
    std::size_t alignment = 0U;
    std::size_t size = alignment + 1U;

    EXPECT_DEATH(CalculateAlignedSize(size, alignment), ".*");
}

}  // namespace score::memory::shared::test
