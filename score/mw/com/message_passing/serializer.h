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
#ifndef SCORE_MW_COM_MESSAGE_PASSING_SERIALIZER_H
#define SCORE_MW_COM_MESSAGE_PASSING_SERIALIZER_H

#include "score/mw/com/message_passing/message.h"
#include "score/mw/com/message_passing/shared_properties.h"

namespace score::mw::com::message_passing
{

using Byte = char;

enum class MessageType : Byte
{
    kStopMessage = 0x00,
    kShortMessage = 0x42,
    kMediumMessage = 0x43,
};

constexpr std::size_t GetMaxMessageSize()
{
    constexpr std::size_t medium_message_size =
        // Suppress "AUTOSAR C++14 A4-10-1", The rule states: "Only nullptr literal shall be used as the
        // null-pointer-constant". No null pointer is used.
        // Suppress "AUTOSAR C++14 m4-10-2", The rule states: "Literal zero (0) shall not be used as the
        // null-pointer-constant". No null pointer is used.
        // coverity[autosar_cpp14_a4_10_1_violation : FALSE]
        // coverity[autosar_cpp14_m4_10_2_violation : FALSE]
        sizeof(MessageType) + sizeof(BaseMessage::id) + sizeof(BaseMessage::pid) + sizeof(MediumMessage::payload);

    return medium_message_size;
}

using RawMessageBuffer = std::array<Byte, GetMaxMessageSize()>;

inline constexpr std::uint32_t GetMessagePriority()
{
    return 0U;
}

/// \details The serialization format for our short message on the queue looks like this:
/// \note serialization format of our medium message looks the same, except, that the payload length is 8.
///
/// +------------+----------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
/// |   Byte 0   |  Byte 1  | Byte 2 | Byte 3 | Byte 4 | Byte 5 | Byte 6 | Byte 7 | Byte 8 | Byte 9 | Byte 10| Byte 11|
/// +------------+----------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
/// | Msg. Type  | Mesg. ID |          PID of Sender            |       Message Payload             |        N/A      |
/// +------------+----------+-----------------------------------+-----------------------------------+-----------------+

inline constexpr std::size_t GetMessageTypePosition()
{
    return 0U;
}

inline constexpr std::size_t GetMessageIdPosition()
{
    return 1U;
}

inline constexpr std::size_t GetMessagePidPosition()
{
    return 2U;
}

inline constexpr std::size_t GetMessageStartPayload()
{
    return GetMessagePidPosition() + sizeof(pid_t);
}

/// \brief Serializes a ShortMessage into a buffer to transmit it (not considering byte-order)
/// \return RawMessageBuffer that contains the serialized message
RawMessageBuffer SerializeToRawMessage(const ShortMessage& message) noexcept;

/// \brief Serializes a MediumMessage into a buffer to transmit it (not considering byte-order)
/// \return RawMessageBuffer that contains the serialized message
RawMessageBuffer SerializeToRawMessage(const MediumMessage& message) noexcept;

/// \brief Deserializes a buffer into a ShortMessage
/// \return ShortMessage build from provided buffer
ShortMessage DeserializeToShortMessage(const RawMessageBuffer& buffer) noexcept;

/// \brief Deserializes a buffer into a MediumMessage
/// \return MediumMessage build from provided buffer
MediumMessage DeserializeToMediumMessage(const RawMessageBuffer& buffer) noexcept;

}  // namespace score::mw::com::message_passing

#endif  // SCORE_MW_COM_MESSAGE_PASSING_SERIALIZER_H
