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

#include "score/mw/com/impl/bindings/lola/messaging/message_passing_client_cache.h"
#include "score/message_passing/mock/client_connection_mock.h"
#include "score/message_passing/mock/client_factory_mock.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl::lola
{
namespace
{
using namespace message_passing;

class MessagePassingClientCacheTest : public ::testing::TestWithParam<ClientQualityType>
{
  public:
    static constexpr pid_t pid_{21};
    static constexpr pid_t pid2_{42};

    score::cpp::pmr::unique_ptr<::testing::NiceMock<ClientConnectionMock>> client_connection_mock_{
        score::cpp::pmr::make_unique<::testing::NiceMock<ClientConnectionMock>>(score::cpp::pmr::new_delete_resource())};
    ClientFactoryMock client_factory_mock_{};
    MessagePassingClientCache client_cache_{GetParam(), client_factory_mock_};
};

INSTANTIATE_TEST_SUITE_P(ClientQualityType,
                         MessagePassingClientCacheTest,
                         ::testing::Values(ClientQualityType::kASIL_QM,
                                           ClientQualityType::kASIL_B,
                                           ClientQualityType::kASIL_QMfromB));

TEST_P(MessagePassingClientCacheTest, GetMessagePassingClientCreatesNewClientConnection)
{
    // Given an empty MessagePassingClientCache
    // and factory that returns new ClientConnectionMock
    // Expect that factory will be invoked to create of a new client connection
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(::testing::ByMove(std::move(client_connection_mock_))));

    // When GetMessagePassingClient is called
    client_cache_.GetMessagePassingClient(pid_);
}

TEST_P(MessagePassingClientCacheTest, GetMessagePassingClientCreatesNewClientConnectionIsNonReadyState)
{
    // Given an empty MessagePassingClientCache
    // and client connection mock in non-ready state
    EXPECT_CALL(*client_connection_mock_, GetState())
        .WillRepeatedly(::testing::Return(IClientConnection::State::kStarting));

    // and factory that returns the client connection mock
    // Expect that factory will be invoked to create of a new client connection
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(::testing::ByMove(std::move(client_connection_mock_))));

    // When GetMessagePassingClient is called
    auto client_connection = client_cache_.GetMessagePassingClient(pid_);
}

TEST_P(MessagePassingClientCacheTest, GetMessagePassingClientCreatesNewClientConnectionIsStoppedState)
{
    // Given an empty MessagePassingClientCache
    // expect that GetState is called at the client connection, which will return kStopped
    EXPECT_CALL(*client_connection_mock_, GetState()).WillOnce(::testing::Return(IClientConnection::State::kStopped));
    // and expect that GetStopReason is called at the client connection, which will return kIoError
    EXPECT_CALL(*client_connection_mock_, GetStopReason())
        .WillOnce(::testing::Return(IClientConnection::StopReason::kIoError));

    // Expect that factory will be invoked to create a new client connection
    // and factory returns the client connection mock expecting the above
    // (the order is changed because ByMoveWrapper invalidates the original unique_ptr)
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(::testing::ByMove(std::move(client_connection_mock_))));

    // When GetMessagePassingClient is called
    auto client_connection = client_cache_.GetMessagePassingClient(pid_);

    // then the returned shared_ptr is valid
    EXPECT_TRUE(client_connection);
}

TEST_P(MessagePassingClientCacheTest,
       GetMessagePassingClientCreatesDistinctClientConnectionsProvidedDifferentTargetNodeIds)
{
    // Given an empty MessagePassingClientCache
    // and factory that returns new ClientConnectionMock each time
    // Expect that factory will be invoked to create of a new client connection
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(::testing::ByMove(
            score::cpp::pmr::make_unique<::testing::NiceMock<ClientConnectionMock>>(score::cpp::pmr::new_delete_resource()))))
        .WillOnce(::testing::Return(::testing::ByMove(
            score::cpp::pmr::make_unique<::testing::NiceMock<ClientConnectionMock>>(score::cpp::pmr::new_delete_resource()))));

    // When GetMessagePassingClient is called with different target node ids
    auto client1 = client_cache_.GetMessagePassingClient(pid_);
    auto client2 = client_cache_.GetMessagePassingClient(pid2_);

    // Expect different client connections returned
    EXPECT_NE(client1, client2);
}

TEST_P(MessagePassingClientCacheTest, GetMessagePassingClientReturnsSameClientConectionForSameTargetNodeId)
{
    // Given an empty MessagePassingClientCache
    // and factory that returns new ClientConnectionMock
    // Expect that factory will be invoked once to create of a new client connection
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(::testing::ByMove(std::move(client_connection_mock_))));

    // When GetMessagePassingClient is called twice with the same target node id
    auto client = client_cache_.GetMessagePassingClient(pid_);
    auto client_2 = client_cache_.GetMessagePassingClient(pid_);

    // Expect both results to point at the same object
    EXPECT_EQ(client, client_2);
}

TEST_P(MessagePassingClientCacheTest, RemoveMessagePassingClientRemovesClientInStoppedState)
{
    // Given an empty MessagePassingClientCache
    // and client connection mock that returns stopped state after creation
    EXPECT_CALL(*client_connection_mock_, GetState())
        // to avoid multiple calls to GetState() after creation
        .WillOnce(::testing::Return(IClientConnection::State::kReady))
        .WillOnce(::testing::Return(IClientConnection::State::kStopped));

    // and factory that returns the ClientConnectionMock
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(::testing::ByMove(std::move(client_connection_mock_))));

    // and a new client connection is created
    auto cl1 = client_cache_.GetMessagePassingClient(pid_);

    // When RemoveMessagePassingClient with called with the same target node id
    client_cache_.RemoveMessagePassingClient(pid_);

    // Expect factory's .Create() to be called
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(
            ::testing::ByMove(score::cpp::pmr::make_unique<ClientConnectionMock>(score::cpp::pmr::new_delete_resource()))));

    // When GetMessagePassingClient is called with the same target node id
    client_cache_.GetMessagePassingClient(pid_);
}

TEST_P(MessagePassingClientCacheTest, RemoveMessagePassingClientRemovingNonExistentClientDoesntLeadToAbort)
{
    // Given an empty MessagePassingClientCache
    // When RemoveMessagePassingClient is called with previously unused target node id
    // Expect no termination
    client_cache_.RemoveMessagePassingClient(pid_);
}

TEST(MessagePassingClientCacheDeathTest, RemoveMessagePassingClientTerminatesWhenCalledOnNonStoppedClientConnection)
{
    constexpr pid_t target_node_id{12};

    // Given client connection mock in ready state
    score::cpp::pmr::unique_ptr<::testing::NiceMock<ClientConnectionMock>> client_connection_mock{
        score::cpp::pmr::make_unique<::testing::NiceMock<ClientConnectionMock>>(score::cpp::pmr::new_delete_resource())};
    EXPECT_CALL(*client_connection_mock, GetState())
        .WillRepeatedly(::testing::Return(IClientConnection::State::kReady));

    // and factory that returns the ClientConnectionMock
    ClientFactoryMock client_factory_mock{};
    EXPECT_CALL(client_factory_mock, Create(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(::testing::ByMove(std::move(client_connection_mock))));

    // and an empty MessagePassingClientCache
    MessagePassingClientCache client_cache{ClientQualityType::kASIL_B, client_factory_mock};
    client_cache.GetMessagePassingClient(target_node_id);

    // When RemoveMessagePassingClient is called with the same target_node_id
    // Expect it to terminate
    EXPECT_DEATH(client_cache.RemoveMessagePassingClient(target_node_id), ".*");
}

}  // namespace
}  // namespace score::mw::com::impl::lola
