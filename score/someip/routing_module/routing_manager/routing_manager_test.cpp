/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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
#include "score/someip/routing_module/routing_manager/routing_manager.h"

#include "score/someip/routing_module/routing_manager/routing_command_handler_mock.h"

#include "score/message_passing/mock/server_connection_mock.h"
#include "score/message_passing/mock/server_factory_mock.h"
#include "score/message_passing/mock/server_mock.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <cerrno>
#include <cstdint>
#include <utility>
#include <vector>

namespace score::someip::routing_module
{
namespace
{

using routing_control_protocol::Message;
using routing_control_protocol::Opcode;
using score::message_passing::ClientIdentity;
using score::message_passing::ConnectCallback;
using score::message_passing::DisconnectCallback;
using score::message_passing::MessageCallback;
using score::message_passing::ServerConnectionMock;
using score::message_passing::ServerFactoryMock;
using score::message_passing::ServerMock;
using score::message_passing::UserData;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

void Trace(const char* const requirements, const char* const given, const char* const when, const char* const then)
{
    ::testing::Test::RecordProperty("lobster-tracing", requirements);
    ::testing::Test::RecordProperty("given", given);
    ::testing::Test::RecordProperty("when", when);
    ::testing::Test::RecordProperty("then", then);
}

auto IsServiceInstance(const std::uint16_t service, const std::uint16_t instance)
{
    return AllOf(Field(&ServiceInstance::service, service), Field(&ServiceInstance::instance, instance));
}

auto IsEndpoint(const routing_control_protocol::Ipv4Address address, const std::uint16_t port)
{
    return AllOf(Field(&Endpoint::address, address), Field(&Endpoint::port, port));
}

Message WithServiceInstance(const Opcode opcode, const std::uint16_t service)
{
    Message message{};
    message.opcode = opcode;
    message.service = service;
    message.instance = 0x0001U;
    return message;
}

Message WithEvent(const Opcode opcode, const std::uint16_t service, const std::uint16_t event)
{
    Message message = WithServiceInstance(opcode, service);
    message.event = event;
    return message;
}

Message RemoteSubscription(const Opcode opcode)
{
    Message message = WithEvent(opcode, 0x3000U, 0x8003U);
    message.remote_ip = routing_control_protocol::Ipv4Address{192U, 168U, 0U, 7U};
    message.remote_port = 30721U;
    return message;
}

Message AddRemoteService(const std::uint16_t service)
{
    Message message = WithServiceInstance(Opcode::kAddRemoteService, service);
    message.local_port = 30501U;
    message.remote_ip = routing_control_protocol::Ipv4Address{10U, 0U, 3U, 2U};
    message.remote_port = 30502U;
    message.events = routing_control_protocol::EventIds{0x8001U};
    return message;
}

Message AddLocalService()
{
    Message message = WithServiceInstance(Opcode::kAddLocalService, 0x2000U);
    message.local_port = 30720U;
    message.major = 1U;
    message.events = routing_control_protocol::EventIds{0x8002U};
    message.fields = routing_control_protocol::EventIds{0x8002U};
    return message;
}

class RoutingManagerTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        ON_CALL(*server_, StartListening(_, _, _, _))
            .WillByDefault([this](ConnectCallback connect,
                                  DisconnectCallback disconnect,
                                  MessageCallback command,
                                  MessageCallback request) -> score::cpp::expected_blank<score::os::Error> {
                connect_ = std::move(connect);
                disconnect_ = std::move(disconnect);
                command_ = std::move(command);
                request_ = std::move(request);
                return {};
            });
        server_mock_ = server_.get();
        ON_CALL(server_factory_, Create(_, _)).WillByDefault(Return(::testing::ByMove(std::move(server_))));
        ON_CALL(daemon_, GetClientIdentity()).WillByDefault(::testing::ReturnRef(daemon_identity_));
        ON_CALL(daemon_, GetUserData()).WillByDefault(::testing::ReturnRef(daemon_user_data_));
        ON_CALL(other_client_, GetClientIdentity()).WillByDefault(::testing::ReturnRef(other_identity_));
        ON_CALL(other_client_, GetUserData()).WillByDefault(::testing::ReturnRef(other_user_data_));
    }

    void StartAndConnectDaemon()
    {
        ASSERT_TRUE(manager_.Start().has_value());
        ConnectDaemon();
    }

    void ConnectDaemon()
    {
        auto user_data = connect_(daemon_);
        ASSERT_TRUE(user_data.has_value());
        daemon_user_data_ = std::move(user_data).value();
    }

    void Send(const Message& message)
    {
        std::array<std::uint8_t, routing_control_protocol::kMaxMessageSize> buffer{};
        const auto size = routing_control_protocol::Encode(message, buffer);
        ASSERT_TRUE(size.has_value());
        EXPECT_TRUE(command_(daemon_, score::cpp::span<const std::uint8_t>{buffer.data(), size.value()}).has_value());
    }

    NiceMock<ServerFactoryMock> server_factory_{};
    score::cpp::pmr::unique_ptr<NiceMock<ServerMock>> server_{
        score::cpp::pmr::make_unique<NiceMock<ServerMock>>(score::cpp::pmr::get_default_resource())};
    NiceMock<ServerMock>* server_mock_{nullptr};
    StrictMock<RoutingCommandHandlerMock> handler_{};
    NiceMock<ServerConnectionMock> daemon_{};
    NiceMock<ServerConnectionMock> other_client_{};
    ClientIdentity daemon_identity_{100, 1000U, 1000U};
    ClientIdentity other_identity_{200, 2000U, 2000U};
    UserData daemon_user_data_{};
    UserData other_user_data_{};

    ConnectCallback connect_{};
    DisconnectCallback disconnect_{};
    MessageCallback command_{};
    MessageCallback request_{};
    std::size_t wakeups_{0U};

    RoutingManager manager_{server_factory_, handler_, [this]() {
                                ++wakeups_;
                            }};
};

TEST_F(RoutingManagerTest, StartsRoutingCommandServer)
{
    Trace("RoutingModule.CR_RoutingModule_ControlInterface",
          "a routing manager",
          "it is started",
          "it listens on the routing control protocol service identifier for routing commands");

    EXPECT_CALL(server_factory_,
                Create(AllOf(Field(&score::message_passing::ServiceProtocolConfig::identifier,
                                   routing_control_protocol::kServiceIdentifier),
                             Field(&score::message_passing::ServiceProtocolConfig::max_send_size,
                                   routing_control_protocol::kMaxMessageSize)),
                       _));
    EXPECT_CALL(*server_mock_, StartListening(_, _, _, _));

    EXPECT_TRUE(manager_.Start().has_value());
}

TEST_F(RoutingManagerTest, StartFailsWhenServerCreationFails)
{
    Trace("RoutingModule.CR_RoutingModule_ControlInterface",
          "a routing command server factory that cannot create a server",
          "the routing manager is started",
          "starting fails with an out-of-memory error");
    EXPECT_CALL(server_factory_, Create(_, _)).WillOnce(Return(nullptr));

    const auto result = manager_.Start();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().GetOsDependentErrorCode(), ENOMEM);
}

TEST_F(RoutingManagerTest, StartFailsWhenListeningFails)
{
    Trace("RoutingModule.CR_RoutingModule_ControlInterface",
          "a routing command server that cannot listen",
          "the routing manager is started",
          "starting fails with the server's error");

    EXPECT_CALL(*server_mock_, StartListening(_, _, _, _))
        .WillOnce(Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(EACCES))));

    const auto result = manager_.Start();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), score::os::Error::Code::kPermissionDenied);
}

TEST_F(RoutingManagerTest, StopStopsListening)
{
    Trace("RoutingModule.CR_RoutingModule_ControlInterface",
          "a started routing manager",
          "it is stopped",
          "it stops listening for routing commands");
    ASSERT_TRUE(manager_.Start().has_value());

    EXPECT_CALL(*server_mock_, StopListening());

    manager_.Stop();
}

TEST_F(RoutingManagerTest, DispatchesEveryRoutingCommandToTheHandler)
{
    Trace("RoutingModule.CR_RoutingModule_ControlInterface",
          "a connected SD daemon",
          "it sends one routing command of each opcode and the pending commands are processed",
          "each command is passed with its parameters to the matching routing command handler operation");
    StartAndConnectDaemon();
    const routing_control_protocol::Ipv4Address subscriber{192U, 168U, 0U, 7U};

    {
        InSequence sequence{};
        EXPECT_CALL(handler_, AddRemoteService(Field(&Message::service, 0x1000U)))
            .WillOnce(Return(CommandResult::kApplied));
        EXPECT_CALL(handler_, RemoveRemoteService(IsServiceInstance(0x1000U, 0x0001U)))
            .WillOnce(Return(CommandResult::kApplied));
        EXPECT_CALL(handler_, AddLocalSubscription(IsServiceInstance(0x1000U, 0x0001U), 0x8001U))
            .WillOnce(Return(CommandResult::kApplied));
        EXPECT_CALL(handler_, RemoveLocalSubscription(IsServiceInstance(0x1000U, 0x0001U), 0x8001U))
            .WillOnce(Return(CommandResult::kApplied));
        EXPECT_CALL(handler_, AddLocalService(Field(&Message::fields, routing_control_protocol::EventIds{0x8002U})))
            .WillOnce(Return(CommandResult::kApplied));
        EXPECT_CALL(handler_, RemoveLocalService(IsServiceInstance(0x2000U, 0x0001U)))
            .WillOnce(Return(CommandResult::kApplied));
        EXPECT_CALL(handler_,
                    AddRemoteSubscription(IsServiceInstance(0x3000U, 0x0001U), 0x8003U, IsEndpoint(subscriber, 30721U)))
            .WillOnce(Return(CommandResult::kApplied));
        EXPECT_CALL(
            handler_,
            RemoveRemoteSubscription(IsServiceInstance(0x3000U, 0x0001U), 0x8003U, IsEndpoint(subscriber, 30721U)))
            .WillOnce(Return(CommandResult::kApplied));
        EXPECT_CALL(handler_, RemoveAll());
    }

    Send(AddRemoteService(0x1000U));
    Send(WithServiceInstance(Opcode::kRemoveRemoteService, 0x1000U));
    Send(WithEvent(Opcode::kAddLocalSubscription, 0x1000U, 0x8001U));
    Send(WithEvent(Opcode::kRemoveLocalSubscription, 0x1000U, 0x8001U));
    Send(AddLocalService());
    Send(WithServiceInstance(Opcode::kRemoveLocalService, 0x2000U));
    Send(RemoteSubscription(Opcode::kAddRemoteSubscription));
    Send(RemoteSubscription(Opcode::kRemoveRemoteSubscription));
    Send(Message{Opcode::kShutdown});
    manager_.ProcessPendingCommands();
}

TEST_F(RoutingManagerTest, AppliesCommandsOnlyWhenProcessedInArrivalOrder)
{
    Trace("RoutingModule.CR_RoutingModule_ControlOrdering",
          "a connected SD daemon that sends three routing commands",
          "the commands arrive, the wakeup is signaled and the pending commands are processed",
          "no command is applied before processing, and processing applies them in arrival order");
    StartAndConnectDaemon();

    Send(AddRemoteService(0x1001U));
    Send(AddRemoteService(0x1002U));
    Send(AddRemoteService(0x1003U));

    EXPECT_EQ(wakeups_, 3U);
    ::testing::Mock::VerifyAndClearExpectations(&handler_);
    InSequence sequence{};
    EXPECT_CALL(handler_, AddRemoteService(Field(&Message::service, 0x1001U)))
        .WillOnce(Return(CommandResult::kApplied));
    EXPECT_CALL(handler_, AddRemoteService(Field(&Message::service, 0x1002U)))
        .WillOnce(Return(CommandResult::kApplied));
    EXPECT_CALL(handler_, AddRemoteService(Field(&Message::service, 0x1003U)))
        .WillOnce(Return(CommandResult::kApplied));
    manager_.ProcessPendingCommands();
}

TEST_F(RoutingManagerTest, DiscardsInvalidCommandAndKeepsTheConnection)
{
    Trace("RoutingModule.CR_RoutingModule_InvalidControl",
          "a connected SD daemon",
          "it sends a malformed routing command followed by a valid one",
          "the malformed command is discarded without error to the connection and only the valid one is applied");
    StartAndConnectDaemon();
    const std::array<std::uint8_t, 3U> malformed{0x01U, 0x7FU, 0x00U};

    EXPECT_TRUE(
        command_(daemon_, score::cpp::span<const std::uint8_t>{malformed.data(), malformed.size()}).has_value());
    Send(AddRemoteService(0x1000U));

    EXPECT_CALL(handler_, AddRemoteService(Field(&Message::service, 0x1000U)))
        .WillOnce(Return(CommandResult::kApplied));
    manager_.ProcessPendingCommands();
}

TEST_F(RoutingManagerTest, RejectedCommandDoesNotStopProcessing)
{
    Trace("RoutingModule.CR_RoutingModule_InvalidControl",
          "a routing command handler that rejects the first of two routing commands",
          "the pending commands are processed",
          "the second command is applied as well");
    StartAndConnectDaemon();
    Send(AddRemoteService(0x1000U));
    Send(AddRemoteService(0x1001U));

    InSequence sequence{};
    EXPECT_CALL(handler_, AddRemoteService(Field(&Message::service, 0x1000U)))
        .WillOnce(Return(CommandResult::kRejected));
    EXPECT_CALL(handler_, AddRemoteService(Field(&Message::service, 0x1001U)))
        .WillOnce(Return(CommandResult::kApplied));
    manager_.ProcessPendingCommands();
}

TEST_F(RoutingManagerTest, DiscardsCommandsWhileTheQueueIsFull)
{
    Trace("RoutingModule.CR_RoutingModule_ControlQueueOverflow",
          "a connected SD daemon and a command queue that is not processed",
          "the daemon sends three routing commands more than the queue holds, and one more after processing",
          "the three surplus commands are discarded without error to the connection, all others are applied");
    StartAndConnectDaemon();
    for (std::uint16_t index = 0U; index < (RoutingManager::kCommandQueueCapacity + 3U); ++index)
    {
        Send(AddRemoteService(index));
    }

    EXPECT_CALL(handler_, AddRemoteService(_))
        .Times(static_cast<int>(RoutingManager::kCommandQueueCapacity))
        .WillRepeatedly(Return(CommandResult::kApplied));
    EXPECT_CALL(
        handler_,
        AddRemoteService(Field(&Message::service, static_cast<std::uint16_t>(RoutingManager::kCommandQueueCapacity))))
        .Times(0);
    manager_.ProcessPendingCommands();
    ::testing::Mock::VerifyAndClearExpectations(&handler_);

    Send(AddRemoteService(0xF000U));
    EXPECT_CALL(handler_, AddRemoteService(Field(&Message::service, 0xF000U)))
        .WillOnce(Return(CommandResult::kApplied));
    manager_.ProcessPendingCommands();
}

TEST_F(RoutingManagerTest, RejectsSecondClientWhileTheDaemonIsConnected)
{
    Trace("RoutingModule.CR_RoutingModule_SingleController",
          "a connected SD daemon",
          "a second client connects and later disconnects",
          "the second client is rejected and its disconnect leaves the routing state unchanged");
    StartAndConnectDaemon();

    const auto second = connect_(other_client_);
    other_user_data_ = std::uintptr_t{2U};
    disconnect_(other_client_);

    ASSERT_FALSE(second.has_value());
    EXPECT_EQ(second.error(), score::os::Error::Code::kDeviceOrResourceBusy);
    EXPECT_CALL(handler_, RemoveAll()).Times(0);
    manager_.ProcessPendingCommands();
}

TEST_F(RoutingManagerTest, IgnoresDisconnectFromClientWithoutAcceptedMarker)
{
    Trace("RoutingModule.CR_RoutingModule_SingleController",
          "a connected SD daemon and a second client with no accepted user-data marker",
          "the second client is rejected and then disconnects",
          "the rejected client's disconnect does not remove routing state");
    StartAndConnectDaemon();

    const auto second = connect_(other_client_);
    ASSERT_FALSE(second.has_value());
    EXPECT_CALL(handler_, RemoveAll()).Times(0);

    disconnect_(other_client_);
    manager_.ProcessPendingCommands();
}

TEST_F(RoutingManagerTest, AcceptsNewDaemonAfterDisconnect)
{
    Trace("RoutingModule.CR_RoutingModule_SingleController",
          "an SD daemon that connected and disconnected",
          "a client connects",
          "the client is accepted");
    StartAndConnectDaemon();
    disconnect_(daemon_);

    const auto next = connect_(other_client_);

    EXPECT_TRUE(next.has_value());
    EXPECT_CALL(handler_, RemoveAll());
    manager_.ProcessPendingCommands();
}

TEST_F(RoutingManagerTest, RemovesAllRoutingStateWhenTheDaemonDisconnects)
{
    Trace("RoutingModule.CR_RoutingModule_ControllerLoss",
          "a connected SD daemon with two queued routing commands",
          "the daemon disconnects without announcing its shutdown and the pending commands are processed",
          "the queued commands are discarded and all routing state is removed");
    StartAndConnectDaemon();
    Send(AddRemoteService(0x1000U));
    Send(AddRemoteService(0x1001U));

    disconnect_(daemon_);

    EXPECT_EQ(wakeups_, 3U);
    EXPECT_CALL(handler_, RemoveAll());
    manager_.ProcessPendingCommands();
}

TEST_F(RoutingManagerTest, CommandsOfReconnectedDaemonFollowTheRemoval)
{
    Trace("RoutingModule.CR_RoutingModule_ControllerLoss,RoutingModule.CR_RoutingModule_ControlOrdering",
          "an SD daemon that disconnected and reconnected before the pending commands are processed",
          "the reconnected daemon sends a routing command and the pending commands are processed",
          "all routing state is removed before the new command is applied");
    StartAndConnectDaemon();
    disconnect_(daemon_);
    ConnectDaemon();
    Send(AddRemoteService(0x1000U));

    InSequence sequence{};
    EXPECT_CALL(handler_, RemoveAll());
    EXPECT_CALL(handler_, AddRemoteService(Field(&Message::service, 0x1000U)))
        .WillOnce(Return(CommandResult::kApplied));
    manager_.ProcessPendingCommands();
}

TEST_F(RoutingManagerTest, ReconnectDuringProcessingKeepsRemovalBeforeNewCommands)
{
    Trace("RoutingModule.CR_RoutingModule_ControllerLoss,RoutingModule.CR_RoutingModule_ControlOrdering",
          "pending commands being processed, while the SD daemon disconnects, reconnects and sends a new command "
          "during the processing of the first command",
          "processing continues",
          "the remaining old command is discarded, and all routing state is removed before the new command is applied");
    StartAndConnectDaemon();
    Send(AddRemoteService(0x1000U));
    Send(AddRemoteService(0x1001U));

    InSequence sequence{};
    EXPECT_CALL(handler_, AddRemoteService(Field(&Message::service, 0x1000U))).WillOnce([this](const Message&) {
        disconnect_(daemon_);
        ConnectDaemon();
        Send(AddRemoteService(0x2000U));
        return CommandResult::kApplied;
    });
    EXPECT_CALL(handler_, RemoveAll());
    EXPECT_CALL(handler_, AddRemoteService(Field(&Message::service, 0x2000U)))
        .WillOnce(Return(CommandResult::kApplied));
    manager_.ProcessPendingCommands();
}

TEST_F(RoutingManagerTest, AnnouncedShutdownRemovesAllRoutingState)
{
    Trace("RoutingModule.CR_RoutingModule_ControllerShutdown",
          "a connected SD daemon with a queued routing command",
          "the daemon announces its shutdown, the pending commands are processed and the daemon disconnects",
          "the queued command is applied before all routing state is removed");
    StartAndConnectDaemon();
    Send(AddRemoteService(0x1000U));
    Send(Message{Opcode::kShutdown});

    {
        InSequence sequence{};
        EXPECT_CALL(handler_, AddRemoteService(Field(&Message::service, 0x1000U)))
            .WillOnce(Return(CommandResult::kApplied));
        EXPECT_CALL(handler_, RemoveAll()).Times(2);
    }
    manager_.ProcessPendingCommands();
    disconnect_(daemon_);
    manager_.ProcessPendingCommands();
}

TEST_F(RoutingManagerTest, RejectsCommandsThatRequestAReply)
{
    Trace("RoutingModule.CR_RoutingModule_ControlInterface",
          "a connected SD daemon",
          "it sends a valid routing command as a request with reply",
          "the request is rejected and no command is applied");
    StartAndConnectDaemon();
    std::array<std::uint8_t, routing_control_protocol::kMaxMessageSize> buffer{};
    const auto size = routing_control_protocol::Encode(AddRemoteService(0x1000U), buffer);
    ASSERT_TRUE(size.has_value());

    const auto result = request_(daemon_, score::cpp::span<const std::uint8_t>{buffer.data(), size.value()});

    EXPECT_FALSE(result.has_value());
    manager_.ProcessPendingCommands();
}

}  // namespace
}  // namespace score::someip::routing_module
