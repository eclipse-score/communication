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
#include "score/memory/shared/typedshm/typedshm_wrapper/typed_memory.h"
#include "platform/aas/intc/typedmemd/code/clientlib/mock/typedsharedmemory_mock.h"
#include "score/os/mocklib/qnx/mock_mman.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::_;
using ::testing::ByRef;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::SetArgReferee;
using ::testing::StrEq;
using ::testing::StrictMock;

using namespace score::memory::shared;

class TypedMemoryFixture : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mman_mock_ = std::make_unique<StrictMock<score::os::qnx::MmanQnxMock>>();
        shared_memory_mock_ = std::make_unique<StrictMock<score::tmd::TypedSharedMemoryMock>>();
        mman_mock_raw_ptr_ = mman_mock_.get();
        shared_memory_mock_raw_ptr_ = shared_memory_mock_.get();
    };

    void TearDown() override
    {
        ASSERT_TRUE(Mock::VerifyAndClearExpectations(mman_mock_raw_ptr_));
        ASSERT_TRUE(Mock::VerifyAndClearExpectations(shared_memory_mock_raw_ptr_));
    };

    std::unique_ptr<StrictMock<score::os::qnx::MmanQnxMock>> mman_mock_;
    std::unique_ptr<StrictMock<score::tmd::TypedSharedMemoryMock>> shared_memory_mock_;
    StrictMock<score::os::qnx::MmanQnxMock>* mman_mock_raw_ptr_;
    StrictMock<score::tmd::TypedSharedMemoryMock>* shared_memory_mock_raw_ptr_;
};
TEST_F(TypedMemoryFixture, AllocateNamedTypedMemorySuccessPermisionWriteable)
{
    const permission::UserPermissions permissions{permission::WorldWritable{}};
    EXPECT_CALL(*shared_memory_mock_, AllocateNamedTypedMemory(_, _, _)).WillOnce(Return(score::cpp::blank{}));
    score::memory::shared::internal::TypedMemoryImpl typed_memory{std::move(mman_mock_), std::move(shared_memory_mock_)};
    const auto result = typed_memory.AllocateNamedTypedMemory(std::size_t{1}, std::string{"/dev/example"}, permissions);
    EXPECT_TRUE(result.has_value());
}
TEST_F(TypedMemoryFixture, AllocateNamedTypedMemorySuccessPermisionReadable)
{
    const permission::UserPermissions permissions{permission::WorldReadable{}};
    EXPECT_CALL(*shared_memory_mock_, AllocateNamedTypedMemory(_, _, _)).WillOnce(Return(score::cpp::blank{}));
    score::memory::shared::internal::TypedMemoryImpl typed_memory{std::move(mman_mock_), std::move(shared_memory_mock_)};
    const auto result = typed_memory.AllocateNamedTypedMemory(std::size_t{1}, std::string{"/dev/example"}, permissions);
    EXPECT_TRUE(result.has_value());
}
TEST_F(TypedMemoryFixture, AllocateNamedTypedMemorySuccessPermisionExecutable)
{
    permission::UserPermissionsMap map{{score::os::Acl::Permission::kExecute, std::vector<uid_t>{12}}};
    const permission::UserPermissions permissions{map};
    EXPECT_CALL(*shared_memory_mock_, AllocateNamedTypedMemory(_, _, _)).WillOnce(Return(score::cpp::blank{}));
    score::memory::shared::internal::TypedMemoryImpl typed_memory{std::move(mman_mock_), std::move(shared_memory_mock_)};
    const auto result = typed_memory.AllocateNamedTypedMemory(std::size_t{1}, std::string{"/dev/example"}, permissions);
    EXPECT_TRUE(result.has_value());
}
TEST_F(TypedMemoryFixture, AllocateNamedTypedMemoryFail)
{
    const permission::UserPermissions permissions{permission::WorldWritable{}};
    EXPECT_CALL(*shared_memory_mock_, AllocateNamedTypedMemory(_, _, _)).WillOnce([this]() {
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOSYS));
    });
    score::memory::shared::internal::TypedMemoryImpl typed_memory{std::move(mman_mock_), std::move(shared_memory_mock_)};
    const auto result = typed_memory.AllocateNamedTypedMemory(std::size_t{1}, std::string{"/dev/example"}, permissions);
    EXPECT_FALSE(result.has_value());
}

TEST_F(TypedMemoryFixture, AllocateAndOpenAnonymousTypedMemoryAllocateFail)
{
    EXPECT_CALL(*shared_memory_mock_, AllocateHandleTypedMemory(_, _)).WillOnce([this]() {
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOSYS));
    });
    score::memory::shared::internal::TypedMemoryImpl typed_memory{std::move(mman_mock_), std::move(shared_memory_mock_)};
    const auto result = typed_memory.AllocateAndOpenAnonymousTypedMemory(std::size_t{1});
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), score::os::Error::createFromErrno(ENOSYS));
}

TEST_F(TypedMemoryFixture, AllocateAndOpenAnonymousTypedMemoryOpenHandleFail)
{
    EXPECT_CALL(*shared_memory_mock_, AllocateHandleTypedMemory(_, _)).WillOnce(Return(score::cpp::blank{}));
    EXPECT_CALL(*mman_mock_, shm_open_handle(_, _)).WillOnce([this]() {
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOSYS));
    });
    score::memory::shared::internal::TypedMemoryImpl typed_memory{std::move(mman_mock_), std::move(shared_memory_mock_)};
    const auto result = typed_memory.AllocateAndOpenAnonymousTypedMemory(std::size_t{1});
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), score::os::Error::createFromErrno(ENOSYS));
}

TEST_F(TypedMemoryFixture, AllocateAndOpenAnonymousTypedMemoryOpenHandleOk)
{
    EXPECT_CALL(*shared_memory_mock_, AllocateHandleTypedMemory(_, _)).WillOnce(Return(score::cpp::blank{}));
    EXPECT_CALL(*mman_mock_, shm_open_handle(_, _)).WillOnce(Return(1));
    score::memory::shared::internal::TypedMemoryImpl typed_memory{std::move(mman_mock_), std::move(shared_memory_mock_)};
    const auto result = typed_memory.AllocateAndOpenAnonymousTypedMemory(std::size_t{1});
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 1);
}

TEST_F(TypedMemoryFixture, UnlinkOk)
{
    EXPECT_CALL(*shared_memory_mock_, Unlink(_)).WillOnce(Return(score::cpp::blank{}));
    score::memory::shared::internal::TypedMemoryImpl typed_memory{std::move(mman_mock_), std::move(shared_memory_mock_)};
    const auto result = typed_memory.Unlink(std::string{"/dev/example"});
    EXPECT_TRUE(result.has_value());
}

TEST_F(TypedMemoryFixture, UnlinkFail)
{
    EXPECT_CALL(*shared_memory_mock_, Unlink(_))
        .WillOnce(Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOSYS))));
    score::memory::shared::internal::TypedMemoryImpl typed_memory{std::move(mman_mock_), std::move(shared_memory_mock_)};
    const auto result = typed_memory.Unlink(std::string{"/dev/example"});
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), score::os::Error::createFromErrno(ENOSYS));
}

TEST_F(TypedMemoryFixture, DefaultIsNotNull)
{
    auto typed_mem_impl = score::memory::shared::internal::TypedMemoryImpl::Default();
    EXPECT_NE(typed_mem_impl, nullptr);
}
