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
#ifndef SCORE_SOMEIP_SD_CLIENT_SD_PROTOCOL_H
#define SCORE_SOMEIP_SD_CLIENT_SD_PROTOCOL_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace score::someip_sd::protocol
{

/// Name of the message_passing service offered by the SD daemon.
inline constexpr const char* kServiceIdentifier = "someip_sd";

inline constexpr std::uint8_t kVersion = 1U;

/// Minimal opcode-based control protocol between applications and the SD daemon.
/// To be aligned with the protocol of PoC safe-posix-platform#7492.
enum class Opcode : std::uint8_t
{
    // client -> daemon
    kRegister = 0x01U,  ///< SendWaitReply, the reply carries the assigned client id
    kRequestService = 0x02U,
    kReleaseService = 0x03U,
    kSubscribe = 0x04U,
    kUnsubscribe = 0x05U,
    kOfferService = 0x06U,
    kStopOfferService = 0x07U,
    // daemon -> client (Notify)
    kAvailability = 0x81U,       ///< flag: 1 = available, 0 = not available
    kSubscriptionState = 0x82U,  ///< flag: 1 = acknowledged, 0 = not acknowledged / lost
};

struct Message
{
    Opcode opcode{Opcode::kRegister};
    std::uint16_t service{0U};
    std::uint16_t instance{0U};
    std::uint8_t major{0U};
    std::uint8_t flag{0U};
    std::uint32_t minor{0U};
    std::uint16_t eventgroup{0U};
    std::uint16_t client_id{0U};
};

/// Fixed wire layout, little endian:
/// [0] version, [1] opcode, [2..3] service, [4..5] instance, [6] major, [7] flag,
/// [8..11] minor, [12..13] eventgroup, [14..15] client id
inline constexpr std::size_t kMessageSize = 16U;
using Buffer = std::array<std::uint8_t, kMessageSize>;

inline Buffer Encode(const Message& message) noexcept
{
    Buffer buffer{};
    const auto put16 = [&buffer](std::size_t pos, std::uint16_t value) {
        buffer[pos] = static_cast<std::uint8_t>(value & 0xFFU);
        buffer[pos + 1U] = static_cast<std::uint8_t>(value >> 8U);
    };
    buffer[0] = kVersion;
    buffer[1] = static_cast<std::uint8_t>(message.opcode);
    put16(2U, message.service);
    put16(4U, message.instance);
    buffer[6] = message.major;
    buffer[7] = message.flag;
    put16(8U, static_cast<std::uint16_t>(message.minor & 0xFFFFU));
    put16(10U, static_cast<std::uint16_t>(message.minor >> 16U));
    put16(12U, message.eventgroup);
    put16(14U, message.client_id);
    return buffer;
}

inline std::optional<Message> Decode(const std::uint8_t* data, std::size_t size) noexcept
{
    if ((data == nullptr) || (size != kMessageSize) || (data[0] != kVersion))
    {
        return std::nullopt;
    }
    const auto get16 = [data](std::size_t pos) {
        return static_cast<std::uint16_t>(data[pos] | static_cast<std::uint16_t>(data[pos + 1U] << 8U));
    };
    Message message{};
    message.opcode = static_cast<Opcode>(data[1]);
    message.service = get16(2U);
    message.instance = get16(4U);
    message.major = data[6];
    message.flag = data[7];
    message.minor = static_cast<std::uint32_t>(get16(8U)) | (static_cast<std::uint32_t>(get16(10U)) << 16U);
    message.eventgroup = get16(12U);
    message.client_id = get16(14U);
    return message;
}

}  // namespace score::someip_sd::protocol

#endif  // SCORE_SOMEIP_SD_CLIENT_SD_PROTOCOL_H
