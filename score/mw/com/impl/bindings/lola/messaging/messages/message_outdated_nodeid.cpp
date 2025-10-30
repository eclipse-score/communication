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

#include <score/utility.hpp>

#include <cstring>

namespace score::mw::com::impl::lola
{

OutdatedNodeIdMessage DeserializeToOutdatedNodeIdMessage(
    const score::mw::com::message_passing::ShortMessagePayload& message_payload,
    const pid_t sender_node_id)
{
    pid_t pid_to_unregister{};
    score::cpp::ignore =
        // NOLINTNEXTLINE(score-banned-function): Retrieves pid from certain bytes of message payload.
        std::memcpy(static_cast<void*>(&pid_to_unregister), static_cast<const void*>(&message_payload), sizeof(pid_t));

    return {pid_to_unregister, sender_node_id};
}

// Suppress "AUTOSAR C++14 A3-1-1", The rule states: "It shall be possible to include any header file
// in multiple translation units without violating the One Definition Rule."
// This is false positive. Function is declared only once.
// coverity[autosar_cpp14_a3_1_1_violation]
message_passing::ShortMessage SerializeToShortMessage(const OutdatedNodeIdMessage& outdated_node_id_message)
{
    static_assert(sizeof(outdated_node_id_message.pid_to_unregister) <= sizeof(message_passing::ShortMessage::payload),
                  "ShortMessage size not sufficient for OutdatedNodeIdMessage.");
    message_passing::ShortMessage message{};
    message.id = static_cast<message_passing::MessageId>(MessageType::kOutdatedNodeId);
    message.pid = outdated_node_id_message.sender_node_id;
    // NOLINTNEXTLINE(score-banned-function): copies pid into message payload.
    score::cpp::ignore = std::memcpy(static_cast<void*>(&message.payload),
                                     static_cast<const void*>(&outdated_node_id_message.pid_to_unregister),
                                     sizeof(outdated_node_id_message.pid_to_unregister));
    return message;
}

bool operator==(const OutdatedNodeIdMessage& lhs, const OutdatedNodeIdMessage& rhs) noexcept
{
    return ((lhs.sender_node_id == rhs.sender_node_id) && (lhs.pid_to_unregister == rhs.pid_to_unregister));
}

}  // namespace score::mw::com::impl::lola
