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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGE_OUTDATED_NODEID_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGE_OUTDATED_NODEID_H

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/messaging/messages/message_common.h"
#include "score/mw/com/message_passing/message.h"
#include "score/os/unistd.h"

namespace score::mw::com::impl::lola
{

/// \brief Message sent from the consumer/proxy side to the provider/skeleton side, to notify provider/skeleton side
///        about the given pid/node id is outdated (was from a previous run of the consumer/proxy side application).
struct OutdatedNodeIdMessage
{
    // Suppress "AUTOSAR C++14 A9-6-1" rule findings. This rule declares: "Data types used for interfacing with hardware
    // or conforming to communication protocols shall be trivial, standard-layout and only contain members of types with
    // defined sizes."
    // While true that pid_t is not a fixed width integer it is required by the POSIX standard here.
    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule declares: "Member data in non-POD class types
    // shall be private."
    // struct is semantically POD/trivial type.
    // coverity[autosar_cpp14_a9_6_1_violation]
    // coverity[autosar_cpp14_m11_0_1_violation]
    pid_t pid_to_unregister{};
    // coverity[autosar_cpp14_a9_6_1_violation]
    // coverity[autosar_cpp14_m11_0_1_violation]
    pid_t sender_node_id{};
};

/// \brief Creates a OutdatedNodeIdMessage from a serialized short message payload.
/// \param message_payload payload from where to deserialize
/// \param sender_node_id node id of the sender, which sends this message.
/// \return OutdatedNodeIdMessage created from payload
OutdatedNodeIdMessage DeserializeToOutdatedNodeIdMessage(const message_passing::ShortMessagePayload& message_payload,
                                                         const pid_t sender_node_id);
/// \brief Serialize message to short message payload.
/// \param outdated_node_id_message OutdatedNodeIdMessage to be serialized.
/// \return short message representing OutdatedNodeIdMessage
message_passing::ShortMessage SerializeToShortMessage(const OutdatedNodeIdMessage& outdated_node_id_message);

/// \brief comparison op for OutdatedNodeIdMessage
/// \param lhs
/// \param rhs
/// \return true in case all members compare equal, false else.
bool operator==(const OutdatedNodeIdMessage& lhs, const OutdatedNodeIdMessage& rhs) noexcept;

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGE_OUTDATED_NODEID_H
