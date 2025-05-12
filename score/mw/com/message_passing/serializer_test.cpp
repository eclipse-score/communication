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
#include "score/mw/com/message_passing/serializer.h"

#include <gtest/gtest.h>

#include <cstring>

namespace score::mw::com::message_passing
{
namespace
{

TEST(Serializer, CanSerializeShortMessageToRawMessage)
{
    // Given a ShortMessage
    ShortMessage message{};
    message.id = 0x42;
    message.pid = 1233;
    message.payload = 0xABCDEF;

    // When serializing into a RawMessage
    const auto raw_message = SerializeToRawMessage(message);

    // Then
    // first byte representing the MessageType is a ShortMessage
    EXPECT_EQ(static_cast<MessageType>(raw_message.at(0)), MessageType::kShortMessage);
    // second byte holds the message id
    EXPECT_EQ(raw_message.at(1), message.id);
    // next sizeof(pid_t) bytes contain the pid
    EXPECT_EQ(std::memcmp(&message.pid, raw_message.data() + 2, sizeof(message.pid)), 0);
    // following bytes hold the payload
    EXPECT_EQ(raw_message.at(sizeof(message.pid) + 2), '\xEF');
    EXPECT_EQ(raw_message.at(sizeof(message.pid) + 3), '\xCD');
    EXPECT_EQ(raw_message.at(sizeof(message.pid) + 4), '\xAB');
}

TEST(Serializer, CanSerializeMediumMessageToRawMessage)
{
    // Given a MediumMessage
    MediumMessage message{};
    message.id = 0x22;
    message.pid = 1233;
    message.payload = {'L', 'o', 'L', 'a', ' ', 'G', 'o', '!'};

    // then serializing into a RawMessage
    const auto raw_message = SerializeToRawMessage(message);

    // Then
    // first byte representing the MessageType is a MediumMessage
    EXPECT_EQ(static_cast<MessageType>(raw_message.at(0)), MessageType::kMediumMessage);
    // second byte holds the message id
    EXPECT_EQ(raw_message.at(1), message.id);
    // next sizeof(pid_t) bytes contain the pid
    EXPECT_EQ(std::memcmp(&message.pid, raw_message.data() + 2, sizeof(message.pid)), 0);
    // following bytes hold the payload
    EXPECT_EQ(raw_message.at(sizeof(message.pid) + 2), 'L');
    EXPECT_EQ(raw_message.at(sizeof(message.pid) + 3), 'o');
    EXPECT_EQ(raw_message.at(sizeof(message.pid) + 4), 'L');
    EXPECT_EQ(raw_message.at(sizeof(message.pid) + 5), 'a');
    EXPECT_EQ(raw_message.at(sizeof(message.pid) + 6), ' ');
    EXPECT_EQ(raw_message.at(sizeof(message.pid) + 7), 'G');
    EXPECT_EQ(raw_message.at(sizeof(message.pid) + 8), 'o');
    EXPECT_EQ(raw_message.at(sizeof(message.pid) + 9), '!');
}

TEST(Serializer, CanDeserializeSerializedShortMessage)
{
    // Given a serialized ShortMessage
    ShortMessage message{};
    message.id = 0x42;
    message.pid = 1233;
    message.payload = 0xABCDEF;
    const auto raw_message = SerializeToRawMessage(message);

    // When deserializing to a ShortMessage
    const auto short_message = DeserializeToShortMessage(raw_message);

    // Then the message is the same
    EXPECT_EQ(short_message.id, message.id);
    EXPECT_EQ(short_message.pid, message.pid);
    EXPECT_EQ(short_message.payload, message.payload);
}

TEST(Serializer, CanDeserializeSerializedMediumMessage)
{
    // Given a serialized MediumMessage
    MediumMessage message{};
    message.id = 0x22;
    message.pid = 1233;
    message.payload = {'L', 'o', 'L', 'a', ' ', 'G', 'o', '!'};
    const auto raw_message = SerializeToRawMessage(message);

    // When deserializing to a MediumMessage
    const auto short_message = DeserializeToMediumMessage(raw_message);

    // Then the message is the same
    EXPECT_EQ(short_message.id, message.id);
    EXPECT_EQ(short_message.pid, message.pid);
    EXPECT_EQ(short_message.payload, message.payload);
}

}  // namespace
}  // namespace score::mw::com::message_passing
