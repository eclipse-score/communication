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
#include "score/someip/routing_module/event_handler/someip_message_header_parser.h"

namespace score::someip::routing_module
{
namespace
{

constexpr std::size_t kServiceIdOffset{0U};
constexpr std::size_t kMethodOrEventIdOffset{2U};
constexpr std::size_t kLengthOffset{4U};
constexpr std::size_t kClientIdOffset{8U};
constexpr std::size_t kSessionIdOffset{10U};
constexpr std::size_t kProtocolVersionOffset{12U};
constexpr std::size_t kInterfaceVersionOffset{13U};
constexpr std::size_t kMessageTypeOffset{14U};
constexpr std::size_t kReturnCodeOffset{15U};
constexpr std::size_t kLengthFieldSize{sizeof(std::uint32_t)};

std::uint16_t Read16(const score::cpp::span<const std::uint8_t> bytes, const std::size_t offset) noexcept
{
    const std::uint32_t value =
        (static_cast<std::uint32_t>(bytes[offset]) << 8U) | static_cast<std::uint32_t>(bytes[offset + 1U]);
    return static_cast<std::uint16_t>(value);
}

std::uint32_t Read32(const score::cpp::span<const std::uint8_t> bytes, const std::size_t offset) noexcept
{
    return (static_cast<std::uint32_t>(bytes[offset]) << 24U) |
           (static_cast<std::uint32_t>(bytes[offset + 1U]) << 16U) |
           (static_cast<std::uint32_t>(bytes[offset + 2U]) << 8U) | static_cast<std::uint32_t>(bytes[offset + 3U]);
}

}  // namespace

score::cpp::expected<SomeipMessageHeader, SomeipHeaderParseError> SomeipMessageHeaderParser::Parse(
    const score::cpp::span<const std::uint8_t> header,
    const std::size_t bytes_remaining_in_datagram) noexcept
{
    if ((header.size() != kSomeipHeaderSize) || (bytes_remaining_in_datagram < kSomeipHeaderSize))
    {
        return score::cpp::make_unexpected(SomeipHeaderParseError::kInvalidHeaderSize);
    }
    const std::uint8_t protocol_version = header[kProtocolVersionOffset];
    if (protocol_version != kSomeipProtocolVersion)
    {
        return score::cpp::make_unexpected(SomeipHeaderParseError::kUnsupportedProtocolVersion);
    }
    const std::uint32_t someip_length = Read32(header, kLengthOffset);
    if (someip_length < kSomeipMinimumLength)
    {
        return score::cpp::make_unexpected(SomeipHeaderParseError::kLengthTooShort);
    }
    const std::size_t length = someip_length;
    if (length > (bytes_remaining_in_datagram - kSomeipMinimumLength))
    {
        return score::cpp::make_unexpected(SomeipHeaderParseError::kMessageExceedsDatagram);
    }
    const std::size_t payload_size = length - kSomeipMinimumLength;
    return SomeipMessageHeader{Read16(header, kServiceIdOffset),
                               Read16(header, kMethodOrEventIdOffset),
                               Read16(header, kClientIdOffset),
                               Read16(header, kSessionIdOffset),
                               protocol_version,
                               header[kInterfaceVersionOffset],
                               header[kMessageTypeOffset],
                               header[kReturnCodeOffset],
                               length + kLengthFieldSize + kLengthOffset,
                               payload_size};
}

}  // namespace score::someip::routing_module
