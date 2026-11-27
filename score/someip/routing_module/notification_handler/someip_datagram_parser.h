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
#ifndef SCORE_SOMEIP_ROUTING_MODULE_NOTIFICATION_HANDLER_SOMEIP_DATAGRAM_PARSER_H
#define SCORE_SOMEIP_ROUTING_MODULE_NOTIFICATION_HANDLER_SOMEIP_DATAGRAM_PARSER_H

#include "score/someip/routing_module/notification_handler/someip_message_header_parser.h"

#include <score/expected.hpp>
#include <score/span.hpp>

#include <array>
#include <cstddef>
#include <optional>

namespace score::someip::routing_module
{

struct SomeipMessageFrame
{
    SomeipMessageHeader header;
    std::size_t payload_offset;
};

/// Iterates over the SOME/IP messages of a datagram, copying only each fixed-size header and leaving payload bytes in
/// the packet.
class SomeipDatagramParser final
{
  public:
    SomeipDatagramParser() noexcept;

    template <typename PacketReader>
    [[nodiscard]] score::cpp::expected<std::optional<SomeipMessageFrame>, SomeipHeaderParseError> NextMessage(
        const PacketReader& datagram) noexcept
    {
        const std::size_t datagram_size = datagram.Size();
        if (offset_ >= datagram_size)
        {
            return std::optional<SomeipMessageFrame>{};
        }
        const std::size_t remaining = datagram_size - offset_;
        if (remaining < kSomeipHeaderSize)
        {
            return score::cpp::make_unexpected(SomeipHeaderParseError::kInvalidHeaderSize);
        }
        std::array<std::uint8_t, kSomeipHeaderSize> header_bytes{};
        if (!datagram.Copy(offset_, header_bytes))
        {
            return score::cpp::make_unexpected(SomeipHeaderParseError::kPacketReadFailed);
        }
        const auto parsed_header = SomeipMessageHeaderParser::Parse(header_bytes, remaining);
        if (!parsed_header.has_value())
        {
            return score::cpp::make_unexpected(parsed_header.error());
        }
        const std::size_t payload_offset = offset_ + kSomeipHeaderSize;
        offset_ += parsed_header.value().message_size;
        return std::optional<SomeipMessageFrame>{SomeipMessageFrame{parsed_header.value(), payload_offset}};
    }

  private:
    std::size_t offset_{0U};
};

}  // namespace score::someip::routing_module

#endif  // SCORE_SOMEIP_ROUTING_MODULE_NOTIFICATION_HANDLER_SOMEIP_DATAGRAM_PARSER_H
