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

#include "score/message_passing/client_connection.h"

#include "score/message_passing/client_server_communication.h"
#include "score/message_passing/mock/shared_resource_engine_mock.h"

#include <future>
#include <thread>

namespace score
{
namespace message_passing
{
namespace
{

using namespace ::testing;

using State = detail::ClientConnection::State;
using StopReason = detail::ClientConnection::StopReason;

void stdout_handler(const score::cpp::handler_parameters& param)
{
    std::cout << "In " << param.file << ":" << param.line << " " << param.function << " condition " << param.condition
              << " >> " << param.message << std::endl;
}

const std::int32_t kValidFd = 1;

class ClientConnectionTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        score::cpp::set_assertion_handler(stdout_handler);

        resource_ = score::cpp::pmr::get_default_resource();
        engine_ = score::cpp::pmr::make_shared<StrictMock<SharedResourceEngineMock>>(resource_);
        EXPECT_CALL(*engine_, GetMemoryResource()).WillRepeatedly(Return(resource_));
        EXPECT_CALL(*engine_, IsOnCallbackThread()).Times(AtLeast(0)).WillRepeatedly([&]() -> bool {
            return on_callback_thread_;
        });
    }

    void TearDown() override
    {
        if (background_thread_.joinable())
        {
            background_thread_.join();
        }
        connect_command_callback_ = ISharedResourceEngine::CommandCallback{};
        engine_.reset();
    }

    void CatchImmediateConnectEndpointRegistrationCommand()
    {
        EXPECT_CALL(*engine_, EnqueueCommand(_, ISharedResourceEngine::TimePoint{}, _, _))
            .WillOnce([&](auto&, auto, ISharedResourceEngine::CommandCallback callback, auto) {
                // reusing the connect callback in case ow queued endpoint registration
                connect_command_callback_ = std::move(callback);
            });
    }

    void CatchTimedConnectCommand()
    {
        EXPECT_CALL(*engine_, EnqueueCommand(_, Gt(ISharedResourceEngine::Clock::now()), _, _))
            .WillOnce([&](auto&, auto, ISharedResourceEngine::CommandCallback callback, auto) {
                connect_command_callback_ = std::move(callback);
            });
    }

    void CatchDisconnectCommand()
    {
        EXPECT_CALL(*engine_, EnqueueCommand(_, ISharedResourceEngine::TimePoint{}, _, _))
            .WillOnce([&](auto&, auto, ISharedResourceEngine::CommandCallback callback, auto) {
                disconnect_command_callback_ = std::move(callback);
            });
    }

    void CatchSendQueueCommand()
    {
        EXPECT_CALL(*engine_, EnqueueCommand(_, ISharedResourceEngine::TimePoint{}, _, _))
            .WillOnce([&](auto&, auto, ISharedResourceEngine::CommandCallback callback, auto) {
                send_queue_command_callback_ = std::move(callback);
            });
    }

    void AtTryOpenCall_Return(score::cpp::expected<std::int32_t, score::os::Error> result)
    {
        EXPECT_CALL(*engine_, TryOpenClientConnection(std::string_view{service_identifier_})).WillOnce(Return(result));
    }

    void ExpectCleanUpOwner(detail::ClientConnection& connection)
    {
        EXPECT_CALL(*engine_, CleanUpOwner(&connection)).WillOnce([&](auto) {
            EXPECT_EQ(connection.GetState(), State::kStopping);
        });
    }

    void ExpectCloseFd()
    {
        EXPECT_CALL(*engine_, CloseClientConnection(kValidFd)).Times(1);
    }

    void InvokeConnectCommand()
    {
        on_callback_thread_ = true;
        {
            auto callback = std::move(connect_command_callback_);
            callback(ISharedResourceEngine::Clock::now());
            // the destructor for callback capture, if any, is called now
        }
        on_callback_thread_ = false;
    }

    void InvokeDisconnectCommand()
    {
        on_callback_thread_ = true;
        {
            auto callback = std::move(disconnect_command_callback_);
            callback(ISharedResourceEngine::Clock::now());
            // the destructor for callback capture, if any, is called now
        }
        on_callback_thread_ = false;
    }

    void InvokeSendQueueCommand()
    {
        on_callback_thread_ = true;
        {
            auto callback = std::move(send_queue_command_callback_);
            callback(ISharedResourceEngine::Clock::now());
            // the destructor for callback capture, if any, is called now
        }
        on_callback_thread_ = false;
    }

    void ExpectAndInvokeDelayedDisconnectCommand()
    {
        EXPECT_CALL(*engine_, EnqueueCommand(_, ISharedResourceEngine::TimePoint{}, _, _))
            .WillOnce([&](auto&, auto, ISharedResourceEngine::CommandCallback callback, auto) {
                disconnect_command_callback_ = std::move(callback);
                background_thread_ = std::thread([&]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    on_callback_thread_ = true;
                    auto disconnect_callback = std::move(disconnect_command_callback_);
                    disconnect_callback(ISharedResourceEngine::Clock::now());
                    on_callback_thread_ = false;
                });
            });
        EXPECT_CALL(*engine_, CleanUpOwner);
        EXPECT_CALL(*engine_, CloseClientConnection);
    }

    void CatchPosixEndpointRegistration()
    {
        EXPECT_CALL(*engine_, RegisterPosixEndpoint(_))
            .WillOnce([&](ISharedResourceEngine::PosixEndpointEntry& endpoint) {
                posix_endpoint_ = &endpoint;
            });
    }

    void AtPosixEndpointUnregistration_InvokeDisconnect()
    {
        EXPECT_CALL(*engine_, UnregisterPosixEndpoint(Address(posix_endpoint_))).WillOnce([&](auto&) {
            on_callback_thread_ = true;
            posix_endpoint_->disconnect();
            on_callback_thread_ = false;
        });
    }

    void AtProtocolReceive_Return(std::uint8_t code,
                                  score::cpp::expected<score::cpp::span<const std::uint8_t>, score::os::Error> result)
    {
        EXPECT_CALL(*engine_, ReceiveProtocolMessage(kValidFd, _))
            .WillOnce(DoAll(SetArgReferee<1>(code), Return(result)));
    }

    void InvokeEndpointInput()
    {
        on_callback_thread_ = true;
        posix_endpoint_->input();
        on_callback_thread_ = false;
    }

    void MakeSuccessfulConnection(detail::ClientConnection& connection,
                                  IClientConnection::StateCallback state_callback = {},
                                  IClientConnection::NotifyCallback notify_callback = {})
    {
        AtTryOpenCall_Return(kValidFd);
        CatchImmediateConnectEndpointRegistrationCommand();

        connection.Start(std::move(state_callback), std::move(notify_callback));

        CatchPosixEndpointRegistration();
        InvokeConnectCommand();

        EXPECT_EQ(connection.GetState(), State::kReady);
        EXPECT_EQ(posix_endpoint_->fd, kValidFd);
        EXPECT_EQ(posix_endpoint_->owner, &connection);
        EXPECT_EQ(posix_endpoint_->max_receive_size, kMaxNotifySize);
    }

    void StopConnectionInProgress(detail::ClientConnection& connection)
    {
        CatchDisconnectCommand();
        connection.Stop();
        EXPECT_EQ(connection.GetState(), State::kStopping);
        EXPECT_EQ(connection.GetStopReason(), StopReason::kUserRequested);

        InvokeDisconnectCommand();
        EXPECT_EQ(connection.GetState(), State::kStopped);
        EXPECT_EQ(connection.GetStopReason(), StopReason::kUserRequested);
    }

    void StopCurrentConnection(detail::ClientConnection& connection)
    {
        CatchDisconnectCommand();
        connection.Stop();
        EXPECT_EQ(connection.GetState(), State::kStopping);
        EXPECT_EQ(connection.GetStopReason(), StopReason::kUserRequested);

        ExpectCleanUpOwner(connection);
        ExpectCloseFd();
        InvokeDisconnectCommand();
        EXPECT_EQ(connection.GetState(), State::kStopped);
        EXPECT_EQ(connection.GetStopReason(), StopReason::kUserRequested);
    }

    void StopCurrentConnectionAsIfInCallback(detail::ClientConnection& connection)
    {
        on_callback_thread_ = true;
        ExpectCleanUpOwner(connection);
        ExpectCloseFd();
        connection.Stop();

        EXPECT_EQ(connection.GetState(), State::kStopped);
        EXPECT_EQ(connection.GetStopReason(), StopReason::kUserRequested);
    }

    constexpr static std::uint32_t kMaxSendSize = 8U;
    constexpr static std::uint32_t kMaxReplySize = 10U;
    constexpr static std::uint32_t kMaxNotifySize = 12U;

    score::cpp::pmr::memory_resource* resource_{};
    std::shared_ptr<StrictMock<SharedResourceEngineMock>> engine_{};
    const std::string service_identifier_{"test_identifier"};
    const ServiceProtocolConfig protocol_config_{service_identifier_, kMaxSendSize, kMaxReplySize, kMaxNotifySize};
    IClientFactory::ClientConfig client_config_{0, 1, false, false};
    ISharedResourceEngine::CommandCallback connect_command_callback_{};
    ISharedResourceEngine::CommandCallback disconnect_command_callback_{};
    ISharedResourceEngine::CommandCallback send_queue_command_callback_{};
    ISharedResourceEngine::PosixEndpointEntry* posix_endpoint_{};
    std::atomic<bool> on_callback_thread_{false};  // only atomic for tsan and sleep synchronization
    std::thread background_thread_{};
};

TEST_F(ClientConnectionTest, Constructed)
{
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    EXPECT_EQ(connection.GetState(), State::kStopped);
    EXPECT_EQ(connection.GetStopReason(), StopReason::kInit);
}

TEST_F(ClientConnectionTest, ConstructedStopDoesNothing)
{
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    connection.Stop();
    EXPECT_EQ(connection.GetState(), State::kStopped);
    EXPECT_EQ(connection.GetStopReason(), StopReason::kInit);
}

TEST_F(ClientConnectionTest, TryingToConnectOnceStoppingWhileConnecting)
{
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);

    EXPECT_CALL(*engine_, TryOpenClientConnection(std::string_view{service_identifier_}))
        .WillOnce([this, &connection](auto&&) {
            StopConnectionInProgress(connection);
            return score::cpp::make_unexpected(score::os::Error::createFromErrno(EIO));  // this error code would be ignored
        });
    ExpectCleanUpOwner(connection);

    connection.Start(IClientConnection::StateCallback(), IClientConnection::NotifyCallback());
    EXPECT_EQ(connection.GetState(), State::kStopped);
    EXPECT_EQ(connection.GetStopReason(), StopReason::kUserRequested);
}

TEST_F(ClientConnectionTest, TryingToConnectOnceStoppingOnHardError)
{
    const auto error_list_to_test = {std::make_pair(EACCES, StopReason::kPermission),
                                     std::make_pair(EPIPE, StopReason::kIoError)};

    for (auto entry : error_list_to_test)
    {
        detail::ClientConnection connection(engine_, protocol_config_, client_config_);

        AtTryOpenCall_Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(entry.first)));
        ExpectCleanUpOwner(connection);

        connection.Start(IClientConnection::StateCallback(), IClientConnection::NotifyCallback());
        EXPECT_EQ(connection.GetState(), State::kStopped);
        EXPECT_EQ(connection.GetStopReason(), entry.second);
    }
}

TEST_F(ClientConnectionTest, TryingToConnectMultipleTimesStoppingOnPermissionError)
{
    const auto error_list_to_test = {EAGAIN, ECONNREFUSED, ENOENT};

    detail::ClientConnection connection(engine_, protocol_config_, client_config_);

    for (auto entry : error_list_to_test)
    {
        AtTryOpenCall_Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(entry)));
        CatchTimedConnectCommand();
        if (entry == *error_list_to_test.begin())
        {
            connection.Start(IClientConnection::StateCallback(), IClientConnection::NotifyCallback());
        }
        else
        {
            InvokeConnectCommand();
        }
        EXPECT_EQ(connection.GetState(), State::kStarting);
    }

    AtTryOpenCall_Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(EACCES)));
    ExpectCleanUpOwner(connection);
    InvokeConnectCommand();

    EXPECT_EQ(connection.GetState(), State::kStopped);
    EXPECT_EQ(connection.GetStopReason(), StopReason::kPermission);
}

TEST_F(ClientConnectionTest, TryingToConnectMultipleTimesFinallyConnectingAndImplicitlyStopping)
{
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);

    AtTryOpenCall_Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOENT)));
    CatchTimedConnectCommand();
    connection.Start(IClientConnection::StateCallback(), IClientConnection::NotifyCallback());
    EXPECT_EQ(connection.GetState(), State::kStarting);

    // shall be enough to cover kConnectRetryMsMax
    for (std::size_t i = 0; i < 20U; ++i)
    {
        AtTryOpenCall_Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOENT)));
        CatchTimedConnectCommand();
        InvokeConnectCommand();
        EXPECT_EQ(connection.GetState(), State::kStarting);
    }

    AtTryOpenCall_Return(kValidFd);
    CatchPosixEndpointRegistration();
    InvokeConnectCommand();
    EXPECT_EQ(connection.GetState(), State::kReady);
    EXPECT_EQ(posix_endpoint_->fd, kValidFd);
    EXPECT_EQ(posix_endpoint_->owner, &connection);
    EXPECT_EQ(posix_endpoint_->max_receive_size, kMaxNotifySize);

    ExpectAndInvokeDelayedDisconnectCommand();
}

TEST_F(ClientConnectionTest, SuccessfullyConnectingAtFirstAttemptThenExplicitlyStopping)
{
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    StopCurrentConnection(connection);
}

TEST_F(ClientConnectionTest, SuccessfullyConnectingAtFirstAttemptThenExplicitlyStoppingAsIfFromCallback)
{
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    StopCurrentConnectionAsIfInCallback(connection);
}

TEST_F(ClientConnectionTest, SuccessfullyConnectingAtFirstAttemptThenFirstReadDisconnected)
{
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    AtProtocolReceive_Return(0, score::cpp::make_unexpected(score::os::Error::createFromErrno(EPIPE)));
    AtPosixEndpointUnregistration_InvokeDisconnect();
    ExpectCleanUpOwner(connection);
    ExpectCloseFd();
    InvokeEndpointInput();
    EXPECT_EQ(connection.GetState(), State::kStopped);
    EXPECT_EQ(connection.GetStopReason(), StopReason::kClosedByPeer);
}

TEST_F(ClientConnectionTest, NotConnectedSendNotReady)
{
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);

    std::array<std::uint8_t, kMaxSendSize> send_buffer;
    std::array<std::uint8_t, kMaxReplySize> reply_buffer;

    auto send_result = connection.Send(send_buffer);
    EXPECT_FALSE(send_result);
    EXPECT_EQ(send_result.error().GetOsDependentErrorCode(), EINVAL);

    auto send_wait_reply_result = connection.SendWaitReply(send_buffer, reply_buffer);
    EXPECT_FALSE(send_wait_reply_result);
    EXPECT_EQ(send_wait_reply_result.error().GetOsDependentErrorCode(), EINVAL);

    auto send_with_callback_result = connection.SendWithCallback(send_buffer, IClientConnection::ReplyCallback{});
    EXPECT_FALSE(send_with_callback_result);
    EXPECT_EQ(send_with_callback_result.error().GetOsDependentErrorCode(), EINVAL);
}

TEST_F(ClientConnectionTest, ConnectedRestartdDoesNothing)
{
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    connection.Restart();
    EXPECT_EQ(connection.GetState(), State::kReady);

    StopCurrentConnection(connection);
}

TEST_F(ClientConnectionTest, SendTooLarge)
{
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    std::array<std::uint8_t, kMaxSendSize + 1> send_buffer;
    std::array<std::uint8_t, kMaxReplySize> reply_buffer;

    auto send_result = connection.Send(send_buffer);
    EXPECT_FALSE(send_result);
    EXPECT_EQ(send_result.error().GetOsDependentErrorCode(), ENOMEM);

    auto send_wait_reply_result = connection.SendWaitReply(send_buffer, reply_buffer);
    EXPECT_FALSE(send_wait_reply_result);
    EXPECT_EQ(send_wait_reply_result.error().GetOsDependentErrorCode(), ENOMEM);

    auto send_with_callback_result = connection.SendWithCallback(send_buffer, IClientConnection::ReplyCallback{});
    EXPECT_FALSE(send_with_callback_result);
    EXPECT_EQ(send_with_callback_result.error().GetOsDependentErrorCode(), ENOMEM);

    StopCurrentConnection(connection);
}

TEST_F(ClientConnectionTest, SendIsDirectWithOurDefaultConfig)
{
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    EXPECT_CALL(*engine_, SendProtocolMessage);

    std::array<std::uint8_t, kMaxSendSize> send_buffer;
    auto send_result = connection.Send(send_buffer);
    EXPECT_TRUE(send_result);

    StopCurrentConnection(connection);
}

TEST_F(ClientConnectionTest, SendIsDirectWithFullyOrderedAndEmptyQueue)
{
    client_config_.fully_ordered = true;
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    EXPECT_CALL(*engine_, SendProtocolMessage);

    std::array<std::uint8_t, kMaxSendSize> send_buffer;
    auto send_result = connection.Send(send_buffer);
    EXPECT_TRUE(send_result);

    StopCurrentConnection(connection);
}

TEST_F(ClientConnectionTest, SendIsQueuedIfTrulyAsync)
{
    client_config_.max_queued_sends = 2;
    client_config_.truly_async = true;
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    CatchSendQueueCommand();

    std::array<std::uint8_t, kMaxSendSize> send_buffer;
    auto send_result = connection.Send(send_buffer);
    EXPECT_TRUE(send_result);

    send_result = connection.Send(send_buffer);
    EXPECT_TRUE(send_result);

    // third send shall fail as we only have two slots in the send queue
    send_result = connection.Send(send_buffer);
    EXPECT_FALSE(send_result);
    EXPECT_EQ(send_result.error().GetOsDependentErrorCode(), ENOMEM);

    // both queued messages shall be sent by the same command, as they have no receive callbacks
    EXPECT_CALL(*engine_, SendProtocolMessage).Times(2);
    InvokeSendQueueCommand();

    StopCurrentConnection(connection);
}

TEST_F(ClientConnectionTest, SendIsNotQueuedIfTrulyAsyncButNoSlots)
{
    // this is a meaningless case, but as we check for it in the implementation, we shall cover it
    client_config_.max_queued_sends = 0;
    client_config_.truly_async = true;
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    std::array<std::uint8_t, kMaxSendSize> send_buffer;
    auto send_result = connection.Send(send_buffer);
    EXPECT_FALSE(send_result);
    EXPECT_EQ(send_result.error().GetOsDependentErrorCode(), ENOMEM);

    StopCurrentConnection(connection);
}

TEST_F(ClientConnectionTest, SendWaitReplyFailsWhenCalledInCallback)
{
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    std::array<std::uint8_t, kMaxSendSize> send_buffer;
    std::array<std::uint8_t, kMaxReplySize> reply_buffer;

    on_callback_thread_ = true;
    auto send_wait_reply_result = connection.SendWaitReply(send_buffer, reply_buffer);
    on_callback_thread_ = false;
    EXPECT_FALSE(send_wait_reply_result);
    EXPECT_EQ(send_wait_reply_result.error().GetOsDependentErrorCode(), EAGAIN);

    StopCurrentConnection(connection);
}

TEST_F(ClientConnectionTest, SendWaitReplyFailsWhenCannotSendMessageDirectly)
{
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    std::array<std::uint8_t, kMaxSendSize> send_buffer;
    std::array<std::uint8_t, kMaxReplySize> reply_buffer;

    EXPECT_CALL(*engine_, SendProtocolMessage)
        .WillOnce(Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(EPIPE))));

    auto send_wait_reply_result = connection.SendWaitReply(send_buffer, reply_buffer);
    EXPECT_FALSE(send_wait_reply_result);
    EXPECT_EQ(send_wait_reply_result.error().GetOsDependentErrorCode(), EPIPE);

    StopCurrentConnection(connection);
}

TEST_F(ClientConnectionTest, SendWaitReplyFailsDirectlyButUnblocksQueue)
{
    client_config_.truly_async = true;
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    std::array<std::uint8_t, kMaxSendSize> send_buffer;
    std::array<std::uint8_t, kMaxReplySize> reply_buffer;

    EXPECT_CALL(*engine_, SendProtocolMessage).WillOnce([&](auto&&...) {
        // as truly_async, this one will go into the queue
        auto send_result = connection.Send(send_buffer);
        EXPECT_TRUE(send_result);

        return score::cpp::make_unexpected(score::os::Error::createFromErrno(EPIPE));
    });

    CatchSendQueueCommand();

    auto send_wait_reply_result = connection.SendWaitReply(send_buffer, reply_buffer);
    EXPECT_FALSE(send_wait_reply_result);
    EXPECT_EQ(send_wait_reply_result.error().GetOsDependentErrorCode(), EPIPE);

    EXPECT_CALL(*engine_, SendProtocolMessage);  // will be called from unblocked queue
    InvokeSendQueueCommand();

    StopCurrentConnection(connection);
}

TEST_F(ClientConnectionTest, SendWaitReplyFailsWhenCannotQueueMessage)
{
    client_config_.max_queued_sends = 1;  // explicitly
    client_config_.truly_async = true;    // make sure Send goes into queue
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    std::array<std::uint8_t, kMaxSendSize> send_buffer;
    std::array<std::uint8_t, kMaxReplySize> reply_buffer;

    CatchSendQueueCommand();

    // occupy the queue slot
    auto send_result = connection.Send(send_buffer);
    EXPECT_TRUE(send_result);

    // second send shall fail as we only have one slot in the send queue
    auto send_wait_reply_result = connection.SendWaitReply(send_buffer, reply_buffer);
    EXPECT_FALSE(send_wait_reply_result);
    EXPECT_EQ(send_wait_reply_result.error().GetOsDependentErrorCode(), ENOMEM);

    EXPECT_CALL(*engine_, SendProtocolMessage);
    InvokeSendQueueCommand();

    StopCurrentConnection(connection);
}

TEST_F(ClientConnectionTest, SendWaitReplyFailsWhenQueuedMessageCancels)
{
    client_config_.max_queued_sends = 2;
    client_config_.truly_async = true;
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    std::promise<void> done;
    // once for Send, then once for SendWaitReply
    EXPECT_CALL(*engine_, SendProtocolMessage).Times(2);

    std::array<std::uint8_t, kMaxSendSize> send_buffer;

    CatchSendQueueCommand();

    // queued, because of truly_async
    auto send_result = connection.Send(send_buffer);
    EXPECT_TRUE(send_result);

    background_thread_ = std::thread([&connection]() {
        std::array<std::uint8_t, kMaxSendSize> another_send_buffer;
        std::array<std::uint8_t, kMaxReplySize> reply_buffer;
        auto send_wait_reply_result = connection.SendWaitReply(another_send_buffer, reply_buffer);
        EXPECT_FALSE(send_wait_reply_result);
        EXPECT_EQ(send_wait_reply_result.error().GetOsDependentErrorCode(), EPIPE);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // unblock queue, it shall send both messages
    InvokeSendQueueCommand();

    // close connection without replying to SendWaitReply
    StopCurrentConnection(connection);
}

TEST_F(ClientConnectionTest, SendWaitReplyFailsWhenReceiveTooLong)
{
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    std::array<std::uint8_t, kMaxSendSize> send_buffer;
    std::array<std::uint8_t, kMaxReplySize> reply_buffer;

    EXPECT_CALL(*engine_, SendProtocolMessage).WillOnce([&](auto&&...) {
        std::array<std::uint8_t, kMaxReplySize + 1> long_reply_buffer;
        AtProtocolReceive_Return(score::cpp::to_underlying(detail::ServerToClient::REPLY), long_reply_buffer);
        InvokeEndpointInput();
        return score::cpp::blank{};
    });

    auto send_wait_reply_result = connection.SendWaitReply(send_buffer, reply_buffer);
    EXPECT_FALSE(send_wait_reply_result);
    EXPECT_EQ(send_wait_reply_result.error().GetOsDependentErrorCode(), ENOMEM);

    StopCurrentConnection(connection);
}

TEST_F(ClientConnectionTest, SendWithCallbackFailsWhenCannotSendMessageDirectly)
{
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    std::array<std::uint8_t, kMaxSendSize> send_buffer;

    EXPECT_CALL(*engine_, SendProtocolMessage)
        .WillOnce(Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(EPIPE))));

    auto send_with_callback_result = connection.SendWithCallback(send_buffer, IClientConnection::ReplyCallback{});
    EXPECT_FALSE(send_with_callback_result);
    EXPECT_EQ(send_with_callback_result.error().GetOsDependentErrorCode(), EPIPE);

    StopCurrentConnection(connection);
}

TEST_F(ClientConnectionTest, SendWithCallbackFailsWhenCannotQueueMessage)
{
    // without truly_async, the first SendWithCallback is not queued and does not need async reply slots
    client_config_.max_async_replies = 0;
    client_config_.max_queued_sends = 0;
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    std::array<std::uint8_t, kMaxSendSize> send_buffer;

    EXPECT_CALL(*engine_, SendProtocolMessage);

    // first send shall succeed
    auto send_with_callback_result = connection.SendWithCallback(send_buffer, [](auto&&) {});
    EXPECT_TRUE(send_with_callback_result);

    // second send shall fail as we only have one slot in the send queue
    send_with_callback_result = connection.SendWithCallback(send_buffer, [](auto&&) {});
    EXPECT_FALSE(send_with_callback_result);
    EXPECT_EQ(send_with_callback_result.error().GetOsDependentErrorCode(), ENOMEM);

    StopCurrentConnection(connection);
}

TEST_F(ClientConnectionTest, SendWithCallbackFailsWhenCannotQueueMessageTrulyAsync)
{
    client_config_.truly_async = true;
    client_config_.max_async_replies = 0;
    client_config_.max_queued_sends = 0;
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    std::array<std::uint8_t, kMaxSendSize> send_buffer;

    // first send shall fail
    auto send_with_callback_result = connection.SendWithCallback(send_buffer, [](auto&&) {});
    EXPECT_EQ(send_with_callback_result.error().GetOsDependentErrorCode(), ENOMEM);

    StopCurrentConnection(connection);
}

TEST_F(ClientConnectionTest, SendWithCallbackReportsFailureWhenQueuedSendFailsTrulyAsync)
{
    client_config_.truly_async = true;
    client_config_.max_async_replies = 1;
    client_config_.max_queued_sends = 0;
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    std::array<std::uint8_t, kMaxSendSize> send_buffer;

    CatchSendQueueCommand();

    // the send itself shall succeed
    auto send_with_callback_result = connection.SendWithCallback(send_buffer, [](auto&& message_expected) {
        EXPECT_FALSE(message_expected);
        EXPECT_EQ(message_expected.error().GetOsDependentErrorCode(), EPIPE);
    });
    EXPECT_TRUE(send_with_callback_result);

    EXPECT_CALL(*engine_, SendProtocolMessage)
        .WillOnce(Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(EPIPE))));

    InvokeSendQueueCommand();

    StopCurrentConnection(connection);
}

TEST_F(ClientConnectionTest, QueuedSendsCancelIfConnectionClosed)
{
    client_config_.max_queued_sends = 4;
    client_config_.truly_async = true;
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    std::promise<void> done;
    // inside background thread, when SendWaitReply is queued for reply
    EXPECT_CALL(*engine_, SendProtocolMessage).WillOnce([&done](auto&&...) {
        done.set_value();
        return score::cpp::blank{};
    });

    background_thread_ = std::thread([&connection]() {
        std::array<std::uint8_t, kMaxSendSize> another_send_buffer;
        std::array<std::uint8_t, kMaxReplySize> reply_buffer;
        auto send_wait_reply_result = connection.SendWaitReply(another_send_buffer, reply_buffer);
        EXPECT_FALSE(send_wait_reply_result);
        EXPECT_EQ(send_wait_reply_result.error().GetOsDependentErrorCode(), EPIPE);
    });
    done.get_future().wait();

    std::array<std::uint8_t, kMaxSendSize> send_buffer;

    // queued, because of truly_async
    auto send_result = connection.Send(send_buffer);
    EXPECT_TRUE(send_result);

    auto send_with_callback_result = connection.SendWithCallback(send_buffer, IClientConnection::ReplyCallback{});
    EXPECT_TRUE(send_with_callback_result);

    send_with_callback_result = connection.SendWithCallback(send_buffer, [](auto&& message_expected) {
        EXPECT_FALSE(message_expected);
        EXPECT_EQ(message_expected.error().GetOsDependentErrorCode(), EPIPE);
    });
    EXPECT_TRUE(send_with_callback_result);

    StopCurrentConnection(connection);
}

TEST_F(ClientConnectionTest, ConnectedStopsIfReceivesIoError)
{
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    AtProtocolReceive_Return(0, score::cpp::make_unexpected(score::os::Error::createFromErrno(EIO)));
    AtPosixEndpointUnregistration_InvokeDisconnect();
    ExpectCleanUpOwner(connection);
    ExpectCloseFd();
    InvokeEndpointInput();
    EXPECT_EQ(connection.GetState(), State::kStopped);
    EXPECT_EQ(connection.GetStopReason(), StopReason::kIoError);
}

TEST_F(ClientConnectionTest, ConnectedStopsIfReceivesInvalidCode)
{
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    AtProtocolReceive_Return(score::cpp::to_underlying(detail::ServerToClient::NOTIFY) + 1, {});
    AtPosixEndpointUnregistration_InvokeDisconnect();
    ExpectCleanUpOwner(connection);
    ExpectCloseFd();
    InvokeEndpointInput();
    EXPECT_EQ(connection.GetState(), State::kStopped);
    EXPECT_EQ(connection.GetStopReason(), StopReason::kIoError);
}

TEST_F(ClientConnectionTest, ConnectedStopsIfReceivesReplyWithoutSend)
{
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    AtProtocolReceive_Return(score::cpp::to_underlying(detail::ServerToClient::REPLY), {});
    AtPosixEndpointUnregistration_InvokeDisconnect();
    ExpectCleanUpOwner(connection);
    ExpectCloseFd();
    InvokeEndpointInput();
    EXPECT_EQ(connection.GetState(), State::kStopped);
    EXPECT_EQ(connection.GetStopReason(), StopReason::kIoError);
}

TEST_F(ClientConnectionTest, ConnectedContinuesIfReceivesNotifyWithoutCallback)
{
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    AtProtocolReceive_Return(score::cpp::to_underlying(detail::ServerToClient::NOTIFY), {});
    InvokeEndpointInput();
    EXPECT_EQ(connection.GetState(), State::kReady);

    StopCurrentConnection(connection);
}

TEST_F(ClientConnectionTest, ConnectedReceivesNotifyInCallback)
{
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    std::uint32_t call_counter{0U};
    MakeSuccessfulConnection(connection, IClientConnection::StateCallback{}, [&call_counter](auto&&) noexcept {
        ++call_counter;
    });

    AtProtocolReceive_Return(score::cpp::to_underlying(detail::ServerToClient::NOTIFY), {});
    InvokeEndpointInput();
    EXPECT_EQ(connection.GetState(), State::kReady);
    EXPECT_EQ(call_counter, 1U);

    StopCurrentConnection(connection);
}

}  // namespace
}  // namespace message_passing
}  // namespace score
