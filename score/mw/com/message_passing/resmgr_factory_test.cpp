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
#include <gtest/gtest.h>

#include "score/concurrency/thread_pool.h"

#include "score/mw/com/message_passing/qnx/resmgr_receiver_traits.h"
#include "score/mw/com/message_passing/receiver_factory_impl.h"
#include "score/mw/com/message_passing/sender_factory_impl.h"

#include "score/os/mocklib/qnx/mock_channel.h"
#include "score/os/mocklib/qnx/mock_dispatch.h"
#include "score/os/mocklib/qnx/mock_iofunc.h"

#include "score/os/unistd.h"

namespace score::mw::com::message_passing
{
namespace
{

using namespace ::testing;

TEST(ResmgrFactoryTest, Senders)
{
    NiceMock<score::os::MockChannel> channel_mock_{};
    NiceMock<score::os::MockDispatch> dispatch_mock_{};
    NiceMock<score::os::MockIoFunc> iofunc_mock_{};

    score::os::Channel::set_testing_instance(channel_mock_);
    score::os::Dispatch::set_testing_instance(dispatch_mock_);
    score::os::IoFunc::set_testing_instance(iofunc_mock_);

    std::string identifier1{"/ResmgrFactoryTest1"};
    std::string identifier2{"/ResmgrFactoryTest2"};
    score::cpp::stop_source stop{};
    stop.request_stop();
    auto sender1 = SenderFactoryImpl::Create(identifier1, stop.get_token());
    auto sender2 = SenderFactoryImpl::Create(identifier2, stop.get_token());
    EXPECT_TRUE(sender1);
    EXPECT_TRUE(sender2);
    EXPECT_FALSE(sender1->HasNonBlockingGuarantee());

    score::os::IoFunc::restore_instance();
    score::os::Dispatch::restore_instance();
    score::os::Channel::restore_instance();
}

class ResmgrFactoryFixture : public ::testing::Test
{
  public:
    void SetUp() override
    {
        score::os::Channel::set_testing_instance(channel_mock_);
        score::os::Dispatch::set_testing_instance(dispatch_mock_);
        score::os::IoFunc::set_testing_instance(iofunc_mock_);

        // Minimum setup to successfully start and finish listening session
        EXPECT_CALL(dispatch_mock_, message_attach)
            .WillRepeatedly(DoAll(SaveArg<4>(&message_handler_), Return(score::cpp::blank{})));
        EXPECT_CALL(dispatch_mock_, message_connect).WillRepeatedly(Return(kSideChannelCoid));
        EXPECT_CALL(dispatch_mock_, dispatch_context_alloc)
            .Times(AtMost(ResmgrReceiverTraits::kConcurrency))
            .WillRepeatedly([this](dispatch_t*) {
                dispatch_contexts_[dispatch_contexts_count_].resmgr_context.dpp = kDispatchPointer;
                dispatch_contexts_[dispatch_contexts_count_].resmgr_context.id = kDispatchId;
                dispatch_contexts_[dispatch_contexts_count_].resmgr_context.extra = &extra_[dispatch_contexts_count_];
                return &dispatch_contexts_[dispatch_contexts_count_++];
            });
        EXPECT_CALL(dispatch_mock_, dispatch_block).WillRepeatedly([this](auto&& ctp) {
            ctp->resmgr_context.info.pid = score::os::Unistd::instance().getpid();
            ctp->resmgr_context.rcvid = kSideChannelRcvid;
            (*message_handler_)(&ctp->resmgr_context, 0, 0, nullptr);
            return score::cpp::blank{};
        });
    }

    void TearDown() override
    {
        score::os::IoFunc::restore_instance();
        score::os::Dispatch::restore_instance();
        score::os::Channel::restore_instance();
    }

    NiceMock<score::os::MockChannel> channel_mock_{};
    NiceMock<score::os::MockDispatch> dispatch_mock_{};
    NiceMock<score::os::MockIoFunc> iofunc_mock_{};

    static constexpr dispatch_t* kDispatchPointer = nullptr;  // incomplete class by design, cannot instantiate
    static constexpr std::uint32_t kDispatchId = 1;
    static constexpr std::uint32_t kSideChannelCoid = 2;
    static constexpr std::uint32_t kSideChannelRcvid = 3;

    _extended_context extra_[ResmgrReceiverTraits::kConcurrency];
    dispatch_context_t dispatch_contexts_[ResmgrReceiverTraits::kConcurrency];
    std::uint32_t dispatch_contexts_count_{0};

    std::int32_t (*message_handler_)(message_context_t* const ctp,
                                     const std::int32_t code,
                                     const std::uint32_t flags,
                                     void* const handle) noexcept {nullptr};
};

TEST_F(ResmgrFactoryFixture, Receivers)
{
    std::string identifier1{"/ResmgrFactoryTest1"};
    std::string identifier2{"/ResmgrFactoryTest2"};
    score::concurrency::ThreadPool thread_pool1{1};
    score::concurrency::ThreadPool thread_pool2{2};
    auto receiver1 = ReceiverFactoryImpl::Create(
        identifier1, thread_pool1, {}, ReceiverConfig{}, score::cpp::pmr::get_default_resource());
    auto receiver2 = ReceiverFactoryImpl::Create(
        identifier2, thread_pool2, {}, ReceiverConfig{}, score::cpp::pmr::get_default_resource());
    EXPECT_TRUE(receiver1);
    EXPECT_TRUE(receiver2);
    receiver2->StartListening();
}

}  // namespace
}  // namespace score::mw::com::message_passing
