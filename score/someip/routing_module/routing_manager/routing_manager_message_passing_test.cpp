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

#include "score/message_passing/client_factory.h"
#include "score/message_passing/server_factory.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

namespace score::someip::routing_module
{
namespace
{

using routing_control_protocol::Message;
using routing_control_protocol::Opcode;
using score::message_passing::IClientConnection;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrictMock;

constexpr std::chrono::seconds kTimeout{5};

class WakeupCounter
{
  public:
    void Signal()
    {
        {
            const std::lock_guard<std::mutex> lock{mutex_};
            ++count_;
        }
        condition_.notify_all();
    }

    bool WaitFor(const std::size_t count)
    {
        std::unique_lock<std::mutex> lock{mutex_};
        return condition_.wait_for(lock, kTimeout, [this, count]() {
            return count_ >= count;
        });
    }

  private:
    std::mutex mutex_;
    std::condition_variable condition_;
    std::size_t count_{0U};
};

bool WaitUntilReady(const IClientConnection& client)
{
    const auto deadline = std::chrono::steady_clock::now() + kTimeout;
    while (client.GetState() != IClientConnection::State::kReady)
    {
        if (std::chrono::steady_clock::now() > deadline)
        {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    return true;
}

bool SendCommand(IClientConnection& client, const Message& command)
{
    std::array<std::uint8_t, routing_control_protocol::kMaxMessageSize> buffer{};
    const auto size = routing_control_protocol::Encode(command, buffer);
    return size.has_value() &&
           client.Send(score::cpp::span<const std::uint8_t>{buffer.data(), size.value()}).has_value();
}

Message AddRemoteService(const std::uint16_t service)
{
    Message message{};
    message.opcode = Opcode::kAddRemoteService;
    message.service = service;
    message.instance = 0x0001U;
    message.local_port = 30501U;
    message.remote_ip = routing_control_protocol::Ipv4Address{10U, 0U, 3U, 2U};
    message.remote_port = 30502U;
    return message;
}

TEST(RoutingManagerMessagePassingTest, AppliesCommandsReceivedOverMessagePassing)
{
    ::testing::Test::RecordProperty("lobster-tracing",
                                    "RoutingModule.CR_RoutingModule_ControlInterface,"
                                    "RoutingModule.CR_RoutingModule_ControlOrdering,"
                                    "RoutingModule.CR_RoutingModule_ControllerLoss");
    ::testing::Test::RecordProperty("given",
                                    "a started routing manager and an SD daemon client using score::message_passing");
    ::testing::Test::RecordProperty("when",
                                    "the client sends two routing commands, the commands are processed, and the "
                                    "client then disconnects");
    ::testing::Test::RecordProperty("then",
                                    "both commands are applied in order, and all routing state is removed after the "
                                    "disconnect");

    StrictMock<RoutingCommandHandlerMock> handler{};
    WakeupCounter wakeups{};
    score::message_passing::ServerFactory server_factory{};
    RoutingManager manager{server_factory, handler, [&wakeups]() {
                               wakeups.Signal();
                           }};
    ASSERT_TRUE(manager.Start().has_value());

    score::message_passing::ClientFactory client_factory{};
    const score::message_passing::ServiceProtocolConfig protocol{
        routing_control_protocol::kServiceIdentifier, routing_control_protocol::kMaxMessageSize, 0U, 0U};
    auto client = client_factory.Create(
        protocol, score::message_passing::IClientFactory::ClientConfig{0U, 8U, false, false, true});
    ASSERT_NE(client, nullptr);
    client->Start(IClientConnection::StateCallback{}, IClientConnection::NotifyCallback{});
    ASSERT_TRUE(WaitUntilReady(*client));

    ASSERT_TRUE(SendCommand(*client, AddRemoteService(0x1000U)));
    ASSERT_TRUE(SendCommand(*client, AddRemoteService(0x1001U)));
    ASSERT_TRUE(wakeups.WaitFor(2U));
    {
        InSequence sequence{};
        EXPECT_CALL(handler, AddRemoteService(Field(&Message::service, 0x1000U)))
            .WillOnce(Return(CommandResult::kApplied));
        EXPECT_CALL(handler, AddRemoteService(Field(&Message::service, 0x1001U)))
            .WillOnce(Return(CommandResult::kApplied));
    }
    manager.ProcessPendingCommands();
    ::testing::Mock::VerifyAndClearExpectations(&handler);

    client->Stop();
    client.reset();
    ASSERT_TRUE(wakeups.WaitFor(3U));
    EXPECT_CALL(handler, RemoveAll());
    manager.ProcessPendingCommands();
    manager.Stop();
}

}  // namespace
}  // namespace score::someip::routing_module
