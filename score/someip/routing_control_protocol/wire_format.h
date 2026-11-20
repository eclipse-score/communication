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
#ifndef SCORE_SOMEIP_ROUTING_CONTROL_PROTOCOL_WIRE_FORMAT_H
#define SCORE_SOMEIP_ROUTING_CONTROL_PROTOCOL_WIRE_FORMAT_H

#include <cstddef>
#include <cstdint>

/// Internal wire layout of a routing command; include routing_control_protocol.h instead.
/// Multi-byte integers are little endian, the IPv4 address is in network byte order.
namespace score::someip::routing_control_protocol::detail
{

constexpr std::size_t kIpv4AddressSize{4U};
constexpr std::size_t kIdSize{sizeof(std::uint16_t)};

constexpr std::size_t kProtocolVersionOffset{0U};
constexpr std::size_t kOpcodeOffset{kProtocolVersionOffset + sizeof(std::uint8_t)};
constexpr std::size_t kServiceOffset{kOpcodeOffset + sizeof(std::uint8_t)};
constexpr std::size_t kInstanceOffset{kServiceOffset + sizeof(std::uint16_t)};
constexpr std::size_t kEventOffset{kInstanceOffset + sizeof(std::uint16_t)};
constexpr std::size_t kLocalPortOffset{kEventOffset + sizeof(std::uint16_t)};
constexpr std::size_t kRemotePortOffset{kLocalPortOffset + sizeof(std::uint16_t)};
constexpr std::size_t kRemoteIpOffset{kRemotePortOffset + sizeof(std::uint16_t)};
constexpr std::size_t kMajorOffset{kRemoteIpOffset + kIpv4AddressSize};
constexpr std::size_t kReservedOffset{kMajorOffset + sizeof(std::uint8_t)};
constexpr std::size_t kEventCountOffset{kReservedOffset + sizeof(std::uint8_t)};
constexpr std::size_t kFieldCountOffset{kEventCountOffset + sizeof(std::uint16_t)};
constexpr std::size_t kHeaderSize{kFieldCountOffset + sizeof(std::uint16_t)};

// A layout change needs a new protocol version.
static_assert(kHeaderSize == 22U, "routing control protocol header layout changed");

constexpr std::size_t EncodedSize(const std::size_t event_count, const std::size_t field_count) noexcept
{
    return kHeaderSize + (kIdSize * (event_count + field_count));
}

}  // namespace score::someip::routing_control_protocol::detail

#endif  // SCORE_SOMEIP_ROUTING_CONTROL_PROTOCOL_WIRE_FORMAT_H
