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
#include "score/mw/com/impl/bindings/lola/messaging/messages/message_element_fq_id.h"

#include "score/mw/com/impl/bindings/lola/messaging/messages/message_common.h"

#include <gtest/gtest.h>
#include <score/optional.hpp>

#include <cstdint>
#include <cstring>

namespace score::mw::com::impl::lola
{
namespace
{

const ElementFqId kSomeElementFqId{1, 1, 1, ServiceElementType::EVENT};
constexpr pid_t PID = 777;
constexpr MessageType test_message_id{MessageType::kNotifyEvent};

TEST(MessageElementFQId, DeserializeToNotifyEventUpdateMessage)
{
    // Given a ShortMessagePayload
    message_passing::ShortMessagePayload shortMsg{};
    shortMsg = static_cast<message_passing::ShortMessagePayload>(kSomeElementFqId.service_id_) << 40u;
    shortMsg |= static_cast<message_passing::ShortMessagePayload>(kSomeElementFqId.element_id_) << 24u;
    shortMsg |= static_cast<message_passing::ShortMessagePayload>(kSomeElementFqId.instance_id_) << 8u;
    shortMsg |= static_cast<message_passing::ShortMessagePayload>(kSomeElementFqId.element_type_);

    // when deserializing the ShortMessagePayload to a ElementFqIdMessage
    const auto actual_message = ElementFqIdMessage<test_message_id>::DeserializeToElementFqIdMessage(shortMsg, PID);
    const auto expected_message = ElementFqIdMessage<test_message_id>(kSomeElementFqId, PID);

    // expect, that members reflect the ShortMessagePayload parts
    EXPECT_EQ(actual_message, expected_message);
}

TEST(MessageElementFQId, SerializeToShortMessage)
{
    // given a ElementFqIdMessage
    auto message = ElementFqIdMessage<test_message_id>(kSomeElementFqId, PID);

    // when serializing to ShortMessage
    auto shortMsg = message.SerializeToShortMessage();

    // expect, that ShortMessage members reflect correctly the ElementFqIdMessage
    EXPECT_EQ(shortMsg.id, static_cast<message_passing::MessageId>(test_message_id));
    EXPECT_EQ(shortMsg.pid, PID);
    EXPECT_EQ(shortMsg.payload >> 40u, kSomeElementFqId.service_id_);
    EXPECT_EQ((shortMsg.payload >> 24u) & 0x000000FF, kSomeElementFqId.element_id_);
    EXPECT_EQ((shortMsg.payload >> 8u) & 0x0000FFFF, kSomeElementFqId.instance_id_);
    EXPECT_EQ((shortMsg.payload) & 0x000000FF,
              static_cast<message_passing::ShortMessagePayload>(kSomeElementFqId.element_type_));
}

TEST(MessageElementFQId, SerializeDeserializeSymetric)
{
    // given a ElementFqIdMessage
    const auto message = ElementFqIdMessage<test_message_id>(kSomeElementFqId, PID);

    // when serializing it to shortmessage
    const auto shortMessage = message.SerializeToShortMessage();
    // and deserializing it to a ElementFqIdMessage again
    const auto message2 =
        ElementFqIdMessage<test_message_id>::DeserializeToElementFqIdMessage(shortMessage.payload, shortMessage.pid);

    // expect, that both ElementFqIdMessage are identical.
    EXPECT_EQ(message, message2);
}

}  // namespace
}  // namespace score::mw::com::impl::lola
