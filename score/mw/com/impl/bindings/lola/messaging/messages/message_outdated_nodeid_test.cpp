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
#include "score/mw/com/impl/bindings/lola/messaging/messages/message_outdated_nodeid.h"

#include "score/mw/com/impl/bindings/lola/messaging/messages/message_common.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl::lola
{
namespace
{

constexpr pid_t SENDER_NODE_ID = 777;
constexpr pid_t OUTDATED_NODE_ID = 888;

TEST(MessageOutdatedNodeId, Creation)
{
    // given an OutdatedNodeIdMessage
    auto message = OutdatedNodeIdMessage{OUTDATED_NODE_ID, SENDER_NODE_ID};

    // expect, that members reflect the ctor params
    EXPECT_EQ(message.pid_to_unregister, OUTDATED_NODE_ID);
    EXPECT_EQ(message.sender_node_id, SENDER_NODE_ID);
}

TEST(MessageOutdatedNodeId, DeserializeToOutdatedNodeIdMessage)
{
    // Given a ShortMessagePayload
    message_passing::ShortMessagePayload shortMsg{};
    shortMsg = static_cast<message_passing::ShortMessagePayload>(OUTDATED_NODE_ID);

    // when deserializing the ShortMessagePayload to a OutdatedNodeIdMessage
    auto message = DeserializeToOutdatedNodeIdMessage(shortMsg, SENDER_NODE_ID);

    // expect, that members reflect the ShortMessagePayload parts
    EXPECT_EQ(message.pid_to_unregister, OUTDATED_NODE_ID);
    EXPECT_EQ(message.sender_node_id, SENDER_NODE_ID);
}

TEST(MessageOutdatedNodeId, SerializeToShortMessage)
{
    // given an OutdatedNodeIdMessage
    auto message = OutdatedNodeIdMessage{OUTDATED_NODE_ID, SENDER_NODE_ID};

    // when serializing to ShortMessage
    auto shortMsg = SerializeToShortMessage(message);

    // expect, that ShortMessage members reflect correctly the OutdatedNodeIdMessage
    EXPECT_EQ(shortMsg.id, static_cast<message_passing::MessageId>(MessageType::kOutdatedNodeId));
    EXPECT_EQ(shortMsg.pid, SENDER_NODE_ID);
    message_passing::ShortMessagePayload expectedPayload;
    expectedPayload = static_cast<message_passing::ShortMessagePayload>(OUTDATED_NODE_ID);
    EXPECT_EQ(shortMsg.payload, expectedPayload);
}

TEST(MessageOutdatedNodeId, Roundtrip)
{
    // given an OutdatedNodeIdMessage
    auto message = OutdatedNodeIdMessage{OUTDATED_NODE_ID, SENDER_NODE_ID};

    // when serializing to ShortMessage
    auto shortMsg = SerializeToShortMessage(message);

    // and then deserializing again to an OutdatedNodeIdMessage
    auto message_2 = DeserializeToOutdatedNodeIdMessage(shortMsg.payload, shortMsg.pid);

    // expect, that both messages are equal
    EXPECT_EQ(message, message_2);
}

TEST(MessageOutdatedNodeId, MessagesContainingSameDataAreEqual)
{
    // given 2 OutdatedNodeIdMessages containing the same data
    auto message = OutdatedNodeIdMessage{OUTDATED_NODE_ID, SENDER_NODE_ID};
    auto message_2 = OutdatedNodeIdMessage{OUTDATED_NODE_ID, SENDER_NODE_ID};

    // when comparing the 2 messages
    // Then the result is true
    EXPECT_EQ(message, message_2);
}

TEST(MessageOutdatedNodeId, MessagesContainingDifferentPidsToUnregisterAreUnequal)
{
    // given 2 OutdatedNodeIdMessages with different pids to register
    auto message = OutdatedNodeIdMessage{OUTDATED_NODE_ID, SENDER_NODE_ID};
    auto message_2 = OutdatedNodeIdMessage{OUTDATED_NODE_ID + 1U, SENDER_NODE_ID};

    // when comparing the 2 messages
    const auto are_equal = message == message_2;

    // Then the result is false
    EXPECT_FALSE(are_equal);
}

TEST(MessageOutdatedNodeId, MessagesContainingDifferentSenderNodeIdsAreUnequal)
{
    // given 2 OutdatedNodeIdMessages with sender node ids
    auto message = OutdatedNodeIdMessage{OUTDATED_NODE_ID, SENDER_NODE_ID};
    auto message_2 = OutdatedNodeIdMessage{OUTDATED_NODE_ID, SENDER_NODE_ID + 1U};

    // when comparing the 2 messages
    const auto are_equal = message == message_2;

    // Then the result is false
    EXPECT_FALSE(are_equal);
}

}  // namespace
}  // namespace score::mw::com::impl::lola
