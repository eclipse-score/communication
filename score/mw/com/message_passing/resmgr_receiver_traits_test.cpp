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
#include "score/mw/com/message_passing/qnx/resmgr_receiver_traits.h"

#include "score/mw/com/message_passing/qnx/resmgr_traits_common.h"

#include "score/os/mocklib/qnx/mock_channel.h"
#include "score/os/mocklib/qnx/mock_dispatch.h"
#include "score/os/mocklib/qnx/mock_iofunc.h"
#include "score/os/mocklib/unistdmock.h"

#include "score/os/unistd.h"

#include <gtest/gtest.h>

namespace score::mw::com::message_passing
{
namespace
{

using namespace ::testing;

class ResmgrReceiverFixtureBase : public ::testing::Test
{
  public:
    using StrictChannelMock = StrictMock<score::os::MockChannel>;
    using StrictDispatchMock = StrictMock<score::os::MockDispatch>;
    using StrictIoFuncMock = StrictMock<score::os::MockIoFunc>;

    void CustomSetUp(const score::cpp::pmr::vector<uid_t>& allowed_uids, bool defensive_test)
    {
        memory_resource_ = score::cpp::pmr::get_default_resource();
        auto channel_mock = score::cpp::pmr::make_unique<StrictChannelMock>(memory_resource_);
        auto dispatch_mock = score::cpp::pmr::make_unique<StrictDispatchMock>(memory_resource_);
        auto iofunc_mock = score::cpp::pmr::make_unique<StrictIoFuncMock>(memory_resource_);
        auto unistd_mock = score::cpp::pmr::make_unique<score::os::UnistdMock>(memory_resource_);

        raw_channel_mock_ = channel_mock.get();
        raw_dispatch_mock_ = dispatch_mock.get();
        raw_iofunc_mock_ = iofunc_mock.get();
        raw_unistd_mock_ = unistd_mock.get();

        os_resources_.channel = std::move(channel_mock);
        os_resources_.dispatch = std::move(dispatch_mock);
        os_resources_.iofunc = std::move(iofunc_mock);
        os_resources_.unistd = std::move(unistd_mock);

        EXPECT_CALL(*raw_iofunc_mock_, iofunc_func_init(_RESMGR_CONNECT_NFUNCS, _, _RESMGR_IO_NFUNCS, _))
            .Times(AnyNumber())
            .WillRepeatedly([](const std::uint32_t,
                               resmgr_connect_funcs_t* const connect_funcs,
                               const std::uint32_t,
                               resmgr_io_funcs_t* const) {
                connect_funcs->open = open_default;
            });
        EXPECT_CALL(*raw_iofunc_mock_, iofunc_attr_init(_, kAttrMode, kNoAttr, kNoClientInfo)).Times(AnyNumber());

        EXPECT_CALL(*raw_dispatch_mock_, dispatch_create_channel(-1, DISPATCH_FLAG_NOLOCK))
            .WillRepeatedly(Return(kDispatchPointer));

        EXPECT_CALL(*raw_dispatch_mock_,
                    resmgr_attach(kDispatchPointer, _, StrEq(kQnxPath.c_str()), _, _RESMGR_FLAG_SELF, _, _, _))
            .WillRepeatedly([this](dispatch_t* const,
                                   resmgr_attr_t* const,
                                   const char* const,
                                   const enum _file_type,
                                   const std::uint32_t,
                                   const resmgr_connect_funcs_t* const connect_funcs,
                                   const resmgr_io_funcs_t* const io_funcs,
                                   RESMGR_HANDLE_T* const) {
                io_open_ = connect_funcs->open;
                io_write_ = io_funcs->write;
                return kDispatchId;
            });

        // expected for internal stop_token channel setup
        EXPECT_CALL(*raw_dispatch_mock_, message_attach)
            .WillRepeatedly(DoAll(SaveArg<4>(&message_handler_), Return(score::cpp::blank{})));
        EXPECT_CALL(*raw_dispatch_mock_, message_connect).WillRepeatedly(Return(kSideChannelCoid));

        EXPECT_CALL(*raw_dispatch_mock_, dispatch_context_alloc)
            .Times(ResmgrReceiverTraits::kConcurrency)
            .WillRepeatedly([this](dispatch_t*) {
                dispatch_contexts_[dispatch_contexts_count_].resmgr_context.dpp = kDispatchPointer;
                dispatch_contexts_[dispatch_contexts_count_].resmgr_context.id = kDispatchId;
                dispatch_contexts_[dispatch_contexts_count_].resmgr_context.extra = &extra_[dispatch_contexts_count_];
                return &dispatch_contexts_[dispatch_contexts_count_++];
            });

        if (defensive_test)
        {
            auto test_error = score::cpp::make_unexpected(score::os::Error::createFromErrno(EINVAL));

            // these expectations will actually be consumed first
            EXPECT_CALL(*raw_dispatch_mock_, dispatch_create_channel)
                .WillOnce(Return(test_error))
                .RetiresOnSaturation();
            EXPECT_CALL(*raw_dispatch_mock_, resmgr_attach).WillOnce(Return(test_error)).RetiresOnSaturation();
            EXPECT_CALL(*raw_dispatch_mock_, message_attach).WillOnce(Return(test_error)).RetiresOnSaturation();
            EXPECT_CALL(*raw_dispatch_mock_, message_connect).WillOnce(Return(test_error)).RetiresOnSaturation();
            EXPECT_CALL(*raw_dispatch_mock_, dispatch_context_alloc).WillOnce(Return(test_error)).RetiresOnSaturation();
        }
        else
        {
            SetUpReceiver(allowed_uids);
        }
    }

    void TearDown() override
    {
        EXPECT_CALL(*raw_channel_mock_, ConnectDetach(kSideChannelCoid));
        EXPECT_CALL(*raw_dispatch_mock_, resmgr_detach(kDispatchPointer, kDispatchId, _RESMGR_DETACH_CLOSE));
        EXPECT_CALL(*raw_dispatch_mock_, dispatch_destroy(kDispatchPointer));
        EXPECT_CALL(*raw_dispatch_mock_, dispatch_context_free).Times(ResmgrReceiverTraits::kConcurrency);

        ResmgrReceiverTraits::close_receiver(fd_, kIdentifier, os_resources_);
    }

    void SetUpReceiver(const score::cpp::pmr::vector<uid_t>& allowed_uids)
    {
        if (fd_ == ResmgrReceiverTraits::INVALID_FILE_DESCRIPTOR)
        {
            auto open_result = ResmgrReceiverTraits::open_receiver(
                kIdentifier, allowed_uids, kMaxNumberMessagesInQueue, os_resources_);
            EXPECT_TRUE(open_result);
            ASSERT_NE(io_open_, nullptr);
            ASSERT_NE(io_write_, nullptr);
            ASSERT_NE(message_handler_, nullptr);

            fd_ = open_result.value();
            ASSERT_NE(fd_, ResmgrReceiverTraits::INVALID_FILE_DESCRIPTOR);
        }
    }

    void ExpectOpenRequest(uid_t client_uid, int open_result)
    {
        EXPECT_CALL(*raw_dispatch_mock_, dispatch_block(&dispatch_contexts_[kThreadId]))
            .WillOnce(Return(score::cpp::blank{}));
        EXPECT_CALL(*raw_dispatch_mock_, dispatch_handler(&dispatch_contexts_[kThreadId]))
            .WillOnce([this, open_result](auto&& ctp) -> score::cpp::expected_blank<std::int32_t> {
                // prepare arguments for "opening" a channel
                ctp->resmgr_context.info.scoid = kScoid;
                io_open_t msg{};
                void* const extra = static_cast<void*>(static_cast<ResmgrReceiverFixtureBase*>(this));
                EXPECT_EQ((*io_open_)(&ctp->resmgr_context, &msg, nullptr, extra), open_result);
                return {};
            });
        EXPECT_CALL(*raw_channel_mock_, ConnectClientInfo(kScoid, _, 0))
            .Times(AtMost(1))
            .WillOnce([client_uid](std::int32_t,
                                   _client_info* info,
                                   std::int32_t) -> score::cpp::expected_blank<score::os::Error> {
                if (client_uid == kUidFailInfo)
                {
                    return score::cpp::make_unexpected(score::os::Error::createFromErrno(EINVAL));
                }
                info->cred.euid = client_uid;
                return {};
            });
    }

    void ExpectWriteRequests(std::size_t size)
    {
        EXPECT_CALL(*raw_dispatch_mock_, dispatch_block(_)).WillRepeatedly(Return(score::cpp::blank{}));
        EXPECT_CALL(*raw_dispatch_mock_, dispatch_handler(_))
            .WillRepeatedly([this, size](auto&& ctp) -> score::cpp::expected_blank<std::int32_t> {
                // prepare context for "receiving" a "message"
                ctp->resmgr_context.info.pid = 0;
                ctp->resmgr_context.info.srcmsglen = size + sizeof(io_write_t);
                ctp->resmgr_context.offset = 0;
                io_write_t msg{};
                msg.i.xtype = _IO_XTYPE_NONE;
                msg.i.nbytes = size;
                RESMGR_OCB_T ocb{};
                std::int32_t result = (*io_write_)(&ctp->resmgr_context, &msg, &ocb);
                if (result == EOK)
                {
                    return {};
                }
                else if (result == ENOMEM)
                {
                    ++rejected_message_count_;
                    return {};
                }
                else
                {
                    return score::cpp::make_unexpected(-1);
                }
            });
        EXPECT_CALL(*raw_iofunc_mock_, iofunc_write_verify).WillRepeatedly(Return(score::cpp::blank()));
        if (size == sizeof(ShortMessage) || size == sizeof(MediumMessage))
        {
            // HACK: no real copy, may trigger sanitizer warnings later
            EXPECT_CALL(*raw_dispatch_mock_, resmgr_msgget).WillRepeatedly(ReturnArg<2>());
        }
    }

    enum class WriteAnomaly
    {
        XType,
        Clipped,
        Pid,
    };

    void ExpectAbnormalWrite(WriteAnomaly anomaly, std::size_t size = sizeof(ShortMessage))
    {
        EXPECT_CALL(*raw_dispatch_mock_, dispatch_handler(_))
            .WillOnce([this, anomaly, size](auto&& ctp) -> score::cpp::expected_blank<std::int32_t> {
                ctp->resmgr_context.info.pid = anomaly == WriteAnomaly::Pid ? 1 : 0;
                ctp->resmgr_context.info.srcmsglen = size + sizeof(io_write_t);
                ctp->resmgr_context.offset = 0;
                io_write_t msg{};
                msg.i.xtype = anomaly == WriteAnomaly::XType ? _IO_XTYPE_OFFSET : _IO_XTYPE_NONE;
                msg.i.nbytes = anomaly == WriteAnomaly::Clipped ? size + 1 : size;
                RESMGR_OCB_T ocb{};
                std::int32_t result = (*io_write_)(&ctp->resmgr_context, &msg, &ocb);
                if (result == EOK)
                {
                    return {};
                }
                else if (result == ENOMEM)
                {
                    ++rejected_message_count_;
                    return {};
                }
                else
                {
                    return score::cpp::make_unexpected(-1);
                }
            })
            .RetiresOnSaturation();
    }

    void ReceiveNextExpectShortMedium(int short_count, int medium_count)
    {
        const auto next_result = ResmgrReceiverTraits::receive_next(
            fd_,
            kThreadId,
            [this](auto&&) {
                ++short_message_count_;
            },
            [this](auto&&) {
                ++medium_message_count_;
            },
            os_resources_);
        ASSERT_TRUE(next_result);
        EXPECT_TRUE(next_result.value());
        EXPECT_EQ(short_message_count_, short_count);
        EXPECT_EQ(medium_message_count_, medium_count);
    }

    void ReceiveNextExpectConnects(int connect_count)
    {
        ReceiveNextExpectShortMedium(0, 0);
        EXPECT_EQ(open_default_count_, connect_count);
    }

    void ReceiveNextExpectStop()
    {
        const auto next_result = ResmgrReceiverTraits::receive_next(
            fd_,
            kThreadId,
            [this](auto&&) {
                ++short_message_count_;
            },
            [this](auto&&) {
                ++medium_message_count_;
            },
            os_resources_);
        ASSERT_TRUE(next_result);
        EXPECT_FALSE(next_result.value());
        EXPECT_EQ(short_message_count_, 0);
        EXPECT_EQ(medium_message_count_, 0);
    }

    // resmgr_connect_funcs_t
    static std::int32_t open_default(resmgr_context_t*, io_open_t*, RESMGR_HANDLE_T*, void* extra)
    {
        ++static_cast<ResmgrReceiverFixtureBase*>(extra)->open_default_count_;
        return EOK;
    }

    ResmgrReceiverTraits::FileDescriptorResourcesType os_resources_;
    score::cpp::pmr::memory_resource* memory_resource_ = nullptr;

    // mocks
    StrictChannelMock* raw_channel_mock_ = nullptr;
    StrictDispatchMock* raw_dispatch_mock_ = nullptr;
    StrictIoFuncMock* raw_iofunc_mock_ = nullptr;
    score::os::UnistdMock* raw_unistd_mock_ = nullptr;

    // consts
    const std::string_view kIdentifier{"/whatever"};
    const QnxResourcePath kQnxPath{kIdentifier};
    static constexpr dispatch_t* kDispatchPointer = nullptr;  // incomplete class by design, cannot instantiate
    static constexpr std::uint32_t kDispatchId = 1;
    static constexpr std::uint32_t kMaxNumberMessagesInQueue = 10;
    static constexpr std::uint32_t kSideChannelCoid = 2;
    static constexpr std::uint32_t kSideChannelRcvid = 3;
    static constexpr std::uint32_t kScoid = 1234;
    static constexpr std::uint32_t kThreadId = 0;
    static constexpr std::uint32_t kOtherThreadId = 1;
    static constexpr mode_t kAttrMode = S_IFNAM | 0666;
    static constexpr iofunc_attr_t* kNoAttr = nullptr;
    static constexpr _client_info* kNoClientInfo = nullptr;
    static constexpr uid_t kUidAccept = 1002;
    static constexpr uid_t kUidFailInfo = 1;

    // variables
    _extended_context extra_[ResmgrReceiverTraits::kConcurrency];
    dispatch_context_t dispatch_contexts_[ResmgrReceiverTraits::kConcurrency];
    std::uint32_t dispatch_contexts_count_{0};
    std::int32_t (*io_open_)(resmgr_context_t* ctp, io_open_t* msg, RESMGR_HANDLE_T* handle, void* extra){nullptr};
    std::int32_t (*io_write_)(resmgr_context_t* ctp, io_write_t* msg, RESMGR_OCB_T* ocb){nullptr};
    std::int32_t (*message_handler_)(message_context_t* const ctp,
                                     const std::int32_t code,
                                     const std::uint32_t flags,
                                     void* const handle) noexcept {nullptr};
    ResmgrReceiverTraits::file_descriptor_type fd_{ResmgrReceiverTraits::INVALID_FILE_DESCRIPTOR};
    std::uint32_t open_default_count_{0};
    std::uint32_t short_message_count_{0};
    std::uint32_t medium_message_count_{0};
    std::uint32_t rejected_message_count_{0};
};

class ResmgrReceiverFixture : public ResmgrReceiverFixtureBase
{
  public:
    void SetUp() override
    {
        CustomSetUp(kAllowedUids, false);
    }

    const score::cpp::pmr::vector<uid_t> kAllowedUids = {};
};

class ResmgrReceiverFixtureWithUids : public ResmgrReceiverFixtureBase
{
  public:
    void SetUp() override
    {
        CustomSetUp(kAllowedUids, false);
    }

    const score::cpp::pmr::vector<uid_t> kAllowedUids = {1001, 1002};
    const uid_t kUidReject = 1003;
};

class ResmgrReceiverFixtureDefensive : public ResmgrReceiverFixtureBase
{
  public:
    void SetUp() override
    {
        CustomSetUp(kAllowedUids, true);
    }

    const score::cpp::pmr::vector<uid_t> kAllowedUids = {};
};

TEST_F(ResmgrReceiverFixture, SetupCleanup)
{
    // Empty by design
}

TEST_F(ResmgrReceiverFixture, OSResourcesFailures)
{
    ResmgrReceiverTraits::FileDescriptorResourcesType null_resources{};
    EXPECT_EXIT(
        ResmgrReceiverTraits::open_receiver(kIdentifier, kAllowedUids, kMaxNumberMessagesInQueue, null_resources),
        ::testing::KilledBySignal(SIGABRT),
        "");
    EXPECT_EXIT(ResmgrReceiverTraits::stop_receive(fd_, null_resources), ::testing::KilledBySignal(SIGABRT), "");
    EXPECT_EXIT(
        ResmgrReceiverTraits::close_receiver(nullptr, "", null_resources), ::testing::KilledBySignal(SIGABRT), "");
}

TEST_F(ResmgrReceiverFixture, SendStopMessage)
{
    EXPECT_CALL(*raw_channel_mock_, MsgSend(kSideChannelCoid, _, _, _, _));

    ResmgrReceiverTraits::stop_receive(fd_, os_resources_);
}

TEST_F(ResmgrReceiverFixture, ReceiveNoMessage)
{
    EXPECT_CALL(*raw_dispatch_mock_, dispatch_block(&dispatch_contexts_[kThreadId]))
        .WillOnce(Return(score::cpp::blank{}));
    EXPECT_CALL(*raw_dispatch_mock_, dispatch_handler(&dispatch_contexts_[kThreadId]))
        .WillOnce(Return(score::cpp::blank{}));

    ReceiveNextExpectShortMedium(0, 0);
}

TEST_F(ResmgrReceiverFixture, ReceiveValidStopMessage)
{
    EXPECT_CALL(*raw_dispatch_mock_, dispatch_block(&dispatch_contexts_[kThreadId])).WillOnce([this](auto&& ctp) {
        // prepare context for "receiving" side_channel "message" from our own process
        ctp->resmgr_context.info.pid = os_resources_.unistd->getpid();
        ctp->resmgr_context.rcvid = kSideChannelRcvid;
        (*message_handler_)(&ctp->resmgr_context, 0, 0, nullptr);
        return score::cpp::blank{};
    });
    EXPECT_CALL(*raw_dispatch_mock_, dispatch_handler(&dispatch_contexts_[kThreadId]))
        .WillOnce(Return(score::cpp::blank{}));
    EXPECT_CALL(*raw_channel_mock_, MsgReply(kSideChannelRcvid, _, _, _));
    EXPECT_CALL(*raw_unistd_mock_, getpid()).Times(2).WillRepeatedly(Return(0));

    ReceiveNextExpectStop();
}

TEST_F(ResmgrReceiverFixture, ReceiveConnect)
{
    ExpectOpenRequest(kUidAccept, EOK);

    ReceiveNextExpectConnects(1);
}

TEST_F(ResmgrReceiverFixtureWithUids, ReceiveConnectAccept)
{
    ExpectOpenRequest(kUidAccept, EOK);

    ReceiveNextExpectConnects(1);
}

TEST_F(ResmgrReceiverFixtureWithUids, ReceiveConnectReject)
{
    ExpectOpenRequest(kUidReject, EACCES);

    ReceiveNextExpectConnects(0);
}

TEST_F(ResmgrReceiverFixture, ReceiveShortMessage)
{
    ExpectWriteRequests(sizeof(ShortMessage));

    ReceiveNextExpectShortMedium(1, 0);
}

TEST_F(ResmgrReceiverFixture, ReceiveMediumMessage)
{
    ExpectWriteRequests(sizeof(MediumMessage));

    ReceiveNextExpectShortMedium(0, 1);
}

TEST_F(ResmgrReceiverFixture, ReceiveQueueOverflow)
{
    ExpectWriteRequests(sizeof(ShortMessage));

    const auto blocking_short_message_processor = [this](auto&&) {
        if (short_message_count_++ == 0)
        {
            static_assert(kOtherThreadId < ResmgrReceiverTraits::kConcurrency, "");
            std::uint32_t racing_message_count = 0;
            // simulate another thread receiving messages running in parallel
            // only the first third of these messages shall be queued
            // none shall be processed before we finish
            for (std::uint32_t i = 0; i < 3 * kMaxNumberMessagesInQueue; ++i)
            {
                const auto next_result = ResmgrReceiverTraits::receive_next(
                    fd_,
                    kOtherThreadId,
                    [&racing_message_count](auto&&) {
                        ++racing_message_count;
                    },
                    [&racing_message_count](auto&&) {
                        ++racing_message_count;
                    },
                    os_resources_);
                ASSERT_TRUE(next_result);
                EXPECT_TRUE(next_result.value());
            }
            EXPECT_EQ(racing_message_count, 0);
        }
        // for short_message_count_ starting from 1, we will be consuming our queued messages here
    };

    const auto next_result = ResmgrReceiverTraits::receive_next(
        fd_,
        kThreadId,
        blocking_short_message_processor,
        [this](auto&&) {
            ++medium_message_count_;
        },
        os_resources_);
    ASSERT_TRUE(next_result);
    EXPECT_TRUE(next_result.value());
    EXPECT_EQ(short_message_count_, 1 + kMaxNumberMessagesInQueue);
    EXPECT_EQ(medium_message_count_, 0);
    EXPECT_EQ(rejected_message_count_, 2 * kMaxNumberMessagesInQueue);
}

// Tests for abnormal scenarios

TEST_F(ResmgrReceiverFixture, IgnoreDispatchErrors)
{
    auto test_error = score::cpp::make_unexpected(score::os::Error::createFromErrno(EINVAL));

    EXPECT_CALL(*raw_dispatch_mock_, dispatch_block(&dispatch_contexts_[kThreadId]))
        .WillOnce(Return(score::cpp::blank{}));
    EXPECT_CALL(*raw_dispatch_mock_, dispatch_handler(&dispatch_contexts_[kThreadId]))
        .WillOnce(Return(score::cpp::make_unexpected(-1)));

    ReceiveNextExpectShortMedium(0, 0);

    EXPECT_CALL(*raw_dispatch_mock_, dispatch_block(&dispatch_contexts_[kThreadId])).WillOnce(Return(test_error));

    ReceiveNextExpectShortMedium(0, 0);
}

TEST_F(ResmgrReceiverFixture, ReceiveInvalidStopMessage)
{
    EXPECT_CALL(*raw_dispatch_mock_, dispatch_block(&dispatch_contexts_[kThreadId])).WillOnce([this](auto&& ctp) {
        // prepare context for "receiving" side_channel "message" from NOT our own process
        ctp->resmgr_context.info.pid = os_resources_.unistd->getpid() + 1;
        ctp->resmgr_context.rcvid = kSideChannelRcvid;
        (*message_handler_)(&ctp->resmgr_context, 0, 0, nullptr);
        return score::cpp::blank{};
    });
    EXPECT_CALL(*raw_dispatch_mock_, dispatch_handler(&dispatch_contexts_[kThreadId]))
        .WillOnce(Return(score::cpp::blank{}));
    EXPECT_CALL(*raw_channel_mock_, MsgError(kSideChannelRcvid, _));
    EXPECT_CALL(*raw_unistd_mock_, getpid()).Times(2).WillRepeatedly(Return(0));

    ReceiveNextExpectShortMedium(0, 0);
}

TEST_F(ResmgrReceiverFixture, ReceiveWrongSizeMessage)
{
    ExpectWriteRequests(sizeof(ShortMessage) + sizeof(MediumMessage));

    ReceiveNextExpectShortMedium(0, 0);
}

TEST_F(ResmgrReceiverFixtureDefensive, OpenReceiverDefensiveProgramming)
{
    // dispatch_create_channel
    EXPECT_FALSE(
        ResmgrReceiverTraits::open_receiver(kIdentifier, kAllowedUids, kMaxNumberMessagesInQueue, os_resources_));
    // resmgr_attach
    EXPECT_FALSE(
        ResmgrReceiverTraits::open_receiver(kIdentifier, kAllowedUids, kMaxNumberMessagesInQueue, os_resources_));
    // message_attach
    EXPECT_FALSE(
        ResmgrReceiverTraits::open_receiver(kIdentifier, kAllowedUids, kMaxNumberMessagesInQueue, os_resources_));
    // message_connect
    EXPECT_FALSE(
        ResmgrReceiverTraits::open_receiver(kIdentifier, kAllowedUids, kMaxNumberMessagesInQueue, os_resources_));
    // dispatch_context_alloc
    EXPECT_FALSE(
        ResmgrReceiverTraits::open_receiver(kIdentifier, kAllowedUids, kMaxNumberMessagesInQueue, os_resources_));
    // finally, open it successfully
    SetUpReceiver(kAllowedUids);

    ExpectWriteRequests(sizeof(ShortMessage));

    // NOT a score::os::error
    EXPECT_CALL(*raw_iofunc_mock_, iofunc_write_verify)
        .WillOnce(Return(score::cpp::make_unexpected(EINVAL)))
        .RetiresOnSaturation();
    ReceiveNextExpectShortMedium(0, 0);

    auto test_error = score::cpp::make_unexpected(score::os::Error::createFromErrno(EINVAL));
    EXPECT_CALL(*raw_dispatch_mock_, resmgr_msgget).WillOnce(Return(test_error)).RetiresOnSaturation();
    ReceiveNextExpectShortMedium(0, 0);

    ExpectAbnormalWrite(WriteAnomaly::XType);
    ReceiveNextExpectShortMedium(0, 0);

    ExpectAbnormalWrite(WriteAnomaly::Clipped);
    ReceiveNextExpectShortMedium(0, 0);

    ExpectAbnormalWrite(WriteAnomaly::Pid);
    ReceiveNextExpectShortMedium(0, 0);

    ExpectAbnormalWrite(WriteAnomaly::Pid, sizeof(MediumMessage));
    ReceiveNextExpectShortMedium(0, 0);

    // finally, receive short message
    ReceiveNextExpectShortMedium(1, 0);
}

TEST_F(ResmgrReceiverFixture, ClosingNullptrReceiverIsHarmless)
{
    ResmgrReceiverTraits::close_receiver(nullptr, "", os_resources_);
}

TEST_F(ResmgrReceiverFixtureWithUids, ReceiveConnectFailInfo)
{
    ExpectOpenRequest(kUidFailInfo, EINVAL);

    ReceiveNextExpectConnects(0);
}

}  // namespace
}  // namespace score::mw::com::message_passing
