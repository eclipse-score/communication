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
#include "score/mw/com/gateway/transport_layer/sample/message_framer.h"

#include "score/mw/log/logging.h"
#include "score/os/socket.h"

#include <score/span.hpp>

namespace score::mw::com::gateway
{

namespace
{

using score::os::Socket;

/// \brief Helper function to send the buffer's data over a socket.
int SendAll(std::int32_t socket_fd, const void* data, std::size_t length)
{
    auto* ptr = static_cast<const char*>(data);
    while (length > 0)
    {
        auto result = Socket::instance().sendto(socket_fd, ptr, length, Socket::MessageFlag::kNone, nullptr, 0);
        if (!result.has_value())
        {
            return -1;
        }
        const auto sent = static_cast<std::size_t>(result.value());
        ptr += sent;
        length -= sent;
    }
    return 0;
}

/// \brief Helper function to receive data from a socket until the expected amount of bytes is received or an error
/// occurs.
ssize_t ReceiveAll(std::int32_t socket_fd, void* buffer, std::size_t length)
{
    auto* ptr = static_cast<char*>(buffer);
    const auto original_length = length;
    const score::os::Socket::MessageFlag flags{};
    while (length > 0)
    {
        auto result = Socket::instance().recv(socket_fd, ptr, length, flags);
        if (!result.has_value())
        {
            ::score::mw::log::LogError() << "ReceiveAll: recv error: " << result.error().ToString()
                                         << " fd=" << socket_fd << " remaining=" << length;
            return -1;
        }
        if (result.value() == 0)
        {
            ::score::mw::log::LogWarn() << "ReceiveAll: peer closed connection, fd=" << socket_fd
                                        << " remaining=" << length << " of " << original_length;
            return 0;
        }
        const auto received = static_cast<std::size_t>(result.value());
        ptr += received;
        length -= received;
    }
    return static_cast<ssize_t>(original_length);
}

}  // namespace

std::unique_ptr<TransportMessage> MessageFramer::CreateMessageForType(MessageType type)
{
    switch (type)
    {
        case MessageType::kProvideServiceRequest:
            return std::make_unique<ProvideServiceRequest>();
        case MessageType::kOfferServiceRequest:
            return std::make_unique<OfferServiceRequest>();
        case MessageType::kStopOfferServiceRequest:
            return std::make_unique<StopOfferServiceRequest>();
        case MessageType::kRegisterNotificationRequest:
            return std::make_unique<RegisterNotificationRequest>();
        case MessageType::kUnregisterNotificationRequest:
            return std::make_unique<UnregisterNotificationRequest>();
        case MessageType::kUpdateNotification:
            return std::make_unique<UpdateNotification>();
        case MessageType::kAckResponse:
            return std::make_unique<AckResponse>();
        case MessageType::kInvalid:
        default:
            return nullptr;
    }
}

score::ResultBlank MessageFramer::SendMessage(std::int32_t socket_fd, const TransportMessage& message)
{
    const auto payload_size = message.Serialize(send_buffer_);
    if (payload_size == 0U)
    {
        return score::MakeUnexpected(TransportErrorc::kSendFailure);
    }

    MessageHeader header = message.GetHeader();
    header.payload_size = static_cast<std::uint32_t>(payload_size);

    std::array<std::uint8_t, MessageHeader::kWireSize> header_buf{};
    header.SerializeToBuffer(header_buf.data());

    if (SendAll(socket_fd, &header_buf, MessageHeader::kWireSize) != 0)
    {
        return score::MakeUnexpected(TransportErrorc::kSendFailure);
    }

    if (SendAll(socket_fd, send_buffer_.data(), payload_size) != 0)
    {
        return score::MakeUnexpected(TransportErrorc::kSendFailure);
    }

    return {};
}

std::unique_ptr<TransportMessage> MessageFramer::ReceiveMessage(std::int32_t socket_fd)
{
    std::array<std::uint8_t, MessageHeader::kWireSize> header_buf{};
    if (ReceiveAll(socket_fd, &header_buf, MessageHeader::kWireSize) <= 0)
    {
        return nullptr;
    }

    MessageHeader header{};
    header.DeserializeFromBuffer(header_buf.data());

    if (header.payload_size > kMaxPayloadSize)
    {
        ::score::mw::log::LogError() << "MessageFramer: payload size " << header.payload_size << " exceeds maximum "
                                     << kMaxPayloadSize;
        return nullptr;
    }

    if (header.payload_size > 0U)
    {
        const auto bytes_received = ReceiveAll(socket_fd, receive_buffer_.data(), header.payload_size);
        if (bytes_received <= 0)
        {
            return nullptr;
        }
    }

    auto message = CreateMessageForType(header.type);
    if (message == nullptr)
    {
        ::score::mw::log::LogError() << "MessageFramer: unknown message type " << static_cast<int>(header.type);
        return nullptr;
    }

    message->SetSequenceNumber(header.sequence);

    if (header.payload_size > 0U &&
        !message->Deserialize(score::cpp::span<const std::uint8_t>(receive_buffer_.data(), header.payload_size)))
    {
        ::score::mw::log::LogError() << "MessageFramer: deserialization failed";
        return nullptr;
    }

    return message;
}

}  // namespace score::mw::com::gateway
