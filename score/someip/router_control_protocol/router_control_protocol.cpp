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
#include "score/someip/router_control_protocol/router_control_protocol.h"

#include "score/someip/router_control_protocol/wire_format.h"

#include <algorithm>

namespace score::someip::router_control_protocol
{
namespace
{

using detail::EncodedSize;
using detail::kEventCountOffset;
using detail::kEventOffset;
using detail::kFieldCountOffset;
using detail::kHeaderSize;
using detail::kIdSize;
using detail::kInstanceOffset;
using detail::kLocalPortOffset;
using detail::kMajorOffset;
using detail::kOpcodeOffset;
using detail::kProtocolVersionOffset;
using detail::kRemoteIpOffset;
using detail::kRemotePortOffset;
using detail::kReservedOffset;
using detail::kServiceOffset;

struct UsedParameters
{
    bool service_instance;
    bool event;
    bool local_port;
    bool remote_endpoint;
    bool major;
    bool events;
    bool fields;
};

constexpr bool IsKnownOpcode(const std::uint8_t value) noexcept
{
    return (value >= static_cast<std::uint8_t>(Opcode::kAddRemoteService)) &&
           (value <= static_cast<std::uint8_t>(Opcode::kShutdown));
}

constexpr UsedParameters UsedParametersOf(const Opcode opcode) noexcept
{
    switch (opcode)
    {
        case Opcode::kAddRemoteService:
            return UsedParameters{true, false, true, true, false, true, false};
        case Opcode::kAddLocalSubscription:
        case Opcode::kRemoveLocalSubscription:
            return UsedParameters{true, true, false, false, false, false, false};
        case Opcode::kAddLocalService:
            return UsedParameters{true, false, true, false, true, true, true};
        case Opcode::kAddRemoteSubscription:
        case Opcode::kRemoveRemoteSubscription:
            return UsedParameters{true, true, false, true, false, false, false};
        case Opcode::kRemoveRemoteService:
        case Opcode::kRemoveLocalService:
            return UsedParameters{true, false, false, false, false, false, false};
        case Opcode::kShutdown:
        default:
            return UsedParameters{false, false, false, false, false, false, false};
    }
}

void Put16(const score::cpp::span<std::uint8_t> buffer, const std::size_t offset, const std::uint16_t value) noexcept
{
    buffer[offset] = static_cast<std::uint8_t>(value & 0xFFU);
    buffer[offset + 1U] = static_cast<std::uint8_t>(value >> 8U);
}

std::uint16_t Get16(const score::cpp::span<const std::uint8_t> data, const std::size_t offset) noexcept
{
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[offset]) |
                                      static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[offset + 1U]) << 8U));
}

score::cpp::expected_blank<ErrorCode> Validate(const Message& message) noexcept
{
    if (!IsKnownOpcode(static_cast<std::uint8_t>(message.opcode)))
    {
        return score::cpp::make_unexpected(ErrorCode::kUnknownOpcode);
    }
    const UsedParameters used = UsedParametersOf(message.opcode);
    const bool remote_endpoint_set =
        (message.remote_port != 0U) ||
        std::any_of(message.remote_ip.cbegin(), message.remote_ip.cend(), [](const std::uint8_t byte) {
            return byte != 0U;
        });
    const bool unexpected =
        ((!used.service_instance) && ((message.service != 0U) || (message.instance != 0U))) ||
        ((!used.event) && (message.event != 0U)) || ((!used.local_port) && (message.local_port != 0U)) ||
        ((!used.remote_endpoint) && remote_endpoint_set) || ((!used.major) && (message.major != 0U)) ||
        ((!used.events) && (!message.events.empty())) || ((!used.fields) && (!message.fields.empty()));
    if (unexpected)
    {
        return score::cpp::make_unexpected(ErrorCode::kUnexpectedParameter);
    }
    const bool fields_in_events =
        std::all_of(message.fields.cbegin(), message.fields.cend(), [&message](const std::uint16_t field) {
            return std::find(message.events.cbegin(), message.events.cend(), field) != message.events.cend();
        });
    if (!fields_in_events)
    {
        return score::cpp::make_unexpected(ErrorCode::kFieldNotInEvents);
    }
    return {};
}

}  // namespace

score::cpp::expected<std::size_t, ErrorCode> Encode(const Message& message,
                                                    const score::cpp::span<std::uint8_t> buffer) noexcept
{
    const auto valid = Validate(message);
    if (!valid.has_value())
    {
        return score::cpp::make_unexpected(valid.error());
    }
    const std::size_t size = EncodedSize(message.events.size(), message.fields.size());
    if (buffer.size() < size)
    {
        return score::cpp::make_unexpected(ErrorCode::kBufferTooSmall);
    }

    buffer[kProtocolVersionOffset] = kProtocolVersion;
    buffer[kOpcodeOffset] = static_cast<std::uint8_t>(message.opcode);
    Put16(buffer, kServiceOffset, message.service);
    Put16(buffer, kInstanceOffset, message.instance);
    Put16(buffer, kEventOffset, message.event);
    Put16(buffer, kLocalPortOffset, message.local_port);
    Put16(buffer, kRemotePortOffset, message.remote_port);
    const auto remote_ip = buffer.subspan(kRemoteIpOffset, message.remote_ip.size());
    std::copy(message.remote_ip.cbegin(), message.remote_ip.cend(), remote_ip.begin());
    buffer[kMajorOffset] = message.major;
    buffer[kReservedOffset] = 0U;
    Put16(buffer, kEventCountOffset, static_cast<std::uint16_t>(message.events.size()));
    Put16(buffer, kFieldCountOffset, static_cast<std::uint16_t>(message.fields.size()));
    std::size_t offset{kHeaderSize};
    for (const std::uint16_t event : message.events)
    {
        Put16(buffer, offset, event);
        offset += kIdSize;
    }
    for (const std::uint16_t field : message.fields)
    {
        Put16(buffer, offset, field);
        offset += kIdSize;
    }
    return size;
}

score::cpp::expected<Message, ErrorCode> Decode(const score::cpp::span<const std::uint8_t> data) noexcept
{
    if (data.size() < kHeaderSize)
    {
        return score::cpp::make_unexpected(ErrorCode::kInvalidSize);
    }
    if (data[kProtocolVersionOffset] != kProtocolVersion)
    {
        return score::cpp::make_unexpected(ErrorCode::kUnsupportedVersion);
    }
    if (!IsKnownOpcode(data[kOpcodeOffset]))
    {
        return score::cpp::make_unexpected(ErrorCode::kUnknownOpcode);
    }
    if (data[kReservedOffset] != 0U)
    {
        return score::cpp::make_unexpected(ErrorCode::kReservedNotZero);
    }
    const std::size_t event_count = Get16(data, kEventCountOffset);
    const std::size_t field_count = Get16(data, kFieldCountOffset);
    if ((event_count > kMaxEvents) || (field_count > kMaxEvents))
    {
        return score::cpp::make_unexpected(ErrorCode::kTooManyEvents);
    }
    if (data.size() != EncodedSize(event_count, field_count))
    {
        return score::cpp::make_unexpected(ErrorCode::kInvalidSize);
    }

    Message message{};
    message.opcode = static_cast<Opcode>(data[kOpcodeOffset]);
    message.service = Get16(data, kServiceOffset);
    message.instance = Get16(data, kInstanceOffset);
    message.event = Get16(data, kEventOffset);
    message.local_port = Get16(data, kLocalPortOffset);
    message.remote_port = Get16(data, kRemotePortOffset);
    const auto remote_ip = data.subspan(kRemoteIpOffset, message.remote_ip.size());
    std::copy(remote_ip.begin(), remote_ip.end(), message.remote_ip.begin());
    message.major = data[kMajorOffset];
    std::size_t offset{kHeaderSize};
    for (std::size_t index = 0U; index < event_count; ++index)
    {
        message.events.push_back(Get16(data, offset));
        offset += kIdSize;
    }
    for (std::size_t index = 0U; index < field_count; ++index)
    {
        message.fields.push_back(Get16(data, offset));
        offset += kIdSize;
    }

    const auto valid = Validate(message);
    if (!valid.has_value())
    {
        return score::cpp::make_unexpected(valid.error());
    }
    return message;
}

}  // namespace score::someip::router_control_protocol
