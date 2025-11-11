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

#include "score/message_passing/qnx_dispatch/qnx_dispatch_server.h"

#include "score/message_passing/resource_manager_fixture_base.h"

namespace score
{
namespace message_passing
{
namespace
{

using namespace ::testing;

using QnxDispatchMockedServerFixture = ResourceManagerFixtureBase;

TEST_F(QnxDispatchMockedServerFixture, ServerOpenConnectClientInfoFailure)
{
    ExpectEngineConstructed();
    ExpectEngineThreadRunning();
    ExpectServerAttached();
    ExpectConnectionOpen();

    EXPECT_CALL(*channel_, ConnectClientInfo)
        .WillOnce(Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(EINVAL))));

    ExpectServerDetached();
    ExpectEngineDestructed();

    auto engine = std::make_shared<QnxDispatchEngine>(score::cpp::pmr::get_default_resource(), MoveMockOsResources());
    ServiceProtocolConfig protocol_config{"fake_path", 0U, 0U, 0U};
    IServerFactory::ServerConfig server_config{};
    detail::QnxDispatchServer server{engine, protocol_config, server_config};
    QnxDispatchEngine::QnxResourcePath path{"fake_path"};

    auto connect_callback = [this](IServerConnection&) -> void* {
        return nullptr;
    };

    ASSERT_TRUE(server.StartListening(connect_callback, DisconnectCallback{}, MessageCallback{}, MessageCallback{})
                    .has_value());

    helper_.HelperInsertIoOpen(score::cpp::blank{});
    EXPECT_EQ(helper_.promises_.open.get_future().get(), EINVAL);

    server.StopListening();
}

TEST_F(QnxDispatchMockedServerFixture, ServerOpenConnectOcbAttachFailure)
{
    ExpectEngineConstructed();
    ExpectEngineThreadRunning();
    ExpectServerAttached();
    ExpectConnectionOpen();

    EXPECT_CALL(*channel_, ConnectClientInfo).Times(1);
    EXPECT_CALL(*iofunc_, iofunc_ocb_attach).WillOnce(Return(score::cpp::make_unexpected(EIO)));

    ExpectServerDetached();
    ExpectEngineDestructed();

    auto engine = std::make_shared<QnxDispatchEngine>(score::cpp::pmr::get_default_resource(), MoveMockOsResources());
    ServiceProtocolConfig protocol_config{"fake_path", 0U, 0U, 0U};
    IServerFactory::ServerConfig server_config{};
    detail::QnxDispatchServer server{engine, protocol_config, server_config};

    auto connect_callback = [this](IServerConnection&) -> void* {
        return nullptr;
    };

    ASSERT_TRUE(server.StartListening(connect_callback, DisconnectCallback{}, MessageCallback{}, MessageCallback{})
                    .has_value());

    helper_.HelperInsertIoOpen(score::cpp::blank{});
    EXPECT_EQ(helper_.promises_.open.get_future().get(), EIO);

    server.StopListening();
}

}  // namespace
}  // namespace message_passing
}  // namespace score
