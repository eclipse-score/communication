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
    const std::size_t index = kServerIndex;
    ResourceManagerMockHelper& helper = helpers_[index];

    WithEngineRunning();

    ServiceProtocolConfig protocol_config{"fake_path", 0U, 0U, 0U};
    IServerFactory::ServerConfig server_config{};
    detail::QnxDispatchServer server{engine_, protocol_config, server_config};

    auto connect_callback = [this](IServerConnection&) -> void* {
        return nullptr;
    };

    ExpectServerAttached();
    EXPECT_CALL(*channel_, ConnectClientInfo)
        .WillOnce(Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(EINVAL))));

    ExpectConnectionOpen();
    ASSERT_TRUE(server.StartListening(connect_callback, DisconnectCallback{}, MessageCallback{}, MessageCallback{})
                    .has_value());

    helper.HelperInsertIoOpen(score::cpp::blank{});
    EXPECT_EQ(helper.promises_.open.get_future().get(), EINVAL);

    ExpectServerDetached();
    server.StopListening();
}

TEST_F(QnxDispatchMockedServerFixture, ServerOpenConnectOcbAttachFailure)
{
    ::testing::Test::RecordProperty("lobster-tracing", "MessagePassing.OsIpcFaultHandling");
    ::testing::Test::RecordProperty("given", "QNX dispatch server listening with ``MockOs``");
    const std::size_t index = kServerIndex;
    ResourceManagerMockHelper& helper = helpers_[index];

    WithEngineRunning();

    ServiceProtocolConfig protocol_config{"fake_path", 0U, 0U, 0U};
    IServerFactory::ServerConfig server_config{};
    detail::QnxDispatchServer server{engine_, protocol_config, server_config};

    auto connect_callback = [this](IServerConnection&) -> void* {
        return nullptr;
    };

    ExpectServerAttached();
    ASSERT_TRUE(server.StartListening(connect_callback, DisconnectCallback{}, MessageCallback{}, MessageCallback{})
                    .has_value());

    ::testing::Test::RecordProperty(
        "when", "client open triggers ``ConnectClientInfo`` success but ``iofunc_ocb_attach`` returns ``EIO``");
    EXPECT_CALL(*channel_, ConnectClientInfo).Times(1);
    EXPECT_CALL(*iofunc_, iofunc_ocb_attach).WillOnce(Return(score::cpp::make_unexpected(EIO)));
    ExpectConnectionOpen();
    helper.HelperInsertIoOpen(score::cpp::blank{});
    ::testing::Test::RecordProperty("then", "server reports the open failure with error code ``EIO``");
    EXPECT_EQ(helper.promises_.open.get_future().get(), EIO);

    ExpectServerDetached();
    server.StopListening();
}

}  // namespace
}  // namespace message_passing
}  // namespace score
