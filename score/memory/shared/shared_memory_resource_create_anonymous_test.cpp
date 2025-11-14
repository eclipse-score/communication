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
#include "score/memory/shared/sealedshm/sealedshm_wrapper/sealed_shm.h"
#include "score/memory/shared/shared_memory_test_resources.h"
#include "score/os/utils/acl/access_control_list_mock.h"

#include "gtest/gtest.h"

#include <memory>

using ::testing::_;
using ::testing::AtMost;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Return;

namespace score::memory::shared::test
{

constexpr std::int32_t file_descriptor = 1;

using SharedMemoryResourceCreateAnonymousTest = SharedMemoryResourceTest;
TEST_F(SharedMemoryResourceCreateAnonymousTest, CreatingAnonymousSharedMemoryInTypedMemorySucceeded)
{
    InSequence sequence{};
    const score::cpp::expected<int, score::os::Error> typed_memory_allocation_return_value{file_descriptor};
    auto typedmemory_mock = std::make_shared<score::memory::shared::TypedMemoryMock>();
    std::array<std::uint8_t, 500U> data_region{};
    bool isInitialized = false;

    EXPECT_CALL(*typedmemory_mock, AllocateAndOpenAnonymousTypedMemory(_))
        .Times(1)
        .WillOnce(Return(typed_memory_allocation_return_value));

    expectFstatReturns(file_descriptor);

    expectMmapReturns(data_region.data(), file_descriptor);

    const auto resource_result = SharedMemoryResourceTestAttorney::CreateAnonymous(
        TestValues::sharedMemoryResourceIdentifier,
        TestValues::some_share_memory_size,
        [&isInitialized](std::shared_ptr<ISharedMemoryResource>) {
            isInitialized = true;
        },
        {},
        nullptr,
        typedmemory_mock);
    ASSERT_TRUE(isInitialized);
    ASSERT_TRUE(resource_result.has_value());

    auto resource = resource_result.value();
    EXPECT_NE(nullptr, resource);
    EXPECT_EQ(nullptr, resource->getPath());
    EXPECT_TRUE(resource->IsShmInTypedMemory());
}

TEST_F(SharedMemoryResourceCreateAnonymousTest, CreatingAnonymousSharedMemoryInTypedMemoryFailed)
{
    InSequence sequence{};
    score::memory::shared::SealedShm::InjectMock(&sealedshm_mock_);
    const auto typed_memory_allocation_error_return_value =
        score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOENT));
    const score::cpp::expected<std::int32_t, score::os::Error> create_anonymous_return_value{file_descriptor};
    const score::cpp::expected_blank<score::os::Error> seal_return_value{};
    std::array<std::uint8_t, 500U> data_region{};
    const auto readAccessForEveryBody =
        ::score::os::ModeToInteger(::score::os::Stat::Mode::kReadUser | ::score::os::Stat::Mode::kWriteUser |
                                 ::score::os::Stat::Mode::kReadGroup | ::score::os::Stat::Mode::kReadOthers);
    bool isInitialized = false;

    EXPECT_CALL(*typedmemory_mock_, AllocateAndOpenAnonymousTypedMemory(_))
        .Times(1)
        .WillOnce(Return(typed_memory_allocation_error_return_value));

    EXPECT_CALL(sealedshm_mock_, OpenAnonymous(Eq(readAccessForEveryBody)))
        .Times(1)
        .WillOnce(Return(create_anonymous_return_value));

    expectFstatReturns(file_descriptor);

    EXPECT_CALL(sealedshm_mock_, Seal(file_descriptor, _)).Times(1).WillOnce(Return(seal_return_value));

    expectMmapReturns(data_region.data(), file_descriptor);

    const auto resource_result = SharedMemoryResourceTestAttorney::CreateAnonymous(
        TestValues::sharedMemoryResourceIdentifier,
        TestValues::some_share_memory_size,
        [&isInitialized](std::shared_ptr<ISharedMemoryResource>) {
            isInitialized = true;
        },
        permission::WorldReadable(),
        nullptr,
        typedmemory_mock_);
    ASSERT_TRUE(isInitialized);
    ASSERT_TRUE(resource_result.has_value());

    auto resource = resource_result.value();
    EXPECT_NE(nullptr, resource);
    EXPECT_EQ(nullptr, resource->getPath());
    EXPECT_FALSE(resource->IsShmInTypedMemory());
}

TEST_F(SharedMemoryResourceCreateAnonymousTest, CreatingAnonymousSharedMemoryInSystemMemorySucceeded)
{
    InSequence sequence{};
    score::memory::shared::SealedShm::InjectMock(&sealedshm_mock_);
    const score::cpp::expected<std::int32_t, score::os::Error> create_anonymous_return_value{file_descriptor};
    const score::cpp::expected_blank<score::os::Error> seal_return_value{};
    std::array<std::uint8_t, 500U> data_region{};
    const auto readAccessForEveryBody =
        ::score::os::ModeToInteger(::score::os::Stat::Mode::kReadUser | ::score::os::Stat::Mode::kWriteUser |
                                 ::score::os::Stat::Mode::kReadGroup | ::score::os::Stat::Mode::kReadOthers);
    bool isInitialized = false;

    EXPECT_CALL(*typedmemory_mock_, AllocateAndOpenAnonymousTypedMemory(_)).Times(0);

    EXPECT_CALL(sealedshm_mock_, OpenAnonymous(Eq(readAccessForEveryBody)))
        .Times(1)
        .WillOnce(Return(create_anonymous_return_value));

    expectFstatReturns(file_descriptor);

    EXPECT_CALL(sealedshm_mock_, Seal(file_descriptor, _)).Times(1).WillOnce(Return(seal_return_value));

    expectMmapReturns(data_region.data(), file_descriptor);

    const auto resource_result = SharedMemoryResourceTestAttorney::CreateAnonymous(
        TestValues::sharedMemoryResourceIdentifier,
        TestValues::some_share_memory_size,
        [&isInitialized](std::shared_ptr<ISharedMemoryResource>) {
            isInitialized = true;
        },
        permission::WorldReadable(),
        nullptr,
        nullptr);
    ASSERT_TRUE(isInitialized);
    ASSERT_TRUE(resource_result.has_value());

    auto resource = resource_result.value();
    EXPECT_NE(nullptr, resource);
    EXPECT_EQ(nullptr, resource->getPath());
    EXPECT_FALSE(resource->IsShmInTypedMemory());
}

TEST_F(SharedMemoryResourceCreateAnonymousTest, CreatingAnonymousSharedMemoryInSystemMemorySealFailed)
{
    InSequence sequence{};
    score::memory::shared::SealedShm::InjectMock(&sealedshm_mock_);
    const score::cpp::expected<std::int32_t, score::os::Error> create_anonymous_return_value{file_descriptor};
    const auto seal_error_return_value{score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOENT))};
    std::array<std::uint8_t, 500U> data_region{};
    const auto readAccessForEveryBody =
        ::score::os::ModeToInteger(::score::os::Stat::Mode::kReadUser | ::score::os::Stat::Mode::kWriteUser |
                                 ::score::os::Stat::Mode::kReadGroup | ::score::os::Stat::Mode::kReadOthers);
    bool isInitialized = false;

    EXPECT_CALL(*typedmemory_mock_, AllocateAndOpenAnonymousTypedMemory(_)).Times(0);

    EXPECT_CALL(sealedshm_mock_, OpenAnonymous(Eq(readAccessForEveryBody)))
        .Times(1)
        .WillOnce(Return(create_anonymous_return_value));

    expectFstatReturns(file_descriptor);

    EXPECT_CALL(sealedshm_mock_, Seal(file_descriptor, _)).Times(1).WillOnce(Return(seal_error_return_value));

    EXPECT_CALL(*unistd_mock_, ftruncate(_, _)).WillOnce(Return(score::cpp::blank{}));

    expectMmapReturns(data_region.data(), file_descriptor);

    const auto resource_result = SharedMemoryResourceTestAttorney::CreateAnonymous(
        TestValues::sharedMemoryResourceIdentifier,
        TestValues::some_share_memory_size,
        [&isInitialized](std::shared_ptr<ISharedMemoryResource>) {
            isInitialized = true;
        },
        permission::WorldReadable(),
        nullptr,
        nullptr);
    ASSERT_TRUE(isInitialized);
    ASSERT_TRUE(resource_result.has_value());

    auto resource = resource_result.value();
    EXPECT_NE(nullptr, resource);
    EXPECT_EQ(nullptr, resource->getPath());
    EXPECT_FALSE(resource->IsShmInTypedMemory());
}

using SharedMemoryResourceCreateAnonymousDeathTest = SharedMemoryResourceCreateAnonymousTest;
TEST_F(SharedMemoryResourceCreateAnonymousDeathTest, CreatingAnonymousSharedMemoryFailureTerminates)
{
    InSequence sequence{};
    score::memory::shared::SealedShm::InjectMock(&sealedshm_mock_);
    const auto create_anonymous_error_return_value{score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOENT))};
    const auto readAccessForEveryBody =
        ::score::os::ModeToInteger(::score::os::Stat::Mode::kReadUser | ::score::os::Stat::Mode::kWriteUser |
                                 ::score::os::Stat::Mode::kReadGroup | ::score::os::Stat::Mode::kReadOthers);

    EXPECT_CALL(*typedmemory_mock_, AllocateAndOpenAnonymousTypedMemory(_)).Times(0);

    EXPECT_CALL(sealedshm_mock_, OpenAnonymous(Eq(readAccessForEveryBody)))
        .Times(AtMost(1))
        .WillRepeatedly(Return(create_anonymous_error_return_value));

    EXPECT_DEATH(SharedMemoryResourceTestAttorney::CreateAnonymous(TestValues::sharedMemoryResourceIdentifier,
                                                                   TestValues::some_share_memory_size,
                                                                   emptyInitCallback,
                                                                   permission::WorldReadable(),
                                                                   nullptr,
                                                                   nullptr),
                 ".*");
}

}  // namespace score::memory::shared::test
