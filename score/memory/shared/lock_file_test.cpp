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
#include "score/memory/shared/lock_file.h"

#include "score/os/errno.h"
#include "score/os/mocklib/fcntl_mock.h"
#include "score/os/mocklib/stat_mock.h"
#include "score/os/mocklib/unistdmock.h"
#include "score/os/stat.h"

#include <score/expected.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <cstdint>
#include <string>
#include <utility>

namespace score::memory::shared
{
namespace
{

using ::testing::_;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::StrEq;

using Mode = ::score::os::Stat::Mode;
using Open = ::score::os::Fcntl::Open;

const std::string kLockFilePath{"/test_lock_file"};
const std::int32_t kLockFileDescriptor{1234};

class LockFileTestFixture : public ::testing::Test
{
  protected:
    os::MockGuard<os::FcntlMock> fcntl_mock_{};
    os::MockGuard<os::StatMock> stat_mock_{};
    os::MockGuard<os::UnistdMock> unistd_mock_{};
};

using LockFileCreateFixture = LockFileTestFixture;
TEST_F(LockFileCreateFixture, LockFileShouldManageFileWithRaii)
{
    bool close_called{false};
    bool unlink_called{false};

    // Expect that open is called on the file which returns a lock file descriptor
    EXPECT_CALL(*fcntl_mock_, open(StrEq(kLockFilePath.data()), _, _)).WillOnce(Return(kLockFileDescriptor));

    // Expect that chmod is called on the file to compensate any umask inflicted right adoptions
    EXPECT_CALL(*stat_mock_, chmod(StrEq(kLockFilePath.data()), _))
        .WillOnce(Return(score::cpp::expected_blank<score::os::Error>{}));

    // and the file is closed and unlinked
    EXPECT_CALL(*unistd_mock_, close(kLockFileDescriptor))
        .WillOnce(InvokeWithoutArgs([&close_called]() -> score::cpp::expected_blank<score::os::Error> {
            close_called = true;
            return {};
        }));
    EXPECT_CALL(*unistd_mock_, unlink(StrEq(kLockFilePath.data())))
        .WillOnce(InvokeWithoutArgs([&unlink_called]() -> score::cpp::expected_blank<score::os::Error> {
            unlink_called = true;
            return {};
        }));

    {
        // When we successfully Create a LockFile
        const auto lock_file_result = LockFile::Create(kLockFilePath);
        ASSERT_TRUE(lock_file_result.has_value());

        // And the file won't be closed or unlinked
        EXPECT_FALSE(close_called);
        EXPECT_FALSE(unlink_called);
    }

    // until the lock file is destroyed
    EXPECT_TRUE(close_called);
    EXPECT_TRUE(unlink_called);
}

TEST_F(LockFileCreateFixture, LockFileCreateShouldReturnEmptyIfOpenFails)
{
    // Expecting that open is called on the file and returns an error
    EXPECT_CALL(*fcntl_mock_, open(StrEq(kLockFilePath.data()), _, _))
        .WillOnce(Return(score::cpp::make_unexpected(os::Error::createFromErrno(ENOENT))));

    // When we Create a LockFile
    const auto lock_file_result = LockFile::Create(kLockFilePath);

    // Then an empty result is returned
    ASSERT_FALSE(lock_file_result.has_value());
}

TEST_F(LockFileCreateFixture, LockFileCreateDoesNotCloseAndUnlinkFileIfOpenFails)
{
    // Expecting open is called on the file and returns an error
    EXPECT_CALL(*fcntl_mock_, open(StrEq(kLockFilePath.data()), _, _))
        .WillOnce(Return(score::cpp::make_unexpected(os::Error::createFromErrno(ENOENT))));

    // Then close and unlink is never called
    EXPECT_CALL(*unistd_mock_, close(_)).Times(0);
    EXPECT_CALL(*unistd_mock_, unlink(_)).Times(0);

    // When we Create a LockFile
    score::cpp::ignore = LockFile::Create(kLockFilePath);
}

TEST_F(LockFileCreateFixture, LockFileCreateShouldReturnEmptyIfChmodFails)
{
    // Expect that open is called on the file which returns a lock file descriptor
    EXPECT_CALL(*fcntl_mock_, open(StrEq(kLockFilePath.data()), _, _)).WillOnce(Return(kLockFileDescriptor));

    // but expecting that chmod is called on the file which returns an error
    EXPECT_CALL(*stat_mock_, chmod(StrEq(kLockFilePath.data()), _))
        .WillOnce(Return(score::cpp::make_unexpected(os::Error::createFromErrno(ENOENT))));

    // When we Create a LockFile
    const auto lock_file_result = LockFile::Create(kLockFilePath);

    // Then an empty result is returned
    ASSERT_FALSE(lock_file_result.has_value());
}

TEST_F(LockFileCreateFixture, LockFileCreateShouldCallOpenWithExclusiveFlag)
{
    const auto exclusive_opening_flags = Open::kExclusive | Open::kCreate | Open::kReadOnly;

    // Expect that open is called on the file with the exclusive flag set
    EXPECT_CALL(*fcntl_mock_, open(StrEq(kLockFilePath.data()), exclusive_opening_flags, _));

    // When we Create a LockFile
    ASSERT_TRUE(LockFile::Create(kLockFilePath).has_value());
}

TEST_F(LockFileCreateFixture, IfLockFileCreateIsCalledThenLockFileShouldBeClosedAndUnlinkedOnDestruction)
{
    const auto exclusive_opening_flags = Open::kExclusive | Open::kCreate | Open::kReadOnly;
    bool close_called{false};
    bool unlink_called{false};
    {
        // Expect that open is called on the file
        EXPECT_CALL(*fcntl_mock_, open(StrEq(kLockFilePath.data()), exclusive_opening_flags, _))
            .WillOnce(Return(kLockFileDescriptor));

        // and the file is closed and unlinked
        EXPECT_CALL(*unistd_mock_, close(kLockFileDescriptor))
            .WillOnce(InvokeWithoutArgs([&close_called]() -> score::cpp::expected_blank<score::os::Error> {
                close_called = true;
                return score::cpp::blank{};
            }));
        EXPECT_CALL(*unistd_mock_, unlink(StrEq(kLockFilePath.data())))
            .WillOnce(InvokeWithoutArgs([&unlink_called]() -> score::cpp::expected_blank<score::os::Error> {
                unlink_called = true;
                return score::cpp::blank{};
            }));

        // When we successfully Create a LockFile
        auto lock_file_result = LockFile::Create(kLockFilePath);
        ASSERT_TRUE(lock_file_result.has_value());
    }
    EXPECT_TRUE(close_called);
    EXPECT_TRUE(unlink_called);
}

using LockFileCreateOrOpenFixture = LockFileTestFixture;
TEST_F(LockFileCreateOrOpenFixture, LockFileCreateOrOpenShouldCallOpenWithoutExclusiveFlag)
{
    const auto opening_flags = Open::kCreate | Open::kReadOnly;

    // Expect that open is called on the file without the exclusive flag set
    EXPECT_CALL(*fcntl_mock_, open(StrEq(kLockFilePath.data()), opening_flags, _));

    // When we CreateOrOpen a LockFile
    const bool take_ownership{false};
    ASSERT_TRUE(LockFile::CreateOrOpen(kLockFilePath, take_ownership).has_value());
}

TEST_F(LockFileCreateOrOpenFixture, LockFileCreateOrOpenShouldReturnEmptyIfOpenFails)
{
    // Expecting that open is called on the file and returns an error
    EXPECT_CALL(*fcntl_mock_, open(StrEq(kLockFilePath.data()), _, _))
        .WillOnce(Return(score::cpp::make_unexpected(os::Error::createFromErrno(ENOENT))));

    // When we CreateOrOpen a LockFile
    const bool take_ownership{false};
    const auto lock_file_result = LockFile::CreateOrOpen(kLockFilePath, take_ownership);

    // Then an empty result is returned
    ASSERT_FALSE(lock_file_result.has_value());
}

TEST_F(LockFileCreateOrOpenFixture, LockFileCreateOrOpenDoesNotCloseAndUnlinkFileIfOpenFails)
{
    // Expecting that open is called on the file and returns an error
    EXPECT_CALL(*fcntl_mock_, open(StrEq(kLockFilePath.data()), _, _))
        .WillOnce(Return(score::cpp::make_unexpected(os::Error::createFromErrno(ENOENT))));

    // Then close and unlink is never called
    EXPECT_CALL(*unistd_mock_, close(_)).Times(0);
    EXPECT_CALL(*unistd_mock_, unlink(_)).Times(0);

    // When we CreateOrOpen a LockFile
    const bool take_ownership{false};
    score::cpp::ignore = LockFile::CreateOrOpen(kLockFilePath, take_ownership);
}

TEST_F(LockFileCreateOrOpenFixture, LockFileCreateOrOpenShouldReturnEmptyIfChmodFails)
{
    // Expect that open is called on the file which returns a lock file descriptor
    EXPECT_CALL(*fcntl_mock_, open(StrEq(kLockFilePath.data()), _, _)).WillOnce(Return(kLockFileDescriptor));

    // but expecting that chmod is called on the file which returns an error
    EXPECT_CALL(*stat_mock_, chmod(StrEq(kLockFilePath.data()), _))
        .WillOnce(Return(score::cpp::make_unexpected(os::Error::createFromErrno(ENOENT))));

    // When we CreateOrOpen a LockFile
    const bool take_ownership{false};
    const auto lock_file_result = LockFile::CreateOrOpen(kLockFilePath, take_ownership);

    // Then an empty result is returned
    ASSERT_FALSE(lock_file_result.has_value());
}

TEST_F(LockFileCreateOrOpenFixture, IfLockFileCreateOrOpenIsCalledThenLockFileShouldBeClosedButNotUnlinkedOnDestruction)
{
    const auto opening_flags = Open::kCreate | Open::kReadOnly;
    bool close_called{false};
    {
        // Expect that open is called on the file
        EXPECT_CALL(*fcntl_mock_, open(StrEq(kLockFilePath.data()), opening_flags, _))
            .WillOnce(Return(kLockFileDescriptor));

        // and the file is closed and unlinked
        EXPECT_CALL(*unistd_mock_, close(kLockFileDescriptor))
            .WillOnce(InvokeWithoutArgs([&close_called]() -> score::cpp::expected_blank<score::os::Error> {
                close_called = true;
                return score::cpp::blank{};
            }));
        EXPECT_CALL(*unistd_mock_, unlink(StrEq(kLockFilePath.data()))).Times(0);

        // When we successfully CreateOrOpen a LockFile
        const bool take_ownership{false};
        auto lock_file_result = LockFile::CreateOrOpen(kLockFilePath, take_ownership);
        ASSERT_TRUE(lock_file_result.has_value());

        EXPECT_FALSE(close_called);
    }
    EXPECT_TRUE(close_called);
}

TEST_F(LockFileCreateOrOpenFixture,
       IfLockFileCreateOrOpenWithOwnershipFlagSetIsCalledThenLockFileShouldBeClosedAndUnlinkedOnDestruction)
{
    const auto exclusive_opening_flags = Open::kCreate | Open::kReadOnly;
    bool close_called{false};
    bool unlink_called{false};
    {
        // Expect that open is called on the file
        EXPECT_CALL(*fcntl_mock_, open(StrEq(kLockFilePath.data()), exclusive_opening_flags, _))
            .WillOnce(Return(kLockFileDescriptor));

        // and the file is closed and unlinked
        EXPECT_CALL(*unistd_mock_, close(kLockFileDescriptor))
            .WillOnce(InvokeWithoutArgs([&close_called]() -> score::cpp::expected_blank<score::os::Error> {
                close_called = true;
                return score::cpp::blank{};
            }));
        EXPECT_CALL(*unistd_mock_, unlink(StrEq(kLockFilePath.data())))
            .WillOnce(InvokeWithoutArgs([&unlink_called]() -> score::cpp::expected_blank<score::os::Error> {
                unlink_called = true;
                return score::cpp::blank{};
            }));

        // When we successfully CreateOrOpen a LockFile with the take_ownership flag set
        const bool take_ownership{true};
        auto lock_file_result = LockFile::CreateOrOpen(kLockFilePath, take_ownership);
        ASSERT_TRUE(lock_file_result.has_value());

        EXPECT_FALSE(close_called);
        EXPECT_FALSE(unlink_called);
    }
    EXPECT_TRUE(close_called);
    EXPECT_TRUE(unlink_called);
}

TEST_F(LockFileCreateOrOpenFixture,
       IfLockFileCreateOrOpenAndTakeOwnershipIsCalledThenLockFileShouldBeClosedAndUnlinkedOnDestruction)
{
    const auto exclusive_opening_flags = Open::kCreate | Open::kReadOnly;
    bool close_called{false};
    bool unlink_called{false};
    {
        // Expect that open is called on the file
        EXPECT_CALL(*fcntl_mock_, open(StrEq(kLockFilePath.data()), exclusive_opening_flags, _))
            .WillOnce(Return(kLockFileDescriptor));

        // and the file is closed and unlinked
        EXPECT_CALL(*unistd_mock_, close(kLockFileDescriptor))
            .WillOnce(InvokeWithoutArgs([&close_called]() -> score::cpp::expected_blank<score::os::Error> {
                close_called = true;
                return score::cpp::blank{};
            }));
        EXPECT_CALL(*unistd_mock_, unlink(StrEq(kLockFilePath.data())))
            .WillOnce(InvokeWithoutArgs([&unlink_called]() -> score::cpp::expected_blank<score::os::Error> {
                unlink_called = true;
                return score::cpp::blank{};
            }));

        // When we successfully CreateOrOpen a LockFile
        const bool take_ownership{false};
        auto lock_file_result = LockFile::CreateOrOpen(kLockFilePath, take_ownership);
        ASSERT_TRUE(lock_file_result.has_value());

        // and TakeOwnerhips of the file
        lock_file_result.value().TakeOwnership();

        EXPECT_FALSE(close_called);
        EXPECT_FALSE(unlink_called);
    }
    EXPECT_TRUE(close_called);
    EXPECT_TRUE(unlink_called);
}

using LockFileOpenFixture = LockFileTestFixture;
TEST_F(LockFileOpenFixture, LockFileOpenShouldCallFcntlOpenWithReadFlagOnly)
{
    const auto opening_flags = Open::kReadOnly;

    // Expect that open is called on the file without the exclusive flag set
    EXPECT_CALL(*fcntl_mock_, open(StrEq(kLockFilePath.data()), opening_flags, _));

    // When we Open a LockFile
    const auto lock_file_result = LockFile::Open(kLockFilePath);
}

TEST_F(LockFileOpenFixture, LockFileOpenShouldReturnEmptyIfOpenFails)
{
    // Expecting that open is called on the file and returns an error
    EXPECT_CALL(*fcntl_mock_, open(StrEq(kLockFilePath.data()), _, _))
        .WillOnce(Return(score::cpp::make_unexpected(os::Error::createFromErrno(ENOENT))));

    // when we Open a LockFile
    const auto lock_file_result = LockFile::Open(kLockFilePath);

    // Then an empty result is returned
    ASSERT_FALSE(lock_file_result.has_value());
}

TEST_F(LockFileOpenFixture, LockFileOpenDoesNotCloseAndUnlinkFileIfOpenFails)
{
    // Expecting that open is called on the file and returns an error
    EXPECT_CALL(*fcntl_mock_, open(StrEq(kLockFilePath.data()), _, _))
        .WillOnce(Return(score::cpp::make_unexpected(os::Error::createFromErrno(ENOENT))));

    // Then close and unlink is never called
    EXPECT_CALL(*unistd_mock_, close(_)).Times(0);
    EXPECT_CALL(*unistd_mock_, unlink(_)).Times(0);

    // When we Open a LockFile
    score::cpp::ignore = LockFile::Open(kLockFilePath);
}

TEST_F(LockFileOpenFixture, IfLockFileOpenIsCalledThenLockFileShouldBeClosedButNotUnlinkedOnDestruction)
{
    const auto opening_flags = Open::kReadOnly;
    bool close_called{false};
    {
        // Expect that open is called on the file
        EXPECT_CALL(*fcntl_mock_, open(StrEq(kLockFilePath.data()), opening_flags, _))
            .WillOnce(Return(kLockFileDescriptor));

        // and the file is closed and unlinked
        EXPECT_CALL(*unistd_mock_, close(kLockFileDescriptor))
            .WillOnce(InvokeWithoutArgs([&close_called]() -> score::cpp::expected_blank<score::os::Error> {
                close_called = true;
                return score::cpp::blank{};
            }));
        EXPECT_CALL(*unistd_mock_, unlink(StrEq(kLockFilePath.data()))).Times(0);

        // When we successfully Open a LockFile
        auto lock_file_result = LockFile::Open(kLockFilePath);
        ASSERT_TRUE(lock_file_result.has_value());

        EXPECT_FALSE(close_called);
    }
    EXPECT_TRUE(close_called);
}

TEST_F(LockFileOpenFixture, IfLockFileOpenAndTakeOwnershipIsCalledThenLockFileShouldBeClosedAndUnlinkedOnDestruction)
{
    const auto exclusive_opening_flags = Open::kReadOnly;
    bool close_called{false};
    bool unlink_called{false};
    {
        // Expect that open is called on the file
        EXPECT_CALL(*fcntl_mock_, open(StrEq(kLockFilePath.data()), exclusive_opening_flags, _))
            .WillOnce(Return(kLockFileDescriptor));

        // and the file is closed and unlinked
        EXPECT_CALL(*unistd_mock_, close(kLockFileDescriptor))
            .WillOnce(InvokeWithoutArgs([&close_called]() -> score::cpp::expected_blank<score::os::Error> {
                close_called = true;
                return score::cpp::blank{};
            }));
        EXPECT_CALL(*unistd_mock_, unlink(StrEq(kLockFilePath.data())))
            .WillOnce(InvokeWithoutArgs([&unlink_called]() -> score::cpp::expected_blank<score::os::Error> {
                unlink_called = true;
                return score::cpp::blank{};
            }));

        // When we successfully Open a LockFile
        auto lock_file_result = LockFile::Open(kLockFilePath);
        ASSERT_TRUE(lock_file_result.has_value());

        // and TakeOwnerhips of the file
        lock_file_result.value().TakeOwnership();

        EXPECT_FALSE(close_called);
        EXPECT_FALSE(unlink_called);
    }
    EXPECT_TRUE(close_called);
    EXPECT_TRUE(unlink_called);
}

using LockFileMoveFixture = LockFileTestFixture;
TEST_F(LockFileMoveFixture, LockFileShouldNotRemoveFileOnMoveConstruction)
{
    bool close_called{false};
    bool unlink_called{false};

    {
        // Expect that open is called on the file
        EXPECT_CALL(*fcntl_mock_, open(StrEq(kLockFilePath.data()), _, _)).WillOnce(Return(kLockFileDescriptor));

        // and the file is closed and unlinked
        EXPECT_CALL(*unistd_mock_, close(kLockFileDescriptor))
            .WillOnce(InvokeWithoutArgs([&close_called]() -> score::cpp::expected_blank<score::os::Error> {
                close_called = true;
                return score::cpp::blank{};
            }));
        EXPECT_CALL(*unistd_mock_, unlink(StrEq(kLockFilePath.data())))
            .WillOnce(InvokeWithoutArgs([&unlink_called]() -> score::cpp::expected_blank<score::os::Error> {
                unlink_called = true;
                return score::cpp::blank{};
            }));

        // When we successfully Create a LockFile
        auto lock_file_result = LockFile::Create(kLockFilePath);
        ASSERT_TRUE(lock_file_result.has_value());

        // and call the move constructor on a new LockFile
        LockFile lock_file_2{std::move(lock_file_result.value())};

        // Then the lock file won't be destroyed
        EXPECT_FALSE(close_called);
        EXPECT_FALSE(unlink_called);
    }
    // Until the new LockFile goes out of scope
    EXPECT_TRUE(close_called);
    EXPECT_TRUE(unlink_called);
}

TEST_F(LockFileMoveFixture, LockFileShouldRemoveFileOnMoveAssignment)
{
    const std::string lock_file_path_2{"/test_lock_file_2"};
    const std::int32_t lock_file_descriptor_2{5678};

    int close_called{false};
    int close_called_2{false};
    int unlink_called{false};
    int unlink_called_2{false};

    {
        // Expect that open is called on both files
        EXPECT_CALL(*fcntl_mock_, open(StrEq(kLockFilePath.data()), _, _)).WillOnce(Return(kLockFileDescriptor));
        EXPECT_CALL(*fcntl_mock_, open(StrEq(lock_file_path_2.data()), _, _)).WillOnce(Return(lock_file_descriptor_2));

        // and both files are closed and unlinked
        EXPECT_CALL(*unistd_mock_, close(kLockFileDescriptor))
            .WillOnce(InvokeWithoutArgs([&close_called]() -> score::cpp::expected_blank<score::os::Error> {
                close_called = true;
                return score::cpp::blank{};
            }));
        EXPECT_CALL(*unistd_mock_, unlink(StrEq(kLockFilePath.data())))
            .WillOnce(InvokeWithoutArgs([&unlink_called]() -> score::cpp::expected_blank<score::os::Error> {
                unlink_called = true;
                return score::cpp::blank{};
            }));
        EXPECT_CALL(*unistd_mock_, close(lock_file_descriptor_2))
            .WillOnce(InvokeWithoutArgs([&close_called_2]() -> score::cpp::expected_blank<score::os::Error> {
                close_called_2 = true;
                return score::cpp::blank{};
            }));
        EXPECT_CALL(*unistd_mock_, unlink(StrEq(lock_file_path_2.data())))
            .WillOnce(InvokeWithoutArgs([&unlink_called_2]() -> score::cpp::expected_blank<score::os::Error> {
                unlink_called_2 = true;
                return score::cpp::blank{};
            }));

        // When we successfully Create 2 LockFiles
        auto lock_file_result = LockFile::Create(kLockFilePath);
        ASSERT_TRUE(lock_file_result.has_value());

        auto lock_file_result_2 = LockFile::Create(lock_file_path_2);
        ASSERT_TRUE(lock_file_result_2.has_value());

        // And move assign the second to the first
        lock_file_result.value() = std::move(lock_file_result_2.value());

        // Then the first lock file should be immediately closed and unlinked
        EXPECT_TRUE(close_called);
        EXPECT_TRUE(unlink_called);
    }
    // And the second should be closed and unlinked when it goes out of scope
    EXPECT_TRUE(close_called_2);
    EXPECT_TRUE(unlink_called_2);
}

TEST_F(LockFileMoveFixture, LockFileShouldNotRemoveFileWhenMoveAssigningToItself)
{
    bool close_called{false};
    bool unlink_called{false};

    {
        // Expect that open is called on the file
        EXPECT_CALL(*fcntl_mock_, open(StrEq(kLockFilePath.data()), _, _)).WillOnce(Return(kLockFileDescriptor));

        // and the file is closed and unlinked
        EXPECT_CALL(*unistd_mock_, close(kLockFileDescriptor))
            .WillOnce(InvokeWithoutArgs([&close_called]() -> score::cpp::expected_blank<score::os::Error> {
                close_called = true;
                return score::cpp::blank{};
            }));
        EXPECT_CALL(*unistd_mock_, unlink(StrEq(kLockFilePath.data())))
            .WillOnce(InvokeWithoutArgs([&unlink_called]() -> score::cpp::expected_blank<score::os::Error> {
                unlink_called = true;
                return score::cpp::blank{};
            }));

        // Given a valid LockFile
        auto lock_file_result = LockFile::Create(kLockFilePath);
        ASSERT_TRUE(lock_file_result.has_value());

        // When we move assign the lock file to itself
        lock_file_result.value() = std::move(lock_file_result.value());

        // Then the lock file won't be destroyed
        EXPECT_FALSE(close_called);
        EXPECT_FALSE(unlink_called);
    }
    // Until the new LockFile goes out of scope
    EXPECT_TRUE(close_called);
    EXPECT_TRUE(unlink_called);
}

}  // namespace
}  // namespace score::memory::shared
