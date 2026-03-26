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
#include "score/memory/shared/flock/exclusive_flock_mutex.h"
#include "score/memory/shared/flock/shared_flock_mutex.h"
#include "score/memory/shared/lock_file.h"

#include "score/os/fcntl.h"
#include "score/os/mocklib/fcntl_mock.h"
#include "score/os/mocklib/stat_mock.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mutex>

namespace score::memory::shared
{
namespace
{

using Mode = ::score::os::Stat::Mode;
using Open = ::score::os::Fcntl::Open;

using ::testing::_;
using ::testing::Return;

#if defined(__QNX__)
// In QNX /tmp is just an "alias" for /dev/shmem/, but using the alias
// has a performance drawback! Therefore we use directly /dev/shmem/ (see Ticket-131757)
constexpr auto kLockFilePath{"/dev/shmem/flock_test_lock_file"};
#else
constexpr auto kLockFilePath{"/tmp/flock_test_lock_file"};
#endif

score::os::Fcntl::Operation GetBlockingLockOperation(const ExclusiveFlockMutex&) noexcept
{
    return os::Fcntl::Operation::kLockExclusive;
}

score::os::Fcntl::Operation GetBlockingLockOperation(const SharedFlockMutex&) noexcept
{
    return os::Fcntl::Operation::kLockShared;
}

score::os::Fcntl::Operation GetNonBlockingLockOperation(const ExclusiveFlockMutex&) noexcept
{
    return os::Fcntl::Operation::kLockExclusive | score::os::Fcntl::Operation::kLockNB;
}

score::os::Fcntl::Operation GetNonBlockingLockOperation(const SharedFlockMutex&) noexcept
{
    return os::Fcntl::Operation::kLockShared | score::os::Fcntl::Operation::kLockNB;
}

template <typename T>
class FlockTestFixture : public ::testing::Test
{
  public:
    FlockTestFixture()
    {
        ON_CALL(*fcntl_mock_, open(testing::StrEq(kLockFilePath), _, _)).WillByDefault(Return(1));
        ON_CALL(*stat_mock_, chmod(testing::StrEq(kLockFilePath), _)).WillByDefault(Return(score::cpp::blank{}));
    }
    os::MockGuard<os::FcntlMock> fcntl_mock_{};
    os::MockGuard<os::StatMock> stat_mock_{};
    LockFile lock_file_{std::move(LockFile::Create(kLockFilePath).value())};
    T flock_mutex_{lock_file_};
};

// Gtest will run all tests in the FlockLockTestFixture once for every type, t, in MyTypes, such that TypeParam
// == t for each run.
using MyTypes = ::testing::Types<ExclusiveFlockMutex, SharedFlockMutex>;
TYPED_TEST_SUITE(FlockTestFixture, MyTypes, );

TYPED_TEST(FlockTestFixture, LockWillReturnWhenFlockSucceeds)
{
    const auto blocking_lock_operation = GetBlockingLockOperation(this->flock_mutex_);
    EXPECT_CALL(*this->fcntl_mock_, flock(_, blocking_lock_operation)).WillOnce(Return(score::cpp::blank{}));
    this->flock_mutex_.lock();
}

TYPED_TEST(FlockTestFixture, LockWillTerminateWhenFlockFails)
{
    const auto blocking_lock_operation = GetBlockingLockOperation(this->flock_mutex_);

    auto lock_failure = [this, blocking_lock_operation] {
        EXPECT_CALL(*this->fcntl_mock_, flock(_, blocking_lock_operation))
            .WillOnce(Return(score::cpp::unexpected<::score::os::Error>{::score::os::Error::createFromErrno(ENOENT)}));
        this->flock_mutex_.lock();
    };
    EXPECT_DEATH(lock_failure(), ".*");
}

TYPED_TEST(FlockTestFixture, TryLockWillReturnTrueWhenFlockSucceeds)
{
    const auto non_blocking_lock_operation = GetNonBlockingLockOperation(this->flock_mutex_);
    EXPECT_CALL(*this->fcntl_mock_, flock(_, non_blocking_lock_operation)).WillOnce(Return(score::cpp::blank{}));
    EXPECT_TRUE(this->flock_mutex_.try_lock());
}

TYPED_TEST(FlockTestFixture, TryLockWillReturnFalseWhenFlockIsAlreadyBlocked)
{
    const auto non_blocking_lock_operation = GetNonBlockingLockOperation(this->flock_mutex_);
    EXPECT_CALL(*this->fcntl_mock_, flock(_, non_blocking_lock_operation))
        .WillOnce(Return(score::cpp::unexpected<::score::os::Error>{::score::os::Error::createFromErrno(EWOULDBLOCK)}));
    EXPECT_FALSE(this->flock_mutex_.try_lock());
}

TYPED_TEST(FlockTestFixture, TryLockWillTerminateWhenFlockFails)
{
    const auto non_blocking_lock_operation = GetNonBlockingLockOperation(this->flock_mutex_);

    auto try_lock_failure = [this, non_blocking_lock_operation] {
        EXPECT_CALL(*this->fcntl_mock_, flock(_, non_blocking_lock_operation))
            .WillOnce(Return(score::cpp::unexpected<::score::os::Error>{::score::os::Error::createFromErrno(ENOENT)}));
        this->flock_mutex_.try_lock();
    };
    EXPECT_DEATH(try_lock_failure(), ".*");
}

TYPED_TEST(FlockTestFixture, UnlockWillReturnWhenFlockSucceeds)
{
    const auto unlock_operation = score::os::Fcntl::Operation::kUnLock;
    EXPECT_CALL(*this->fcntl_mock_, flock(_, unlock_operation)).WillOnce(Return(score::cpp::blank{}));
    this->flock_mutex_.unlock();
}

TYPED_TEST(FlockTestFixture, UnlockWillTerminateWhenFlockFails)
{
    const auto unlock_operation = score::os::Fcntl::Operation::kUnLock;

    auto unlock_failure = [this] {
        EXPECT_CALL(*this->fcntl_mock_, flock(_, unlock_operation))
            .WillOnce(Return(score::cpp::unexpected<::score::os::Error>{::score::os::Error::createFromErrno(ENOENT)}));
        this->flock_mutex_.unlock();
    };
    EXPECT_DEATH(unlock_failure(), ".*");
}

TYPED_TEST(FlockTestFixture, CanCreateLockGuardFromFlockMutex)
{
    const auto blocking_lock_operation = GetBlockingLockOperation(this->flock_mutex_);
    EXPECT_CALL(*this->fcntl_mock_, flock(_, blocking_lock_operation)).WillOnce(Return(score::cpp::blank{}));

    const auto unlock_operation = score::os::Fcntl::Operation::kUnLock;
    EXPECT_CALL(*this->fcntl_mock_, flock(_, unlock_operation)).WillOnce(Return(score::cpp::blank{}));

    std::lock_guard lock{this->flock_mutex_};
}

TYPED_TEST(FlockTestFixture, CanCreateUniqueLockFromFlockMutex)
{
    const auto blocking_lock_operation = GetBlockingLockOperation(this->flock_mutex_);
    EXPECT_CALL(*this->fcntl_mock_, flock(_, blocking_lock_operation)).WillOnce(Return(score::cpp::blank{}));

    const auto unlock_operation = score::os::Fcntl::Operation::kUnLock;
    EXPECT_CALL(*this->fcntl_mock_, flock(_, unlock_operation)).WillOnce(Return(score::cpp::blank{}));

    std::unique_lock lock{this->flock_mutex_};
}

TYPED_TEST(FlockTestFixture, CanCreateUniqueLockFromFlockMutexAndTryLockWillReturnFlockTryLockSuccessfulResult)
{
    const auto non_blocking_lock_operation = GetNonBlockingLockOperation(this->flock_mutex_);
    EXPECT_CALL(*this->fcntl_mock_, flock(_, non_blocking_lock_operation)).WillOnce(Return(score::cpp::blank{}));

    const auto unlock_operation = score::os::Fcntl::Operation::kUnLock;
    EXPECT_CALL(*this->fcntl_mock_, flock(_, unlock_operation)).WillOnce(Return(score::cpp::blank{}));

    std::unique_lock lock{this->flock_mutex_, std::defer_lock};
    EXPECT_TRUE(lock.try_lock());
}
TYPED_TEST(FlockTestFixture, CanCreateUniqueLockFromFlockMutexAndTryLockWillReturnFlockTryLockFailureResult)
{
    const auto non_blocking_lock_operation = GetNonBlockingLockOperation(this->flock_mutex_);
    EXPECT_CALL(*this->fcntl_mock_, flock(_, non_blocking_lock_operation))
        .WillOnce(Return(score::cpp::unexpected<::score::os::Error>{::score::os::Error::createFromErrno(EWOULDBLOCK)}));

    std::unique_lock lock{this->flock_mutex_, std::defer_lock};
    EXPECT_FALSE(lock.try_lock());
}

}  // namespace
}  // namespace score::memory::shared
