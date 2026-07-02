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
#ifndef SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_TRANSPORT_MESSAGE_H_
#define SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_TRANSPORT_MESSAGE_H_

#include <score/span.hpp>

#include <cstdint>
#include <cstring>

namespace score::mw::com::gateway
{

enum class RequestType : std::uint8_t
{
    kProvideService = 0x01,
    kOfferService = 0x02,
    kStopOfferService = 0x03,
    kRegisterNotification = 0x04,
    kUnregisterNotification = 0x05
};

enum class NotificationType : std::uint8_t
{
    kUpdate = 0x40,
};

enum class ResponseType : std::uint8_t
{
    kAck = 0x80
};

enum class MessageType : std::uint8_t
{
    // Requests
    kProvideServiceRequest = static_cast<std::uint8_t>(RequestType::kProvideService),
    kOfferServiceRequest = static_cast<std::uint8_t>(RequestType::kOfferService),
    kStopOfferServiceRequest = static_cast<std::uint8_t>(RequestType::kStopOfferService),
    kRegisterNotificationRequest = static_cast<std::uint8_t>(RequestType::kRegisterNotification),
    kUnregisterNotificationRequest = static_cast<std::uint8_t>(RequestType::kUnregisterNotification),

    // Responses
    kAckResponse = static_cast<std::uint8_t>(ResponseType::kAck),

    // Notifications
    kUpdateNotification = static_cast<std::uint8_t>(NotificationType::kUpdate),
    // Invalid message type, e.g. for uninitialized messages or in case of deserialization failures.
    kInvalid = 0xFFU
};

inline bool IsRequest(MessageType type)
{
    return type == MessageType::kProvideServiceRequest || type == MessageType::kOfferServiceRequest ||
           type == MessageType::kStopOfferServiceRequest || type == MessageType::kRegisterNotificationRequest ||
           type == MessageType::kUnregisterNotificationRequest;
}

inline bool IsNotification(MessageType type)
{
    return (type == MessageType::kUpdateNotification);
}

inline bool IsResponse(MessageType type)
{
    return type == MessageType::kAckResponse;
}

inline bool RequiresResponse(MessageType type)
{
    return IsRequest(type);
}

struct MessageHeader
{
    static constexpr std::size_t kWireSize = sizeof(MessageType) + sizeof(std::uint32_t) + sizeof(std::uint32_t);

    MessageType type{MessageType::kInvalid};
    std::uint32_t sequence{0U};
    std::uint32_t payload_size{0U};

    void SerializeToBuffer(std::uint8_t* buffer) const noexcept
    {
        std::size_t offset = 0U;
        std::memcpy(buffer + offset, &type, sizeof(type));
        offset += sizeof(type);
        std::memcpy(buffer + offset, &sequence, sizeof(sequence));
        offset += sizeof(sequence);
        std::memcpy(buffer + offset, &payload_size, sizeof(payload_size));
    }

    void DeserializeFromBuffer(const std::uint8_t* buffer) noexcept
    {
        std::size_t offset = 0U;
        std::memcpy(&type, buffer + offset, sizeof(type));
        offset += sizeof(type);
        std::memcpy(&sequence, buffer + offset, sizeof(sequence));
        offset += sizeof(sequence);
        std::memcpy(&payload_size, buffer + offset, sizeof(payload_size));
    }
};

class TransportMessage
{
  public:
    explicit TransportMessage(MessageType message_type) noexcept
    {
        header_.type = message_type;
    }

    virtual ~TransportMessage() = default;

    TransportMessage(const TransportMessage&) = delete;
    TransportMessage& operator=(const TransportMessage&) = delete;
    TransportMessage(TransportMessage&&) = default;
    TransportMessage& operator=(TransportMessage&&) = default;

    virtual std::size_t Serialize(score::cpp::span<std::uint8_t> buffer) const = 0;
    virtual bool Deserialize(score::cpp::span<const std::uint8_t> data) = 0;

    const MessageHeader& GetHeader() const
    {
        return header_;
    }

    MessageType GetType() const
    {
        return header_.type;
    }

    std::uint32_t GetSequenceNumber() const
    {
        return header_.sequence;
    }

    void SetSequenceNumber(std::uint32_t sequence)
    {
        header_.sequence = sequence;
    }

  protected:
    MessageHeader header_;
};

}  // namespace score::mw::com::gateway

#endif  // SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_TRANSPORT_MESSAGE_H_
