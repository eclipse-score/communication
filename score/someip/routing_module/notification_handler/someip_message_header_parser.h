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
#ifndef SCORE_SOMEIP_ROUTING_MODULE_NOTIFICATION_HANDLER_SOMEIP_MESSAGE_HEADER_PARSER_H
#define SCORE_SOMEIP_ROUTING_MODULE_NOTIFICATION_HANDLER_SOMEIP_MESSAGE_HEADER_PARSER_H

#include <score/expected.hpp>
#include <score/span.hpp>

#include <cstddef>
#include <cstdint>

namespace score::someip::routing_module
{

constexpr std::size_t kSomeipHeaderSize{16U};
constexpr std::uint8_t kSomeipProtocolVersion{1U};
constexpr std::uint8_t kSomeipNotificationMessageType{0x02U};
constexpr std::uint32_t kSomeipMinimumLength{8U};  // header part included in 'length' field of the message

enum class SomeipHeaderParseError : std::uint8_t
{
    kInvalidHeaderSize = 1U,
    kUnsupportedProtocolVersion = 2U,
    kLengthTooShort = 3U,
    kMessageExceedsDatagram = 4U,
    kPacketReadFailed = 5U,
};

struct SomeipMessageHeader
{
    std::uint16_t service_id;
    std::uint16_t method_or_event_id;
    std::uint16_t client_id;
    std::uint16_t session_id;
    std::uint8_t protocol_version;
    std::uint8_t interface_version;
    std::uint8_t message_type;
    std::uint8_t return_code;
    std::size_t message_size;
    std::size_t payload_size;
};

class SomeipMessageHeaderParser final
{
  public:
    [[nodiscard]] static score::cpp::expected<SomeipMessageHeader, SomeipHeaderParseError> Parse(
        score::cpp::span<const std::uint8_t> header,
        std::size_t bytes_remaining_in_datagram) noexcept;
};

}  // namespace score::someip::routing_module

#endif  // SCORE_SOMEIP_ROUTING_MODULE_NOTIFICATION_HANDLER_SOMEIP_MESSAGE_HEADER_PARSER_H
