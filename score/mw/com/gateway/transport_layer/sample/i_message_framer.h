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
#ifndef SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_I_MESSAGE_FRAMER_H_
#define SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_I_MESSAGE_FRAMER_H_

#include "score/mw/com/gateway/transport_layer/sample/messages/gateway_messages.h"
#include "score/mw/com/gateway/transport_layer/transport_error.h"

#include "score/result/result.h"

#include <array>
#include <cstdint>
#include <memory>

namespace score::mw::com::gateway
{

/// \brief Interface of class MessageFramer, which is used for testing and mocking purposes only.
class IMessageFramer
{
  public:
    virtual ~IMessageFramer() = default;

    /// \brief Serializes the given message and sends it on the specified socket file descriptor.
    /// \param socket_fd The file descriptor of the socket to send on.
    /// \param message The message to serialize and send.
    /// \return Success or kSendFailure.
    virtual score::ResultBlank SendMessage(std::int32_t socket_fd, const TransportMessage& message) = 0;

    /// \brief Reads a complete framed message from the specified socket file descriptor.
    /// \param socket_fd The file descriptor of the socket to receive from.
    /// \return The deserialized message, or nullptr on error/disconnect.
    virtual std::unique_ptr<TransportMessage> ReceiveMessage(std::int32_t socket_fd) = 0;
};

}  // namespace score::mw::com::gateway

#endif  // SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_I_MESSAGE_FRAMER_H_
