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
#include "score/memory/shared/sealedshm/sealedshm_wrapper/sealed_shm_mock.h"
#if defined __QNX__
#include "score/os/mocklib/qnx/mock_mman.h"
#endif
#include "score/os/stat.h"

#include <fcntl.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::_;
using ::testing::Mock;
using ::testing::Return;
using ::testing::StrictMock;

namespace score::memory::shared::test
{

#if defined __QNX__
constexpr auto kFileDescriptor = 42;
constexpr auto kSize = 1682;
#endif

class SealedShmTestAttorney
{
  public:
#if defined __QNX__
    static score::memory::shared::SealedShm CreateInstance(std::unique_ptr<score::os::qnx::MmanQnx> mman) noexcept
    {
        return score::memory::shared::SealedShm{std::move(mman)};
    }
#else
    static score::memory::shared::SealedShm CreateInstance() noexcept
    {
        return score::memory::shared::SealedShm{};
    }
#endif
};

class SealedShmQnxTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
#if defined __QNX__
        mman_mock_ = std::make_unique<StrictMock<score::os::qnx::MmanQnxMock>>();
        mman_mock_raw_ptr_ = mman_mock_.get();
#endif
        // Make sure that the mock is cleared before each test
        score::memory::shared::SealedShm::InjectMock(nullptr);
    };

#if defined __QNX__
    void TearDown() override
    {
        ASSERT_TRUE(Mock::VerifyAndClearExpectations(mman_mock_raw_ptr_));
    };

    std::unique_ptr<StrictMock<score::os::qnx::MmanQnxMock>> mman_mock_;
    StrictMock<score::os::qnx::MmanQnxMock>* mman_mock_raw_ptr_;
#endif
};

TEST_F(SealedShmQnxTest, OpenAnonymous_Success)
{
    const auto mode = score::os::ModeToInteger(score::os::Stat::Mode::kReadUser | score::os::Stat::Mode::kWriteUser);
#if defined __QNX__
    auto sealed_shm = SealedShmTestAttorney::CreateInstance(std::move(mman_mock_));

    EXPECT_CALL(*mman_mock_raw_ptr_, shm_open(SHM_ANON, _, mode)).WillOnce(Return(kFileDescriptor));

    const auto result = sealed_shm.OpenAnonymous(mode);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), kFileDescriptor);
#else
    auto sealed_shm = SealedShmTestAttorney::CreateInstance();
    const auto result = sealed_shm.OpenAnonymous(mode);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), score::os::Error::createFromErrno(ENOTSUP));
#endif
}

TEST_F(SealedShmQnxTest, Seal_Success)
{
#if defined __QNX__
    const std::int32_t shm_ctl_success{0};
    auto sealed_shm = SealedShmTestAttorney::CreateInstance(std::move(mman_mock_));

    EXPECT_CALL(*mman_mock_raw_ptr_, shm_ctl(kFileDescriptor, _, 0, kSize)).WillOnce(Return(shm_ctl_success));

    const auto result = sealed_shm.Seal(kFileDescriptor, kSize);

    EXPECT_TRUE(result.has_value());
#else
    GTEST_SKIP();
#endif
}

TEST_F(SealedShmQnxTest, Seal_Failed)
{
#if defined __QNX__
    const auto shm_ctl_failure = score::cpp::make_unexpected(score::os::Error::createFromErrno(EFAULT));
    auto sealed_shm = SealedShmTestAttorney::CreateInstance(std::move(mman_mock_));

    EXPECT_CALL(*mman_mock_raw_ptr_, shm_ctl(kFileDescriptor, _, 0, kSize)).WillOnce(Return(shm_ctl_failure));

    const auto result = sealed_shm.Seal(kFileDescriptor, kSize);

    EXPECT_FALSE(result.has_value());
#else
    auto sealed_shm = SealedShmTestAttorney::CreateInstance();

    const auto result = sealed_shm.Seal(0, 0);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), score::os::Error::createFromErrno(ENOTSUP));
#endif
}

TEST_F(SealedShmQnxTest, TestInstanceReturnsSameInstance)
{
    auto& instance = score::memory::shared::SealedShm::instance();
    auto& instance2 = score::memory::shared::SealedShm::instance();

    EXPECT_EQ(&instance, &instance2);
}

TEST_F(SealedShmQnxTest, TestInstanceReturnsMockInstance)
{
    auto mock_ptr = std::make_unique<score::memory::shared::SealedShmMock>();
    score::memory::shared::SealedShm::InjectMock(mock_ptr.get());

    auto& instance = score::memory::shared::SealedShm::instance();

    EXPECT_EQ(&instance, mock_ptr.get());
}

}  // namespace score::memory::shared::test
