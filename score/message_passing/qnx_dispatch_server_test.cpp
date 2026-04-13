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

#include "score/message_passing/qnx_dispatch/qnx_dispatch_server_factory.h"

#include "score/message_passing/i_server_connection.h"
#include "score/message_passing/qnx_dispatch/qnx_resource_path.h"

#include <fcntl.h>

namespace score
{
namespace message_passing
{
namespace
{

using namespace ::testing;

TEST(QnxDispatchServerTest, NonRunningServers)
{
    QnxDispatchServerFactory factory{};
    {
        IServerFactory::ServerConfig server_config{};
        ServiceProtocolConfig protocol_config{};
        auto server = factory.Create(protocol_config, server_config);
        EXPECT_TRUE(server);
    }
    {
        IServerFactory::ServerConfig server_config{};
        ServiceProtocolConfig protocol_config{};
        auto server = factory.Create(protocol_config, server_config);
        EXPECT_TRUE(server);
        server->StopListening();
    }
}

TEST(QnxDispatchServerTest, RunningServersWithNoConnections)
{
    QnxDispatchServerFactory factory{};
    IServerFactory::ServerConfig server_config{};
    std::string test_prefix{"test_prefix_"};
    test_prefix += std::to_string(::getpid()) + "_";
    std::string id1{test_prefix + "1"};
    std::string id2{test_prefix + "2"};

    ServiceProtocolConfig protocol_config1{id1, 0U, 0U, 0U};
    ServiceProtocolConfig protocol_config2{id2, 0U, 0U, 0U};
    auto server1 = factory.Create(protocol_config1, server_config);
    auto server2 = factory.Create(protocol_config2, server_config);
    EXPECT_TRUE(server1);
    EXPECT_TRUE(server2);
    auto connect_callback = [](IServerConnection&) {
        return score::cpp::make_unexpected(score::os::Error::createUnspecifiedError());
    };
    EXPECT_TRUE(server1->StartListening(connect_callback).has_value());
    EXPECT_TRUE(server2->StartListening(connect_callback).has_value());
    server2->StopListening();
}

TEST(QnxDispatchServerTest, RunningServerWithConnection)
{
    QnxDispatchServerFactory factory{};
    IServerFactory::ServerConfig server_config{};
    std::string test_prefix{"test_prefix_"};
    test_prefix += std::to_string(::getpid()) + "_";
    std::string id{test_prefix + "1"};

    ServiceProtocolConfig protocol_config{id, 6U, 6U, 6U};
    auto server = factory.Create(protocol_config, server_config);
    EXPECT_TRUE(server);
    auto connect_callback = [this](IServerConnection& connection) -> std::uintptr_t {
        std::cout << "EchoConnectCallback " << &connection << std::endl;
        const pid_t client_pid = connection.GetClientIdentity().pid;
        return static_cast<std::uintptr_t>(client_pid);
    };
    auto disconnect_callback = [this](IServerConnection& connection) {
        const auto client_pid = static_cast<pid_t>(std::get<std::uintptr_t>(connection.GetUserData()));
        std::cout << "EchoDisconnectCallback " << &connection << " " << client_pid << std::endl;
    };
    auto sent_callback = [](IServerConnection& connection,
                            score::cpp::span<const std::uint8_t> message) -> score::cpp::blank {
        std::cout << "EchoSentCallback " << &connection << std::endl;
        connection.Notify(message);
        return {};
    };
    auto sent_with_reply_callback = [](IServerConnection& connection,
                                       score::cpp::span<const std::uint8_t> message) -> score::cpp::blank {
        std::cout << "EchoSentWithReplyCallback " << &connection << std::endl;
        connection.Reply(message);
        return {};
    };
    ASSERT_TRUE(server->StartListening(connect_callback, disconnect_callback, sent_callback, sent_with_reply_callback)
                    .has_value());

    const detail::QnxResourcePath path{id};
    std::int32_t fd = ::open(path.c_str(), O_RDWR | O_CLOEXEC);
    EXPECT_TRUE(fd != -1);

    // send invalid code: at the moment, we just ignore the message
    std::array<std::uint8_t, 1U> invalid_code{255U};
    EXPECT_EQ(::write(fd, invalid_code.data(), invalid_code.size()), invalid_code.size());

    // read without ready message: receive EAGAIN
    std::array<std::uint8_t, 7U> read_buffer{};
    EXPECT_EQ(::read(fd, read_buffer.data(), read_buffer.size()), -1);
    EXPECT_EQ(errno, EAGAIN);

    ::close(fd);

    server->StopListening();
}

}  // namespace
}  // namespace message_passing
}  // namespace score
