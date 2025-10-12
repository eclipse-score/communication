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

    ServiceProtocolConfig protocol_config1{id1, 0, 0, 0};
    ServiceProtocolConfig protocol_config2{id2, 0, 0, 0};
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

#ifndef __QNX__  // not yet clear why QNX allows both instances to listen
TEST(QnxDispatchServerTest, RunningServersWithSameId)
{
    QnxDispatchServerFactory factory{};
    IServerFactory::ServerConfig server_config{};
    std::string test_prefix{"test_prefix_"};
    test_prefix += std::to_string(::getpid()) + "_";
    std::string id1{test_prefix + "1"};

    ServiceProtocolConfig protocol_config1{id1, 0, 0, 0};
    ServiceProtocolConfig protocol_config2{id1, 0, 0, 0};
    auto server1 = factory.Create(protocol_config1, server_config);
    auto server2 = factory.Create(protocol_config2, server_config);
    EXPECT_TRUE(server1);
    EXPECT_TRUE(server2);
    auto connect_callback = [](IServerConnection&) {
        return score::cpp::make_unexpected(score::os::Error::createUnspecifiedError());
    };
    EXPECT_TRUE(server1->StartListening(connect_callback).has_value());
    EXPECT_FALSE(server2->StartListening(connect_callback).has_value());
}
#endif

}  // namespace
}  // namespace message_passing
}  // namespace score
