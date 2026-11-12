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
#include <memory>
#include <thread>
#include <vector>

namespace score::message_passing
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
        EXPECT_CALL(*engine_, GetLogger()).Times(AtLeast(0)).WillRepeatedly(ReturnRef(logger_));
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

    void CatchImmediateConnectCommand()
    {
        EXPECT_CALL(*engine_, EnqueueCommand(_, ISharedResourceEngine::TimePoint{}, _, _))
            .WillOnce([&](auto&, auto, ISharedResourceEngine::CommandCallback callback, auto) {
                connect_command_callback_ = std::move(callback);
            });
    }

    void CatchEndpointRegistrationCommand()
    {
        EXPECT_CALL(*engine_, EnqueueCommand(_, ISharedResourceEngine::TimePoint{}, _, _))
            .WillOnce([&](auto&, auto, ISharedResourceEngine::CommandCallback callback, auto) {
                endpoint_command_callback_ = std::move(callback);
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
        if (connection_delay_ms_ == 0)
        {
            EXPECT_CALL(*engine_, TryOpenClientConnection(std::string_view{service_identifier_}))
                .WillOnce(Return(result));
        }
        else
        {
            EXPECT_CALL(*engine_, TryOpenClientConnection(std::string_view{service_identifier_}))
                .WillOnce([result, this](auto) {
                    std::this_thread::sleep_for(std::chrono::milliseconds{connection_delay_ms_});
                    return result;
                });
        }
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

    void InvokeEndpointRegistrationCommand()
    {
        on_callback_thread_ = true;
        {
            auto callback = std::move(endpoint_command_callback_);
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

    void InvokeEndpointPing()
    {
        on_callback_thread_ = true;
        posix_endpoint_->ping();
        on_callback_thread_ = false;
    }

    void MakeSuccessfulConnection(detail::ClientConnection& connection,
                                  IClientConnection::StateCallback state_callback = {},
                                  IClientConnection::NotifyCallback notify_callback = {})
    {
        if (client_config_.sync_first_connect)
        {
            AtTryOpenCall_Return(kValidFd);
            CatchEndpointRegistrationCommand();

            connection.Start(std::move(state_callback), std::move(notify_callback));

            CatchPosixEndpointRegistration();
            InvokeEndpointRegistrationCommand();
        }
        else
        {
            CatchImmediateConnectCommand();
            connection.Start(std::move(state_callback), std::move(notify_callback));
            EXPECT_EQ(connection.GetState(), State::kStarting);

            AtTryOpenCall_Return(kValidFd);
            CatchPosixEndpointRegistration();
            InvokeConnectCommand();
        }

        EXPECT_EQ(connection.GetState(), State::kReady);
        EXPECT_EQ(posix_endpoint_->fd, kValidFd);
        EXPECT_EQ(posix_endpoint_->owner, &connection);
        EXPECT_EQ(posix_endpoint_->max_receive_size, kMaxNotifySize);
    }

    void StopConnectionInProgress(detail::ClientConnection& connection)
    {
        if (client_config_.sync_first_connect)
        {
            // for the immediate stop on the user thread
            CatchDisconnectCommand();
            connection.Stop();
            EXPECT_EQ(connection.GetState(), State::kStopping);
            EXPECT_EQ(connection.GetStopReason(), StopReason::kUserRequested);

            InvokeDisconnectCommand();
        }
        else
        {
            connection.Stop();
        }
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
        EXPECT_CALL(*engine_, EnqueueCommand(_, ISharedResourceEngine::TimePoint{}, _, &connection))
            .Times(AtMost(1))
            .WillOnce([&](auto&, auto, ISharedResourceEngine::CommandCallback callback, auto) {
                disconnect_command_callback_ = std::move(callback);
            });
        InvokeDisconnectCommand();
        if (background_thread_.joinable())
        {
            background_thread_.join();
        }
        if (!disconnect_command_callback_.empty())
        {
            InvokeDisconnectCommand();
        }
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
    LoggingCallback logger_{};
    std::shared_ptr<StrictMock<SharedResourceEngineMock>> engine_{};
    const std::string service_identifier_{"test_identifier"};
    const ServiceProtocolConfig protocol_config_{service_identifier_, kMaxSendSize, kMaxReplySize, kMaxNotifySize};
    IClientFactory::ClientConfig client_config_{0, 1, false, false, false};
    ISharedResourceEngine::CommandCallback connect_command_callback_{};
    ISharedResourceEngine::CommandCallback endpoint_command_callback_{};
    ISharedResourceEngine::CommandCallback disconnect_command_callback_{};
    ISharedResourceEngine::CommandCallback send_queue_command_callback_{};
    ISharedResourceEngine::PosixEndpointEntry* posix_endpoint_{};
    std::atomic<bool> on_callback_thread_{false};  // only atomic for tsan and sleep synchronization
    std::thread background_thread_{};
    std::int32_t connection_delay_ms_{0};
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

    CatchImmediateConnectCommand();
    connection.Start(IClientConnection::StateCallback(), IClientConnection::NotifyCallback());
    EXPECT_EQ(connection.GetState(), State::kStarting);

    EXPECT_CALL(*engine_, TryOpenClientConnection(std::string_view{service_identifier_}))
        .WillOnce([this, &connection](auto&&) {
            StopConnectionInProgress(connection);
            return score::cpp::make_unexpected(
                score::os::Error::createFromErrno(EIO));  // this error code would be ignored
        });
    ExpectCleanUpOwner(connection);
    InvokeConnectCommand();

    EXPECT_EQ(connection.GetState(), State::kStopped);
    EXPECT_EQ(connection.GetStopReason(), StopReason::kUserRequested);
}

TEST_F(ClientConnectionTest, TryingToSyncConnectOnceStoppingWhileConnecting)
{
    client_config_.sync_first_connect = true;
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);

    EXPECT_CALL(*engine_, TryOpenClientConnection(std::string_view{service_identifier_}))
        .WillOnce([this, &connection](auto&&) {
            StopConnectionInProgress(connection);
            return score::cpp::make_unexpected(
                score::os::Error::createFromErrno(EIO));  // this error code would be ignored
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

TEST_F(ClientConnectionTest, TryingToSyncConnectOnceStoppingOnHardError)
{
    ::testing::Test::RecordProperty(
        "lobster-tracing",
        "MessagePassing.ClientConnectionMaintainsStateMachine MessagePassing.ClientConnectionStateCallbackInvocation");
    ::testing::Test::RecordProperty("given", "a synchronous first connection attempt fails with a terminal open error");
    client_config_.sync_first_connect = true;

    const auto error_list_to_test = {std::make_pair(EACCES, StopReason::kPermission),
                                     std::make_pair(EPIPE, StopReason::kIoError)};

    for (auto entry : error_list_to_test)
    {
        detail::ClientConnection connection(engine_, protocol_config_, client_config_);
        std::vector<State> callback_states;

        AtTryOpenCall_Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(entry.first)));
        CatchDisconnectCommand();
        ExpectCleanUpOwner(connection);

        ::testing::Test::RecordProperty("when", "Start schedules finalization and the engine executes that command");
        connection.Start(
            [this, &callback_states](State state) noexcept {
                callback_states.push_back(state);
                if (state == State::kStopped)
                {
                    EXPECT_TRUE(on_callback_thread_.load());
                }
            },
            {});
        EXPECT_EQ(connection.GetStopReason(), entry.second);
        InvokeDisconnectCommand();

        ::testing::Test::RecordProperty(
            "then", "one engine-context cleanup completes the ordered starting, stopping, and stopped callbacks");
        const std::vector<State> expected_states{State::kStarting, State::kStopping, State::kStopped};
        EXPECT_EQ(callback_states, expected_states);
        EXPECT_EQ(connection.GetState(), State::kStopped);
        EXPECT_EQ(connection.GetStopReason(), entry.second);
    }
}

TEST_F(ClientConnectionTest, SyncConnectFailureMayFinalizeBeforeStartReturns)
{
    ::testing::Test::RecordProperty(
        "lobster-tracing",
        "MessagePassing.ClientConnectionMaintainsStateMachine MessagePassing.ClientConnectionStateCallbackInvocation");
    ::testing::Test::RecordProperty("given", "synchronous connection failure with immediately executed engine cleanup");
    client_config_.sync_first_connect = true;
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    std::vector<State> callback_states;
    const auto caller_thread = std::this_thread::get_id();
    std::thread::id cleanup_thread;

    AtTryOpenCall_Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(EACCES)));
    EXPECT_CALL(*engine_, CleanUpOwner(&connection)).WillOnce([&](auto) {
        cleanup_thread = std::this_thread::get_id();
        EXPECT_NE(cleanup_thread, caller_thread);
        EXPECT_TRUE(on_callback_thread_.load());
    });
    EXPECT_CALL(*engine_, EnqueueCommand(_, ISharedResourceEngine::TimePoint{}, _, &connection))
        .WillOnce([this](auto&, auto, ISharedResourceEngine::CommandCallback callback, auto) {
            std::thread callback_thread([this, callback = std::move(callback)]() mutable {
                on_callback_thread_ = true;
                callback(ISharedResourceEngine::Clock::now());
                on_callback_thread_ = false;
            });
            callback_thread.join();
        });

    ::testing::Test::RecordProperty("when", "the engine finishes queued cleanup before Start returns");
    connection.Start(
        [this, &callback_states](State state) noexcept {
            callback_states.push_back(state);
            if (state == State::kStopped)
            {
                EXPECT_TRUE(on_callback_thread_.load());
            }
        },
        {});

    ::testing::Test::RecordProperty("then",
                                    "ordered callbacks and cleanup complete without requiring a stopping snapshot");
    const std::vector<State> expected_states{State::kStarting, State::kStopping, State::kStopped};
    EXPECT_EQ(callback_states, expected_states);
    EXPECT_NE(cleanup_thread, std::thread::id{});
    EXPECT_EQ(connection.GetState(), State::kStopped);
    EXPECT_EQ(connection.GetStopReason(), StopReason::kPermission);
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
        }
        else
        {
            AtTryOpenCall_Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(entry)));
            CatchTimedConnectCommand();
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

TEST_F(ClientConnectionTest, TryingToSyncConnectMultipleTimesStoppingOnPermissionError)
{
    client_config_.sync_first_connect = true;

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

    CatchImmediateConnectCommand();
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

TEST_F(ClientConnectionTest, SuccessfullyConnectingAtFirstAttemptWithDelayThenExplicitlyStopping)
{
    std::int32_t counter{0};
    logger_ = [&counter](LogSeverity severity, LogItems items) -> void {
        if ((severity == LogSeverity::kWarn) && (items.size() > 1) &&
            std::holds_alternative<std::string_view>(items[0]) &&
            std::get<std::string_view>(items[0]) == std::string_view{"Time exceeded by "})
        {
            ++counter;
        }
    };
    connection_delay_ms_ = 100;

    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    EXPECT_EQ(counter, 1);
    logger_ = LoggingCallback{};

    StopCurrentConnection(connection);
}

TEST_F(ClientConnectionTest, SuccessfullySyncConnectingAtFirstAttemptThenExplicitlyStopping)
{
    client_config_.sync_first_connect = true;
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

TEST_F(ClientConnectionTest, SuccessfullySyncConnectingAtFirstAttemptThenExplicitlyStoppingAsIfFromCallback)
{
    client_config_.sync_first_connect = true;
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    StopCurrentConnectionAsIfInCallback(connection);
}

TEST_F(ClientConnectionTest, ReadyStateCallbackCanStopConnection)
{
    ::testing::Test::RecordProperty(
        "lobster-tracing",
        "MessagePassing.ClientConnectionMaintainsStateMachine MessagePassing.ClientConnectionStateCallbackInvocation");
    ::testing::Test::RecordProperty("given", "connection whose ready-state callback requests Stop");
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    std::vector<State> callback_states;

    CatchImmediateConnectCommand();
    connection.Start(
        [&](State state) noexcept {
            callback_states.push_back(state);
            EXPECT_EQ(connection.GetState(), state);
            if (state == State::kReady)
            {
                connection.Stop();
                EXPECT_EQ(connection.GetState(), State::kStopped);
            }
        },
        {});

    ::testing::Test::RecordProperty("when",
                                    "the engine establishes the connection and invokes the ready-state callback");
    AtTryOpenCall_Return(kValidFd);
    CatchPosixEndpointRegistration();
    ExpectCleanUpOwner(connection);
    ExpectCloseFd();
    InvokeConnectCommand();

    ::testing::Test::RecordProperty("then",
                                    "stopping and stopped callbacks complete before the ready-state callback returns");
    const std::vector<State> expected_states{State::kStarting, State::kReady, State::kStopping, State::kStopped};
    EXPECT_EQ(callback_states, expected_states);
    EXPECT_EQ(connection.GetState(), State::kStopped);
    EXPECT_EQ(connection.GetStopReason(), StopReason::kUserRequested);
}

TEST_F(ClientConnectionTest, StoppedStateCallbackCanRestartConnection)
{
    ::testing::Test::RecordProperty(
        "lobster-tracing",
        "MessagePassing.ClientConnectionMaintainsStateMachine MessagePassing.ClientConnectionStateCallbackInvocation");
    ::testing::Test::RecordProperty("given", "ready connection whose first stopped-state callback requests Restart");
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    std::uint32_t stopped_callback_count{0};
    MakeSuccessfulConnection(connection, [&](State state) noexcept {
        if (state == State::kStopped)
        {
            ++stopped_callback_count;
            if (stopped_callback_count == 1U)
            {
                connection.Restart();
                EXPECT_EQ(connection.GetState(), State::kStarting);
            }
        }
    });

    ::testing::Test::RecordProperty("when", "queued stop cleanup invokes the stopped-state callback");
    CatchDisconnectCommand();
    connection.Stop();
    ExpectCleanUpOwner(connection);
    ExpectCloseFd();
    CatchImmediateConnectCommand();
    InvokeDisconnectCommand();

    ::testing::Test::RecordProperty(
        "then", "restart enters Starting before the stopped-state callback returns and can reconnect");
    EXPECT_EQ(stopped_callback_count, 1U);
    EXPECT_EQ(connection.GetState(), State::kStarting);
    EXPECT_EQ(connection.GetStopReason(), StopReason::kNone);
    AtTryOpenCall_Return(kValidFd);
    CatchPosixEndpointRegistration();
    InvokeConnectCommand();
    EXPECT_EQ(connection.GetState(), State::kReady);
    StopCurrentConnection(connection);
    EXPECT_EQ(stopped_callback_count, 2U);
}

TEST_F(ClientConnectionTest, StoppedStateCallbackKeepsOwnedCaptureAliveDuringDestruction)
{
    ::testing::Test::RecordProperty(
        "lobster-tracing",
        "MessagePassing.ClientConnectionMaintainsStateMachine MessagePassing.ClientConnectionStateCallbackInvocation");
    ::testing::Test::RecordProperty("given", "stopped-state callback owning a capture and destroying its connection");
    auto connection = std::make_unique<detail::ClientConnection>(engine_, protocol_config_, client_config_);
    auto captured_value = std::make_shared<std::uint32_t>(42U);
    const std::weak_ptr<std::uint32_t> capture_lifetime{captured_value};
    const auto destroy_connection = [&]() noexcept {
        connection.reset();
        EXPECT_FALSE(capture_lifetime.expired());
    };
    MakeSuccessfulConnection(*connection,
                             [&destroy_connection, captured_value = std::move(captured_value)](State state) noexcept {
                                 if (state == State::kStopped)
                                 {
                                     destroy_connection();
                                     EXPECT_EQ(*captured_value, 42U);
                                 }
                             });

    ::testing::Test::RecordProperty("when", "queued stop cleanup invokes the stopped-state callback");
    CatchDisconnectCommand();
    connection->Stop();
    ExpectCleanUpOwner(*connection);
    ExpectCloseFd();
    InvokeDisconnectCommand();

    ::testing::Test::RecordProperty(
        "then", "the connection is destroyed but its callback capture survives until callback return");
    EXPECT_FALSE(connection);
    EXPECT_TRUE(capture_lifetime.expired());
}

TEST_F(ClientConnectionTest, ConcurrentStopRequestsFinalizeOnce)
{
    ::testing::Test::RecordProperty(
        "lobster-tracing",
        "MessagePassing.ClientConnectionMaintainsStateMachine MessagePassing.ClientConnectionStateCallbackInvocation");
    ::testing::Test::RecordProperty("given", "ready connection with registered state callbacks");
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    std::atomic<std::uint32_t> stopping_callback_count{0};
    std::atomic<std::uint32_t> stopped_callback_count{0};
    MakeSuccessfulConnection(connection, [&](State state) noexcept {
        if (state == State::kStopping)
        {
            ++stopping_callback_count;
        }
        else if (state == State::kStopped)
        {
            ++stopped_callback_count;
        }
    });

    ::testing::Test::RecordProperty("when", "two application threads request Stop before queued cleanup executes");
    CatchDisconnectCommand();
    std::promise<void> release_stops;
    const auto start_stops = release_stops.get_future().share();
    std::thread first_stop([&]() {
        start_stops.wait();
        connection.Stop();
    });
    std::thread second_stop([&]() {
        start_stops.wait();
        connection.Stop();
    });
    release_stops.set_value();
    first_stop.join();
    second_stop.join();
    connection.Stop();

    EXPECT_EQ(connection.GetState(), State::kStopping);
    EXPECT_EQ(connection.GetStopReason(), StopReason::kUserRequested);
    EXPECT_EQ(stopping_callback_count.load(), 1U);
    EXPECT_EQ(stopped_callback_count.load(), 0U);

    ::testing::Test::RecordProperty(
        "then", "one cleanup and one stopped-state callback occur, and further Stop calls are no-ops");
    ExpectCleanUpOwner(connection);
    ExpectCloseFd();
    InvokeDisconnectCommand();
    connection.Stop();
    EXPECT_EQ(connection.GetState(), State::kStopped);
    EXPECT_EQ(connection.GetStopReason(), StopReason::kUserRequested);
    EXPECT_EQ(stopping_callback_count.load(), 1U);
    EXPECT_EQ(stopped_callback_count.load(), 1U);
}

TEST_F(ClientConnectionTest, SuccessfullyConnectingAtFirstAttemptThenFirstReadDisconnected)
{
    ::testing::Test::RecordProperty("lobster-tracing", "MessagePassing.OsIpcFaultHandling");
    ::testing::Test::RecordProperty("given", "client connection established successfully");
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    ::testing::Test::RecordProperty("when", "protocol receive returns ``EPIPE``");
    AtProtocolReceive_Return(0, score::cpp::make_unexpected(score::os::Error::createFromErrno(EPIPE)));
    AtPosixEndpointUnregistration_InvokeDisconnect();
    ExpectCleanUpOwner(connection);
    ExpectCloseFd();
    InvokeEndpointInput();
    ::testing::Test::RecordProperty("then", "connection stops with reason ``kClosedByPeer``");
    EXPECT_EQ(connection.GetState(), State::kStopped);
    EXPECT_EQ(connection.GetStopReason(), StopReason::kClosedByPeer);
}

TEST_F(ClientConnectionTest, SuccessfullyConnectingAtFirstAttemptThenSpuriousReadGetsIgnored)
{
    ::testing::Test::RecordProperty("lobster-tracing", "MessagePassing.OsIpcFaultHandling");
    ::testing::Test::RecordProperty("given", "client connection established successfully");
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    ::testing::Test::RecordProperty("when", "protocol receive returns ``EAGAIN`` (spurious wake-up)");
    AtProtocolReceive_Return(0, score::cpp::make_unexpected(score::os::Error::createFromErrno(EAGAIN)));
    InvokeEndpointInput();
    ::testing::Test::RecordProperty("then", "connection remains in ``kReady`` state");
    EXPECT_EQ(connection.GetState(), State::kReady);

    StopCurrentConnection(connection);
}

TEST_F(ClientConnectionTest, NotConnectedSendNotReady)
{
    ::testing::Test::RecordProperty("lobster-tracing", "MessagePassing.SendBufferArgumentValidation");
    ::testing::Test::RecordProperty("given", "client connection not yet connected");
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);

    std::array<std::uint8_t, kMaxSendSize> send_buffer;
    std::array<std::uint8_t, kMaxReplySize> reply_buffer;

    ::testing::Test::RecordProperty("when", "``Send``, ``SendWaitReply``, or ``SendWithCallback`` is called");
    auto send_result = connection.Send(send_buffer);
    ::testing::Test::RecordProperty("then", "each call returns ``EINVAL``");
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
    ::testing::Test::RecordProperty("Verifies",
                                    "MessagePassing.SendBufferArgumentValidation, MessagePassing.BE_MessageTooBig");
    ::testing::Test::RecordProperty("given", "client connection established with ``max_send_size = kMaxSendSize``");
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    std::array<std::uint8_t, kMaxSendSize + 1> send_buffer;
    std::array<std::uint8_t, kMaxReplySize> reply_buffer;

    ::testing::Test::RecordProperty("when", "any send variant is called with a buffer larger than ``kMaxSendSize``");
    auto send_result = connection.Send(send_buffer);
    ::testing::Test::RecordProperty("then", "each call returns ``EMSGSIZE``");
    EXPECT_FALSE(send_result);
    EXPECT_EQ(send_result.error().GetOsDependentErrorCode(), EMSGSIZE);

    auto send_wait_reply_result = connection.SendWaitReply(send_buffer, reply_buffer);
    EXPECT_FALSE(send_wait_reply_result);
    EXPECT_EQ(send_wait_reply_result.error().GetOsDependentErrorCode(), EMSGSIZE);

    auto send_with_callback_result = connection.SendWithCallback(send_buffer, IClientConnection::ReplyCallback{});
    EXPECT_FALSE(send_with_callback_result);
    EXPECT_EQ(send_with_callback_result.error().GetOsDependentErrorCode(), EMSGSIZE);

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

TEST_F(ClientConnectionTest, GivenTrulyAsyncWhenSendIsCalledItIsQueued)
{
    ::testing::Test::RecordProperty("lobster-tracing", "MessagePassing.BE_SendQueueExhausted");
    ::testing::Test::RecordProperty("given", "truly-async client connection with send queue capacity of 2");
    client_config_.max_queued_sends = 2;
    // Given Truly Async
    client_config_.truly_async = true;
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    CatchSendQueueCommand();

    ::testing::Test::RecordProperty("when", "three ``Send`` calls are made before the queue is flushed");
    std::array<std::uint8_t, kMaxSendSize> send_buffer;
    auto send_result = connection.Send(send_buffer);
    ::testing::Test::RecordProperty("then", "first two succeed, third returns ``ENOBUFS``");
    EXPECT_TRUE(send_result);

    send_result = connection.Send(send_buffer);
    EXPECT_TRUE(send_result);

    // third send shall fail as we only have two slots in the send queue
    send_result = connection.Send(send_buffer);
    EXPECT_FALSE(send_result);
    EXPECT_EQ(send_result.error().GetOsDependentErrorCode(), ENOBUFS);

    // both queued messages shall be sent by the same command, as they have no receive callbacks
    EXPECT_CALL(*engine_, SendProtocolMessage).Times(2);
    InvokeSendQueueCommand();

    // SendProtocolMessage will not be called for this one.
    CatchSendQueueCommand();
    // When Send() IsCalled then it is queued
    send_result = connection.Send(send_buffer);
    StopCurrentConnection(connection);
}

TEST_F(ClientConnectionTest, GivenQueuedSends_WhenStoppedAndRestarted_ExpectSendCapacityRestored)
{
    client_config_.max_queued_sends = 2;
    client_config_.max_async_replies = 1;
    client_config_.truly_async = true;
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    std::array<std::uint8_t, kMaxSendSize> send_buffer{};
    std::uint32_t reply_callback_count{0};
    for (std::uint32_t cycle = 0; cycle < 2; ++cycle)
    {
        if (cycle != 0)
        {
            CatchImmediateConnectCommand();
            connection.Restart();
            EXPECT_EQ(connection.GetState(), State::kStarting);

            AtTryOpenCall_Return(kValidFd);
            CatchPosixEndpointRegistration();
            InvokeConnectCommand();
            EXPECT_EQ(connection.GetState(), State::kReady);
        }

        CatchSendQueueCommand();
        EXPECT_TRUE(connection.Send(send_buffer));
        EXPECT_TRUE(connection.SendWithCallback(send_buffer, [&](auto message_expected) noexcept {
            EXPECT_FALSE(message_expected);
            if (!message_expected)
            {
                EXPECT_EQ(message_expected.error().GetOsDependentErrorCode(), EPIPE);
            }
            ++reply_callback_count;
        }));
        EXPECT_TRUE(connection.Send(send_buffer));

        const auto send_result = connection.Send(send_buffer);
        EXPECT_FALSE(send_result);
        if (!send_result)
        {
            EXPECT_EQ(send_result.error().GetOsDependentErrorCode(), ENOBUFS);
        }

        StopCurrentConnection(connection);
        EXPECT_EQ(reply_callback_count, cycle + 1);
    }
}

TEST_F(ClientConnectionTest, SendIsNotQueuedIfTrulyAsyncButNoSlots)
{
    ::testing::Test::RecordProperty("lobster-tracing", "MessagePassing.BE_SendQueueExhausted");
    ::testing::Test::RecordProperty("given", "truly-async client connection with send queue capacity of 0");
    // this is a meaningless case, but as we check for it in the implementation, we shall cover it
    client_config_.max_queued_sends = 0;
    client_config_.truly_async = true;
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    ::testing::Test::RecordProperty("when", "``Send`` is called");
    std::array<std::uint8_t, kMaxSendSize> send_buffer;
    auto send_result = connection.Send(send_buffer);
    ::testing::Test::RecordProperty("then", "call returns ``ENOBUFS``");
    EXPECT_FALSE(send_result);
    EXPECT_EQ(send_result.error().GetOsDependentErrorCode(), ENOBUFS);

    StopCurrentConnection(connection);
}

TEST_F(ClientConnectionTest, SendWaitReplyFailsWhenCalledInCallback)
{
    ::testing::Test::RecordProperty("lobster-tracing", "MessagePassing.SendBufferArgumentValidation");
    ::testing::Test::RecordProperty("given",
                                    "client connection established and a callback is executing on the callback thread");
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    std::array<std::uint8_t, kMaxSendSize> send_buffer;
    std::array<std::uint8_t, kMaxReplySize> reply_buffer;

    on_callback_thread_ = true;
    ::testing::Test::RecordProperty("when", "``SendWaitReply`` is called from within the callback");
    auto send_wait_reply_result = connection.SendWaitReply(send_buffer, reply_buffer);
    on_callback_thread_ = false;
    ::testing::Test::RecordProperty("then", "call returns ``EAGAIN``");
    EXPECT_FALSE(send_wait_reply_result);
    EXPECT_EQ(send_wait_reply_result.error().GetOsDependentErrorCode(), EAGAIN);

    StopCurrentConnection(connection);
}

TEST_F(ClientConnectionTest, SendWaitReplyFailsWhenCannotSendMessageDirectly)
{
    ::testing::Test::RecordProperty("lobster-tracing", "MessagePassing.OsIpcFaultHandling");
    ::testing::Test::RecordProperty("given", "client connection established");
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    std::array<std::uint8_t, kMaxSendSize> send_buffer;
    std::array<std::uint8_t, kMaxReplySize> reply_buffer;

    ::testing::Test::RecordProperty("when",
                                    "``SendWaitReply`` is called but ``SendProtocolMessage`` returns ``EPIPE``");
    EXPECT_CALL(*engine_, SendProtocolMessage)
        .WillOnce(Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(EPIPE))));

    auto send_wait_reply_result = connection.SendWaitReply(send_buffer, reply_buffer);
    ::testing::Test::RecordProperty("then", "``SendWaitReply`` returns ``EPIPE``");
    EXPECT_FALSE(send_wait_reply_result);
    EXPECT_EQ(send_wait_reply_result.error().GetOsDependentErrorCode(), EPIPE);

    StopCurrentConnection(connection);
}

TEST_F(ClientConnectionTest, SendWaitReplyFailsDirectlyButUnblocksQueue)
{
    ::testing::Test::RecordProperty("lobster-tracing", "MessagePassing.OsIpcFaultHandling");
    ::testing::Test::RecordProperty("given", "truly-async client connection with a message already queued");
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

    ::testing::Test::RecordProperty("when", "``SendWaitReply`` fails immediately with ``EPIPE``");
    auto send_wait_reply_result = connection.SendWaitReply(send_buffer, reply_buffer);
    ::testing::Test::RecordProperty("then", "queued send is unblocked and dispatched via the next queue command");
    EXPECT_FALSE(send_wait_reply_result);
    EXPECT_EQ(send_wait_reply_result.error().GetOsDependentErrorCode(), EPIPE);

    EXPECT_CALL(*engine_, SendProtocolMessage);  // will be called from unblocked queue
    InvokeSendQueueCommand();

    StopCurrentConnection(connection);
}

TEST_F(ClientConnectionTest, QueuedAdmissionsArmOnceAndPreservePendingReply)
{
    ::testing::Test::RecordProperty("lobster-tracing", "MessagePassing.OsIpcFaultHandling");
    ::testing::Test::RecordProperty("given", "a truly asynchronous connection with room for two queued messages");
    client_config_.truly_async = true;
    client_config_.max_queued_sends = 2;
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    std::array<std::uint8_t, kMaxSendSize> send_buffer{};
    std::array<std::uint8_t, kMaxReplySize> reply_buffer{};
    std::uint32_t callback_count{0};

    CatchSendQueueCommand();
    EXPECT_TRUE(connection.SendWithCallback(send_buffer, [&callback_count](auto message_expected) noexcept {
        ++callback_count;
        EXPECT_TRUE(message_expected.has_value());
    }));
    EXPECT_TRUE(connection.Send(send_buffer));

    ::testing::Test::RecordProperty("when",
                                    "messages queue behind the scheduled drain and then behind its pending request");
    EXPECT_CALL(*engine_, SendProtocolMessage(kValidFd, score::cpp::to_underlying(detail::ClientToServer::REQUEST), _))
        .WillOnce(Return(score::cpp::blank{}));
    InvokeSendQueueCommand();
    EXPECT_TRUE(connection.Send(send_buffer));
    EXPECT_EQ(callback_count, 0U);

    EXPECT_CALL(*engine_, SendProtocolMessage(kValidFd, score::cpp::to_underlying(detail::ClientToServer::SEND), _))
        .Times(2)
        .WillRepeatedly(Return(score::cpp::blank{}));
    AtProtocolReceive_Return(score::cpp::to_underlying(detail::ServerToClient::REPLY), reply_buffer);
    InvokeEndpointInput();
    EXPECT_EQ(callback_count, 1U);

    ::testing::Test::RecordProperty(
        "then", "queued messages share one drain command, the reply completes once, and idle admission can arm again");
    CatchSendQueueCommand();
    EXPECT_TRUE(connection.Send(send_buffer));
    EXPECT_CALL(*engine_, SendProtocolMessage(kValidFd, score::cpp::to_underlying(detail::ClientToServer::SEND), _))
        .WillOnce(Return(score::cpp::blank{}));
    InvokeSendQueueCommand();
    StopCurrentConnection(connection);
    EXPECT_EQ(callback_count, 1U);
}

TEST_F(ClientConnectionTest, SendWaitReplyDirectErrorPreservesQueuedRequestCallback)
{
    ::testing::Test::RecordProperty("lobster-tracing", "MessagePassing.OsIpcFaultHandling");
    ::testing::Test::RecordProperty("given",
                                    "a request is queued while a direct SendWaitReply transport is in progress");
    client_config_.truly_async = true;
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    std::array<std::uint8_t, kMaxSendSize> send_buffer{};
    std::array<std::uint8_t, kMaxReplySize> reply_buffer{};
    std::uint32_t callback_count{0};

    EXPECT_CALL(*engine_, SendProtocolMessage).WillOnce([&](auto&&...) {
        auto send_result = connection.SendWithCallback(send_buffer, [&callback_count](auto message_expected) noexcept {
            ++callback_count;
            EXPECT_TRUE(message_expected.has_value());
        });
        EXPECT_TRUE(send_result);
        EXPECT_EQ(callback_count, 0U);
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(EPIPE));
    });
    CatchSendQueueCommand();

    ::testing::Test::RecordProperty(
        "when", "the direct transport fails with EPIPE and the queued request later receives a reply");
    auto send_wait_reply_result = connection.SendWaitReply(send_buffer, reply_buffer);
    EXPECT_FALSE(send_wait_reply_result);
    EXPECT_EQ(send_wait_reply_result.error().GetOsDependentErrorCode(), EPIPE);
    EXPECT_EQ(callback_count, 0U);

    EXPECT_CALL(*engine_, SendProtocolMessage(kValidFd, score::cpp::to_underlying(detail::ClientToServer::REQUEST), _))
        .WillOnce(Return(score::cpp::blank{}));
    InvokeSendQueueCommand();
    EXPECT_EQ(callback_count, 0U);

    AtProtocolReceive_Return(score::cpp::to_underlying(detail::ServerToClient::REPLY), reply_buffer);
    InvokeEndpointInput();

    ::testing::Test::RecordProperty("then",
                                    "the queued request callback completes exactly once and is not cancelled on stop");
    EXPECT_EQ(callback_count, 1U);
    StopCurrentConnection(connection);
    EXPECT_EQ(callback_count, 1U);
}

TEST_F(ClientConnectionTest, SendWaitReplyStopDefersCloseAndRestartUntilTransportReturns)
{
    ::testing::Test::RecordProperty("lobster-tracing", "MessagePassing.OsIpcFaultHandling");
    ::testing::Test::RecordProperty("given", "a synchronous-first connection with a direct request in transport");
    client_config_.sync_first_connect = true;
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    std::array<std::uint8_t, kMaxSendSize> send_buffer{};
    std::array<std::uint8_t, kMaxReplySize> reply_buffer{};
    EXPECT_CALL(*engine_, SendProtocolMessage).WillOnce([&](auto&&...) {
        CatchDisconnectCommand();
        connection.Stop();
        ExpectCleanUpOwner(connection);
        InvokeDisconnectCommand();
        EXPECT_EQ(connection.GetState(), State::kStopping);

        if (connection.GetState() == State::kStopping)
        {
            connection.Restart();
            EXPECT_EQ(connection.GetState(), State::kStopping);
            EXPECT_FALSE(connection.Send(send_buffer));
            EXPECT_FALSE(connection.SendWithCallback(send_buffer, [](auto) noexcept {}));
            EXPECT_FALSE(connection.SendWaitReply(send_buffer, reply_buffer));
            CatchDisconnectCommand();
        }
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(EPIPE));
    });

    ::testing::Test::RecordProperty("when",
                                    "stopping and restarting are requested before the old transport returns EPIPE");
    auto result = connection.SendWaitReply(send_buffer, reply_buffer);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().GetOsDependentErrorCode(), EPIPE);

    ::testing::Test::RecordProperty("then", "closure and restart are deferred until old send termination completes");
    if (!disconnect_command_callback_.empty())
    {
        ExpectCloseFd();
        InvokeDisconnectCommand();
    }
    EXPECT_EQ(connection.GetState(), State::kStopped);

    AtTryOpenCall_Return(kValidFd);
    CatchEndpointRegistrationCommand();
    connection.Restart();
    CatchPosixEndpointRegistration();
    InvokeEndpointRegistrationCommand();
    EXPECT_EQ(connection.GetState(), State::kReady);
    StopCurrentConnection(connection);
}

TEST_F(ClientConnectionTest, DirectSendsAllowQueuedAdmissionWhileTransportIsDelayed)
{
    ::testing::Test::RecordProperty("lobster-tracing", "MessagePassing.OsIpcFaultHandling");
    ::testing::Test::RecordProperty("given", "a fully ordered direct send paused in transport");
    for (const bool with_reply : {false, true})
    {
        client_config_.fully_ordered = true;
        detail::ClientConnection connection(engine_, protocol_config_, client_config_);
        MakeSuccessfulConnection(connection);
        std::array<std::uint8_t, kMaxSendSize> send_buffer{};
        std::array<std::uint8_t, kMaxReplySize> reply_buffer{};
        std::promise<void> entered;
        std::promise<void> release;
        auto released = release.get_future();
        std::uint32_t callback_count{0};
        EXPECT_CALL(*engine_, SendProtocolMessage).WillOnce([&](auto&&...) {
            entered.set_value();
            released.wait();
            return score::cpp::blank{};
        });
        auto sending = std::async(std::launch::async, [&]() {
            if (with_reply)
            {
                return connection.SendWithCallback(send_buffer, [&](auto message_expected) {
                    EXPECT_TRUE(message_expected.has_value());
                    ++callback_count;
                });
            }
            return connection.Send(send_buffer);
        });
        entered.get_future().wait();

        ::testing::Test::RecordProperty("when",
                                        "another thread submits a send before the direct transport is released");
        auto queued = std::async(std::launch::async, [&]() {
            return connection.Send(send_buffer);
        });
        EXPECT_EQ(queued.wait_for(std::chrono::seconds{5}), std::future_status::ready);
        if (!with_reply)
        {
            CatchSendQueueCommand();
        }
        release.set_value();
        EXPECT_TRUE(sending.get());
        EXPECT_TRUE(queued.get());

        ::testing::Test::RecordProperty("then", "queue admission completes during IPC and ordered dispatch follows it");
        EXPECT_CALL(*engine_, SendProtocolMessage(kValidFd, score::cpp::to_underlying(detail::ClientToServer::SEND), _))
            .WillOnce(Return(score::cpp::blank{}));
        if (with_reply)
        {
            AtProtocolReceive_Return(score::cpp::to_underlying(detail::ServerToClient::REPLY), reply_buffer);
            InvokeEndpointInput();
            EXPECT_EQ(callback_count, 1U);
        }
        else
        {
            InvokeSendQueueCommand();
        }
        StopCurrentConnection(connection);
    }
}

TEST_F(ClientConnectionTest, SendStopDefersCloseUntilTransportReturns)
{
    ::testing::Test::RecordProperty("lobster-tracing", "MessagePassing.OsIpcFaultHandling");
    ::testing::Test::RecordProperty("given", "a direct one-way send in transport");
    for (const bool fully_ordered : {false, true})
    {
        client_config_.fully_ordered = fully_ordered;
        detail::ClientConnection connection(engine_, protocol_config_, client_config_);
        MakeSuccessfulConnection(connection);
        std::array<std::uint8_t, kMaxSendSize> send_buffer{};
        EXPECT_CALL(*engine_, SendProtocolMessage).WillOnce([&](auto&&...) {
            CatchDisconnectCommand();
            connection.Stop();
            ExpectCleanUpOwner(connection);
            InvokeDisconnectCommand();
            EXPECT_EQ(connection.GetState(), State::kStopping);
            EXPECT_FALSE(connection.Send(send_buffer));
            CatchDisconnectCommand();
            return score::cpp::blank{};
        });

        ::testing::Test::RecordProperty("when", "stop cleanup executes before the one-way transport returns");
        EXPECT_TRUE(connection.Send(send_buffer));
        EXPECT_EQ(connection.GetState(), State::kStopping);
        ExpectCloseFd();
        InvokeDisconnectCommand();
        ::testing::Test::RecordProperty("then", "the descriptor closes exactly once after the admitted send completes");
        EXPECT_EQ(connection.GetState(), State::kStopped);
    }
}

TEST_F(ClientConnectionTest, SendWithCallbackStopCompletesOnceBeforeTransportReturns)
{
    ::testing::Test::RecordProperty("lobster-tracing", "MessagePassing.OsIpcFaultHandling");
    ::testing::Test::RecordProperty("given", "a direct callback request in transport");
    for (const bool transport_succeeds : {false, true})
    {
        detail::ClientConnection connection(engine_, protocol_config_, client_config_);
        MakeSuccessfulConnection(connection);
        std::array<std::uint8_t, kMaxSendSize> send_buffer{};
        std::uint32_t callback_count{0};
        EXPECT_CALL(*engine_, SendProtocolMessage).WillOnce([&](auto&&...) {
            CatchDisconnectCommand();
            connection.Stop();
            ExpectCleanUpOwner(connection);
            InvokeDisconnectCommand();
            EXPECT_EQ(connection.GetState(), State::kStopping);
            EXPECT_EQ(callback_count, 1U);
            CatchDisconnectCommand();
            return transport_succeeds ? score::cpp::expected_blank<score::os::Error>{}
                                      : score::cpp::make_unexpected(score::os::Error::createFromErrno(EPIPE));
        });

        ::testing::Test::RecordProperty("when",
                                        "stop cancels the callback before a successful or failed transport return");
        EXPECT_TRUE(connection.SendWithCallback(send_buffer, [&](auto message_expected) {
            ++callback_count;
            ASSERT_FALSE(message_expected.has_value());
            EXPECT_EQ(message_expected.error().GetOsDependentErrorCode(), EPIPE);
        }));
        ExpectCloseFd();
        InvokeDisconnectCommand();
        ::testing::Test::RecordProperty("then",
                                        "the accepted request receives one cancellation and no late slot change");
        EXPECT_EQ(connection.GetState(), State::kStopped);
        EXPECT_EQ(callback_count, 1U);
    }
}

TEST_F(ClientConnectionTest, QueuedRequestStopPreservesActiveSendBufferAndPoolCapacity)
{
    ::testing::Test::RecordProperty("lobster-tracing", "MessagePassing.OsIpcFaultHandling");
    ::testing::Test::RecordProperty("given",
                                    "a queued request borrowing a send node while another send remains queued");
    client_config_.truly_async = true;
    client_config_.max_async_replies = 1;
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);
    std::array<std::uint8_t, kMaxSendSize> request_buffer{42U};
    std::array<std::uint8_t, kMaxSendSize> send_buffer{7U};
    std::uint32_t callback_count{0};
    CatchSendQueueCommand();
    EXPECT_TRUE(connection.SendWithCallback(request_buffer, [&](auto message_expected) {
        ++callback_count;
        ASSERT_FALSE(message_expected.has_value());
        EXPECT_EQ(message_expected.error().GetOsDependentErrorCode(), EPIPE);
    }));
    EXPECT_TRUE(connection.Send(send_buffer));
    EXPECT_CALL(*engine_, SendProtocolMessage).WillOnce([&](auto, auto, score::cpp::span<const std::uint8_t> message) {
        EXPECT_EQ(message.front(), 42U);
        ExpectCleanUpOwner(connection);
        connection.Stop();
        EXPECT_EQ(connection.GetState(), State::kStopping);
        EXPECT_EQ(callback_count, 1U);
        EXPECT_EQ(message.front(), 42U);
        CatchDisconnectCommand();
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(EPIPE));
    });

    ::testing::Test::RecordProperty("when", "stop cancels pending callbacks during queued request IPC");
    InvokeSendQueueCommand();
    ExpectCloseFd();
    InvokeDisconnectCommand();
    EXPECT_EQ(callback_count, 1U);

    CatchImmediateConnectCommand();
    connection.Restart();
    AtTryOpenCall_Return(kValidFd);
    CatchPosixEndpointRegistration();
    InvokeConnectCommand();
    CatchSendQueueCommand();
    EXPECT_TRUE(connection.Send(send_buffer));
    EXPECT_TRUE(connection.Send(send_buffer));
    EXPECT_FALSE(connection.Send(send_buffer));
    ::testing::Test::RecordProperty("then", "active data stays valid and both preallocated send nodes are reusable");
    StopCurrentConnection(connection);
}

TEST_F(ClientConnectionTest, SendWithCallbackEarlyReplyPreservesLaterRequestOnLateTransportError)
{
    ::testing::Test::RecordProperty("lobster-tracing", "MessagePassing.OsIpcFaultHandling");
    ::testing::Test::RecordProperty("given", "a direct request whose reply arrives before transport returns");
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);
    std::array<std::uint8_t, kMaxSendSize> send_buffer{};
    std::array<std::uint8_t, kMaxReplySize> reply_buffer{};
    struct ReplyContext
    {
        detail::ClientConnection& connection;
        const std::array<std::uint8_t, kMaxSendSize>& message;
        std::uint32_t first_count{0};
        std::uint32_t second_count{0};
        std::weak_ptr<std::uint32_t> callback_lifetime{};
    } context{connection, send_buffer};
    EXPECT_CALL(*engine_, SendProtocolMessage).WillOnce([&](auto&&...) {
        EXPECT_CALL(*engine_,
                    SendProtocolMessage(kValidFd, score::cpp::to_underlying(detail::ClientToServer::REQUEST), _))
            .WillOnce(Return(score::cpp::blank{}));
        AtProtocolReceive_Return(score::cpp::to_underlying(detail::ServerToClient::REPLY), reply_buffer);
        InvokeEndpointInput();
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(EPIPE));
    });

    ::testing::Test::RecordProperty("when",
                                    "the reply starts a later request and the old transport then reports EPIPE");
    EXPECT_TRUE(connection.SendWithCallback(send_buffer, [&context](auto message_expected) {
        EXPECT_TRUE(message_expected.has_value());
        ++context.first_count;
        auto captured_value = std::make_shared<std::uint32_t>(42U);
        context.callback_lifetime = captured_value;
        EXPECT_TRUE(context.connection.SendWithCallback(
            context.message,
            [&context, captured_value = std::move(captured_value)](auto next_message_expected) noexcept {
                ++context.second_count;
                EXPECT_EQ(*captured_value, 42U);
                ASSERT_FALSE(next_message_expected.has_value());
                EXPECT_EQ(next_message_expected.error().GetOsDependentErrorCode(), EPIPE);
            }));
    }));
    EXPECT_EQ(context.first_count, 1U);
    EXPECT_EQ(context.second_count, 0U);
    EXPECT_FALSE(context.callback_lifetime.expired());
    StopCurrentConnection(connection);
    ::testing::Test::RecordProperty("then",
                                    "old recovery preserves the later callback and its exactly-once completion");
    EXPECT_EQ(context.first_count, 1U);
    EXPECT_EQ(context.second_count, 1U);
    EXPECT_TRUE(context.callback_lifetime.expired());
}

TEST_F(ClientConnectionTest, CloseRunsOutsideSendMutexAndBeforeStoppedPublication)
{
    ::testing::Test::RecordProperty("lobster-tracing", "MessagePassing.OsIpcFaultHandling");
    ::testing::Test::RecordProperty("given", "a ready connection with a close operation that calls back into its API");
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    bool closed{false};
    MakeSuccessfulConnection(connection, [&](State state) {
        if (state == State::kStopped)
        {
            EXPECT_TRUE(closed);
        }
    });
    std::array<std::uint8_t, kMaxSendSize> send_buffer{};
    CatchDisconnectCommand();
    connection.Stop();
    ExpectCleanUpOwner(connection);
    EXPECT_CALL(*engine_, CloseClientConnection(kValidFd)).WillOnce([&](auto) {
        EXPECT_EQ(connection.GetState(), State::kStopping);
        EXPECT_FALSE(connection.Send(send_buffer));
        EXPECT_FALSE(connection.SendWithCallback(send_buffer, [](auto) noexcept {}));
        connection.Restart();
        EXPECT_EQ(connection.GetState(), State::kStopping);
        closed = true;
    });

    ::testing::Test::RecordProperty("when", "stop reaches descriptor closure");
    InvokeDisconnectCommand();
    ::testing::Test::RecordProperty("then", "send admission remains responsive and stopped is published after close");
    EXPECT_EQ(connection.GetState(), State::kStopped);
    EXPECT_TRUE(closed);
}

TEST_F(ClientConnectionTest, SendWaitReplyFailsWhenCannotQueueMessage)
{
    ::testing::Test::RecordProperty("lobster-tracing", "MessagePassing.BE_SendQueueExhausted");
    ::testing::Test::RecordProperty("given", "truly-async client connection with one send queue slot already occupied");
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

    ::testing::Test::RecordProperty("when", "``SendWaitReply`` is called");
    // second send shall fail as we only have one slot in the send queue
    auto send_wait_reply_result = connection.SendWaitReply(send_buffer, reply_buffer);
    ::testing::Test::RecordProperty("then", "call returns ``ENOBUFS``");
    EXPECT_FALSE(send_wait_reply_result);
    EXPECT_EQ(send_wait_reply_result.error().GetOsDependentErrorCode(), ENOBUFS);

    EXPECT_CALL(*engine_, SendProtocolMessage);
    InvokeSendQueueCommand();

    StopCurrentConnection(connection);
}

TEST_F(ClientConnectionTest, SendWaitReplyFailsWhenQueuedMessageCancels)
{
    ::testing::Test::RecordProperty("lobster-tracing", "MessagePassing.OsIpcFaultHandling");
    ::testing::Test::RecordProperty(
        "given", "truly-async client connection with a ``SendWaitReply`` blocked waiting for a reply");
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

    ::testing::Test::RecordProperty("when", "connection is stopped before the server sends the reply");
    // close connection without replying to SendWaitReply
    StopCurrentConnection(connection);
    ::testing::Test::RecordProperty("then", "``SendWaitReply`` returns ``EPIPE``");
}

TEST_F(ClientConnectionTest, SendWaitReplyFailsWhenReceiveTooLong)
{
    ::testing::Test::RecordProperty("Verifies",
                                    "MessagePassing.SendBufferArgumentValidation, MessagePassing.BE_MessageTooBig");
    ::testing::Test::RecordProperty("given", "client connection established with ``max_reply_size = kMaxReplySize``");
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    std::array<std::uint8_t, kMaxSendSize> send_buffer;
    std::array<std::uint8_t, kMaxReplySize> reply_buffer;

    ::testing::Test::RecordProperty("when", "server sends a reply larger than ``kMaxReplySize``");
    EXPECT_CALL(*engine_, SendProtocolMessage).WillOnce([&](auto&&...) {
        std::array<std::uint8_t, kMaxReplySize + 1> long_reply_buffer;
        AtProtocolReceive_Return(score::cpp::to_underlying(detail::ServerToClient::REPLY), long_reply_buffer);
        InvokeEndpointInput();
        return score::cpp::blank{};
    });

    auto send_wait_reply_result = connection.SendWaitReply(send_buffer, reply_buffer);
    ::testing::Test::RecordProperty("then", "``SendWaitReply`` returns ``EMSGSIZE``");
    EXPECT_FALSE(send_wait_reply_result);
    EXPECT_EQ(send_wait_reply_result.error().GetOsDependentErrorCode(), EMSGSIZE);

    StopCurrentConnection(connection);
}
TEST_F(ClientConnectionTest, SendWithCallbackFailsWhenCannotSendMessageDirectly)
{
    ::testing::Test::RecordProperty("lobster-tracing", "MessagePassing.OsIpcFaultHandling");
    ::testing::Test::RecordProperty("given", "client connection established");
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    std::array<std::uint8_t, kMaxSendSize> send_buffer;

    ::testing::Test::RecordProperty("when",
                                    "``SendWithCallback`` is called but ``SendProtocolMessage`` returns ``EPIPE``");
    EXPECT_CALL(*engine_, SendProtocolMessage)
        .WillOnce(Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(EPIPE))));

    auto send_with_callback_result = connection.SendWithCallback(send_buffer, IClientConnection::ReplyCallback{});
    ::testing::Test::RecordProperty("then", "``SendWithCallback`` returns ``EPIPE``");
    EXPECT_FALSE(send_with_callback_result);
    EXPECT_EQ(send_with_callback_result.error().GetOsDependentErrorCode(), EPIPE);

    StopCurrentConnection(connection);
}

TEST_F(ClientConnectionTest, SendWithCallbackFailsWhenCannotQueueMessage)
{
    ::testing::Test::RecordProperty("lobster-tracing", "MessagePassing.BE_SendQueueExhausted");
    ::testing::Test::RecordProperty("given", "client connection with no async reply slots and no send queue slots");
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

    ::testing::Test::RecordProperty(
        "when", "``SendWithCallback`` is called a second time while the first reply is still pending");
    // second send shall fail as we only have one slot in the send queue
    send_with_callback_result = connection.SendWithCallback(send_buffer, [](auto&&) {});
    ::testing::Test::RecordProperty("then", "second call returns ``ENOBUFS``");
    EXPECT_FALSE(send_with_callback_result);
    EXPECT_EQ(send_with_callback_result.error().GetOsDependentErrorCode(), ENOBUFS);

    StopCurrentConnection(connection);
}

TEST_F(ClientConnectionTest, SendWithCallbackFailsWhenCannotQueueMessageTrulyAsync)
{
    ::testing::Test::RecordProperty("lobster-tracing", "MessagePassing.BE_SendQueueExhausted");
    ::testing::Test::RecordProperty("given",
                                    "truly-async client connection with no async reply slots and no send queue slots");
    client_config_.truly_async = true;
    client_config_.max_async_replies = 0;
    client_config_.max_queued_sends = 0;
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    std::array<std::uint8_t, kMaxSendSize> send_buffer;

    ::testing::Test::RecordProperty("when", "``SendWithCallback`` is called");
    // first send shall fail
    auto send_with_callback_result = connection.SendWithCallback(send_buffer, [](auto&&) {});
    ::testing::Test::RecordProperty("then", "call immediately returns ``ENOBUFS``");
    EXPECT_EQ(send_with_callback_result.error().GetOsDependentErrorCode(), ENOBUFS);

    StopCurrentConnection(connection);
}

TEST_F(ClientConnectionTest, SendWithCallbackReportsFailureWhenQueuedSendFailsTrulyAsync)
{
    ::testing::Test::RecordProperty("lobster-tracing", "MessagePassing.OsIpcFaultHandling");
    ::testing::Test::RecordProperty(
        "given", "truly-async client connection with one async reply slot and a send queued for dispatch");
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

    ::testing::Test::RecordProperty("when", "the queued send dispatches but ``SendProtocolMessage`` returns ``EPIPE``");
    EXPECT_CALL(*engine_, SendProtocolMessage)
        .WillOnce(Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(EPIPE))));

    InvokeSendQueueCommand();
    ::testing::Test::RecordProperty("then", "the reply callback is invoked with ``EPIPE``");

    StopCurrentConnection(connection);
}

TEST_F(ClientConnectionTest, SendWithCallbackStillHasItsSlotBusyWhenQueuedSendHappensTrulyAsync)
{
    ::testing::Test::RecordProperty("lobster-tracing", "MessagePassing.BE_SendQueueExhausted");
    ::testing::Test::RecordProperty(
        "given", "truly-async client connection with one async reply slot busy with a pending ``SendWithCallback``");
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

    ::testing::Test::RecordProperty(
        "when", "queued send dispatches and a second ``SendWithCallback`` is attempted concurrently");
    EXPECT_CALL(*engine_, SendProtocolMessage).WillOnce([&](auto&&...) {
        // with max_async_replies = 1, that single internal slot queued data shall still be busy.
        // (if it's not the case, we may have data corruption if we try to send data concurrently)
        auto send_expected = connection.SendWithCallback(send_buffer, [](auto&&) {});
        EXPECT_FALSE(send_expected);
        EXPECT_EQ(send_expected.error().GetOsDependentErrorCode(), ENOBUFS);
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(EPIPE));
    });

    ::testing::Test::RecordProperty("then",
                                    "second ``SendWithCallback`` returns ``ENOBUFS`` preventing concurrent slot reuse");
    InvokeSendQueueCommand();

    StopCurrentConnection(connection);
}

TEST_F(ClientConnectionTest, QueuedSendsCancelIfConnectionClosed)
{
    ::testing::Test::RecordProperty("lobster-tracing", "MessagePassing.OsIpcFaultHandling");
    ::testing::Test::RecordProperty(
        "given", "truly-async client connection with queued sends and a ``SendWaitReply`` pending reply");
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

    ::testing::Test::RecordProperty("when", "connection is stopped before any reply is received");
    StopCurrentConnection(connection);
    ::testing::Test::RecordProperty("then", "all pending callbacks and ``SendWaitReply`` receive ``EPIPE``");
}

TEST_F(ClientConnectionTest, ConnectedStopsIfReceivesIoError)
{
    ::testing::Test::RecordProperty("lobster-tracing", "MessagePassing.OsIpcFaultHandling");
    ::testing::Test::RecordProperty("given", "client connection established");
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    ::testing::Test::RecordProperty("when", "protocol receive returns ``EIO``");
    AtProtocolReceive_Return(0, score::cpp::make_unexpected(score::os::Error::createFromErrno(EIO)));
    AtPosixEndpointUnregistration_InvokeDisconnect();
    ExpectCleanUpOwner(connection);
    ExpectCloseFd();
    InvokeEndpointInput();
    ::testing::Test::RecordProperty("then", "connection stops with reason ``kIoError``");
    EXPECT_EQ(connection.GetState(), State::kStopped);
    EXPECT_EQ(connection.GetStopReason(), StopReason::kIoError);
}

TEST_F(ClientConnectionTest, ConnectedStopsIfReceivesInvalidCode)
{
    ::testing::Test::RecordProperty("lobster-tracing", "MessagePassing.OsIpcFaultHandling");
    ::testing::Test::RecordProperty("given", "client connection established");
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    ::testing::Test::RecordProperty("when", "protocol receive returns an unrecognised message code");
    AtProtocolReceive_Return(score::cpp::to_underlying(detail::ServerToClient::NOTIFY) + 1, {});
    AtPosixEndpointUnregistration_InvokeDisconnect();
    ExpectCleanUpOwner(connection);
    ExpectCloseFd();
    InvokeEndpointInput();
    ::testing::Test::RecordProperty("then", "connection stops with reason ``kIoError``");
    EXPECT_EQ(connection.GetState(), State::kStopped);
    EXPECT_EQ(connection.GetStopReason(), StopReason::kIoError);
}

TEST_F(ClientConnectionTest, ConnectedStopsIfReceivesReplyWithoutSend)
{
    ::testing::Test::RecordProperty("lobster-tracing", "MessagePassing.OsIpcFaultHandling");
    ::testing::Test::RecordProperty("given", "client connection established with no pending ``SendWaitReply``");
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    ::testing::Test::RecordProperty("when", "a ``REPLY`` message arrives unexpectedly");
    AtProtocolReceive_Return(score::cpp::to_underlying(detail::ServerToClient::REPLY), {});
    AtPosixEndpointUnregistration_InvokeDisconnect();
    ExpectCleanUpOwner(connection);
    ExpectCloseFd();
    InvokeEndpointInput();
    ::testing::Test::RecordProperty("then", "connection stops with reason ``kIoError``");
    EXPECT_EQ(connection.GetState(), State::kStopped);
    EXPECT_EQ(connection.GetStopReason(), StopReason::kIoError);
}

TEST_F(ClientConnectionTest, ReplyDuringDrainMarkerStopsWithoutInvokingEmptyCallback)
{
    ::testing::Test::RecordProperty("lobster-tracing", "MessagePassing.OsIpcFaultHandling");
    ::testing::Test::RecordProperty("given", "an armed queue command with an empty reply-slot drain marker");
    client_config_.truly_async = true;
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);
    std::array<std::uint8_t, kMaxSendSize> send_buffer{};
    std::array<std::uint8_t, kMaxReplySize> reply_buffer{};
    CatchSendQueueCommand();
    EXPECT_TRUE(connection.Send(send_buffer));
    AtProtocolReceive_Return(score::cpp::to_underlying(detail::ServerToClient::REPLY), reply_buffer);
    AtPosixEndpointUnregistration_InvokeDisconnect();
    ExpectCleanUpOwner(connection);
    ExpectCloseFd();

    ::testing::Test::RecordProperty("when", "a reply is delivered before the queue command runs");
    InvokeEndpointInput();
    ::testing::Test::RecordProperty("then", "the unexpected reply stops the connection with an IO error");
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

TEST_F(ClientConnectionTest, ConnectedContinuesIfReceivesPingWithoutCallback)
{
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    MakeSuccessfulConnection(connection);

    InvokeEndpointPing();
    EXPECT_EQ(connection.GetState(), State::kReady);

    StopCurrentConnection(connection);
}

TEST_F(ClientConnectionTest, ConnectedReceivesPingInCallback)
{
    detail::ClientConnection connection(engine_, protocol_config_, client_config_);
    std::uint32_t call_counter{0U};
    MakeSuccessfulConnection(connection, IClientConnection::StateCallback{}, [&call_counter](auto&& message) noexcept {
        EXPECT_EQ(message.size(), 0U);
        ++call_counter;
    });

    InvokeEndpointPing();
    EXPECT_EQ(connection.GetState(), State::kReady);
    EXPECT_EQ(call_counter, 1U);

    StopCurrentConnection(connection);
}

}  // namespace
}  // namespace score::message_passing
