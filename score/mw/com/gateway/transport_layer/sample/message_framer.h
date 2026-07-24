/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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
#ifndef SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_MESSAGE_FRAMER_H_
#define SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_MESSAGE_FRAMER_H_

#include "score/mw/com/gateway/transport_layer/sample/i_message_framer.h"
#include "score/mw/com/gateway/transport_layer/sample/messages/gateway_messages.h"
#include "score/mw/com/gateway/transport_layer/transport_error.h"

#include "score/result/result.h"

#include <array>
#include <cstdint>
#include <memory>

namespace score::mw::com::gateway
{

/// \brief Handles message framing: serialization, deserialization, and wire-format I/O.
///
/// This class encapsulates the logic of converting TransportMessage objects to/from
/// their wire representation. It owns the send and receive buffers and provides methods for
/// sending a framed message and receiving one.
class MessageFramer : public IMessageFramer
{
  public:
    static constexpr std::size_t kMaxPayloadSize = 1024U;

    MessageFramer() = default;
    ~MessageFramer() = default;

    MessageFramer(const MessageFramer&) = delete;
    MessageFramer& operator=(const MessageFramer&) = delete;
    MessageFramer(MessageFramer&&) = delete;
    MessageFramer& operator=(MessageFramer&&) = delete;

    /// \brief Serializes the given message and sends it on the specified socket file descriptor.
    /// \param socket_fd The file descriptor of the socket to send on.
    /// \param message The message to serialize and send.
    /// \return Success or kSendFailure.
    score::ResultBlank SendMessage(std::int32_t socket_fd, const TransportMessage& message) override;

    /// \brief Reads a complete framed message from the specified socket file descriptor.
    /// \param socket_fd The file descriptor of the socket to receive from.
    /// \return The deserialized message, or nullptr on error/disconnect.
    std::unique_ptr<TransportMessage> ReceiveMessage(std::int32_t socket_fd) override;

    /// \brief Creates a TransportMessage instance for the given message type.
    /// \return A new message instance, or nullptr for unknown/invalid types.
    static std::unique_ptr<TransportMessage> CreateMessageForType(MessageType type);

  private:
    std::array<std::uint8_t, kMaxPayloadSize> send_buffer_{};
    std::array<std::uint8_t, kMaxPayloadSize> receive_buffer_{};
};

}  // namespace score::mw::com::gateway

#endif  // SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_MESSAGE_FRAMER_H_
