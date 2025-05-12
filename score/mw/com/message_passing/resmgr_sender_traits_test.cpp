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
#include "score/mw/com/message_passing/qnx/resmgr_sender_traits.h"

#include "score/mw/com/message_passing/message.h"
#include "score/mw/com/message_passing/qnx/resmgr_traits_common.h"

#include "score/os/mocklib/fcntl_mock.h"
#include "score/os/mocklib/unistdmock.h"

#include <gtest/gtest.h>

namespace score
{
namespace mw
{
namespace com
{
namespace message_passing
{
namespace
{

using ::testing::_;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::StrictMock;

class ResmgrSenderFixture : public ::testing::Test
{
  public:
    using StrictFcntlMock = StrictMock<score::os::FcntlMock>;
    using StrictUnistdMock = StrictMock<score::os::UnistdMock>;

    void SetUp() override
    {
        memory_resource_ = score::cpp::pmr::get_default_resource();
        auto fcntl_mock = score::cpp::pmr::make_unique<StrictFcntlMock>(memory_resource_);
        auto unistd_mock = score::cpp::pmr::make_unique<StrictUnistdMock>(memory_resource_);

        raw_fcntl_mock_ = fcntl_mock.get();
        raw_unistd_mock_ = unistd_mock.get();

        os_resources_.fcntl = std::move(fcntl_mock);
        os_resources_.unistd = std::move(unistd_mock);
    }

    void TearDown() override {}

    ResmgrSenderTraits::FileDescriptorResourcesType os_resources_;
    StrictFcntlMock* raw_fcntl_mock_ = nullptr;
    StrictUnistdMock* raw_unistd_mock_ = nullptr;
    score::cpp::pmr::memory_resource* memory_resource_ = nullptr;
};

TEST_F(ResmgrSenderFixture, OSResourcesFailures)
{
    ResmgrSenderTraits::FileDescriptorResourcesType null_resources{};
    EXPECT_EXIT(ResmgrSenderTraits::try_open({}, null_resources), ::testing::KilledBySignal(SIGABRT), "");
    const ShortMessage short_message{};
    auto&& short_message_result = ResmgrSenderTraits::prepare_payload(short_message);
    EXPECT_EXIT(
        ResmgrSenderTraits::try_send({}, short_message_result, null_resources), ::testing::KilledBySignal(SIGABRT), "");
    EXPECT_EXIT(ResmgrSenderTraits::close_sender({}, null_resources), ::testing::KilledBySignal(SIGABRT), "");
}

TEST_F(ResmgrSenderFixture, NormalFlow)
{
    EXPECT_FALSE(ResmgrSenderTraits::has_non_blocking_guarantee());

    std::string identifier{"/whatever"};
    QnxResourcePath path{identifier};

    std::string expected_path = std::string{GetQnxPrefix().data(), GetQnxPrefix().size()} + identifier;
    EXPECT_STREQ(path.c_str(), expected_path.c_str());

    EXPECT_CALL(*raw_fcntl_mock_,
                open(StrEq(path.c_str()), score::os::Fcntl::Open::kWriteOnly | score::os::Fcntl::Open::kCloseOnExec))
        .WillOnce(Return(ResmgrSenderTraits::INVALID_FILE_DESCRIPTOR));
    const auto open_result = ResmgrSenderTraits::try_open(identifier, os_resources_);
    ASSERT_TRUE(open_result);
    const auto file_descriptor = open_result.value();

    const ShortMessage short_message{};
    auto&& short_message_result = ResmgrSenderTraits::prepare_payload(short_message);
    const MediumMessage medium_message{};
    auto&& medium_message_result = ResmgrSenderTraits::prepare_payload(medium_message);
    ASSERT_EQ(&short_message, &short_message_result);
    ASSERT_EQ(&medium_message, &medium_message_result);

    EXPECT_CALL(*raw_unistd_mock_, write(-1, &short_message_result, sizeof(short_message_result)))
        .WillOnce(Return(sizeof(short_message_result)));
    EXPECT_CALL(*raw_unistd_mock_, write(-1, &medium_message_result, sizeof(medium_message_result)))
        .WillOnce(Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(EOF))));
    auto short_send_result = ResmgrSenderTraits::try_send(file_descriptor, short_message_result, os_resources_);
    auto medium_send_result = ResmgrSenderTraits::try_send(file_descriptor, medium_message_result, os_resources_);
    ASSERT_TRUE(short_send_result);
    ASSERT_FALSE(medium_send_result);

    EXPECT_CALL(*raw_unistd_mock_, close(-1));
    ResmgrSenderTraits::close_sender(file_descriptor, os_resources_);
}

}  // namespace
}  // namespace message_passing
}  // namespace com
}  // namespace mw
}  // namespace score
