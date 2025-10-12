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

#include "score/message_passing/mock/shared_resource_engine_mock.h"

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
        engine_ = score::cpp::pmr::make_shared<SharedResourceEngineMock>(resource_);
        EXPECT_CALL(*engine_, GetMemoryResource()).WillRepeatedly(Return(resource_));
        EXPECT_CALL(*engine_, IsOnCallbackThread()).Times(AtLeast(0)).WillRepeatedly([&]() {
            return on_callback_thread_;
        });
    }

    void TearDown() override
    {
        connect_command_callback_ = ISharedResourceEngine::CommandCallback{};
        engine_.reset();
    }

    void CatchImmediateConnectCommand()
    {
        EXPECT_CALL(*engine_, EnqueueCommand(_, ISharedResourceEngine::TimePoint{}, _, _))
            .WillOnce([&](auto&, auto, ISharedResourceEngine::CommandCallback callback, auto) {
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

    void MakeSuccessfulConnection(detail::ClientConnection& connection)
    {
        CatchImmediateConnectCommand();
        connection.Start(IClientConnection::StateCallback(), IClientConnection::NotifyCallback());
        EXPECT_EQ(connection.GetState(), State::kStarting);

        AtTryOpenCall_Return(kValidFd);
        CatchPosixEndpointRegistration();
        InvokeConnectCommand();
        EXPECT_EQ(connection.GetState(), State::kReady);
        EXPECT_EQ(posix_endpoint_->fd, kValidFd);
        EXPECT_EQ(posix_endpoint_->owner, &connection);
        EXPECT_EQ(posix_endpoint_->max_receive_size, 1024);
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

    score::cpp::pmr::memory_resource* resource_{};
    std::shared_ptr<SharedResourceEngineMock> engine_{};
    const std::string service_identifier_{"test_identifier"};
    const ServiceProtocolConfig protocol_config_{service_identifier_, 1024, 1024, 1024};
    const IClientFactory::ClientConfig client_config_{};
    ISharedResourceEngine::CommandCallback connect_command_callback_{};
    ISharedResourceEngine::CommandCallback disconnect_command_callback_{};
    ISharedResourceEngine::PosixEndpointEntry* posix_endpoint_{};
    bool on_callback_thread_{false};
};

TEST_F(ClientConnectionTest, Constructed)
{
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    EXPECT_EQ(connection.GetState(), State::kStopped);
    EXPECT_EQ(connection.GetStopReason(), StopReason::kInit);
}

TEST_F(ClientConnectionTest, TryingToConnectOnceStoppingOnHardError)
{
    const auto error_list_to_test = {std::make_pair(EACCES, StopReason::kPermission),
                                     std::make_pair(EPIPE, StopReason::kIoError)};

    for (auto entry : error_list_to_test)
    {
        detail::ClientConnection connection(engine_, protocol_config_, client_config_);

        CatchImmediateConnectCommand();
        connection.Start(IClientConnection::StateCallback(), IClientConnection::NotifyCallback());
        EXPECT_EQ(connection.GetState(), State::kStarting);

        AtTryOpenCall_Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(entry.first)));
        ExpectCleanUpOwner(connection);
        InvokeConnectCommand();

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
        if (entry == *error_list_to_test.begin())
        {
            CatchImmediateConnectCommand();
            connection.Start(IClientConnection::StateCallback(), IClientConnection::NotifyCallback());
            EXPECT_EQ(connection.GetState(), State::kStarting);
        }
        AtTryOpenCall_Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(entry)));
        CatchTimedConnectCommand();
        InvokeConnectCommand();
        EXPECT_EQ(connection.GetState(), State::kStarting);
    }

    AtTryOpenCall_Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(EACCES)));
    ExpectCleanUpOwner(connection);
    InvokeConnectCommand();

    EXPECT_EQ(connection.GetState(), State::kStopped);
    EXPECT_EQ(connection.GetStopReason(), StopReason::kPermission);
}

TEST_F(ClientConnectionTest, SuccessfullyConnectingAtFirstAttemptThenStopping)
{
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    StopCurrentConnection(connection);
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

}  // namespace
}  // namespace message_passing
}  // namespace score
