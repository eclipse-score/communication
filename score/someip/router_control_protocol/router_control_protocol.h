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
#ifndef SCORE_SOMEIP_ROUTER_CONTROL_PROTOCOL_ROUTER_CONTROL_PROTOCOL_H
#define SCORE_SOMEIP_ROUTER_CONTROL_PROTOCOL_ROUTER_CONTROL_PROTOCOL_H

#include "score/someip/router_control_protocol/wire_format.h"

#include <score/expected.hpp>
#include <score/inplace_vector.hpp>
#include <score/span.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

/// Routing commands sent by the SD daemon to the routing module over score::message_passing (one direction,
/// no replies). The SD daemon runs SOME/IP-SD; the routing module only moves notifications, driven by these
/// commands.
namespace score::someip::router_control_protocol
{

/// score::message_passing service identifier of the routing module.
constexpr std::string_view kServiceIdentifier{"someip_routing_module"};

constexpr std::uint8_t kProtocolVersion{1U};
constexpr std::size_t kMaxEvents{64U};
constexpr std::size_t kMaxMessageSize{detail::EncodedSize(kMaxEvents, kMaxEvents)};

/// RX (remote provider -> local consumers) uses remote services and local subscriptions; TX (local provider ->
/// remote consumers) uses local services and remote subscriptions. A remote or local service sets up the data
/// path of a service instance; notifications of an event only flow once a local subscription (RX) or a remote
/// subscription (TX) for that event exists.
enum class Opcode : std::uint8_t
{
    /// RX: SOME/IP-SD found a remote offer of service/instance at remote_ip:remote_port. The routing module
    /// receives on local_port from that endpoint and offers the instance with events to local consumers.
    kAddRemoteService = 0x01U,
    /// RX: undoes kAddRemoteService (remote offer stopped or expired).
    kRemoveRemoteService = 0x02U,
    /// RX: the remote provider acknowledged the subscription of a local consumer. The routing module forwards
    /// received notifications of event to local consumers. Requires a remote service of service/instance.
    kAddLocalSubscription = 0x03U,
    /// RX: undoes kAddLocalSubscription (local consumer unsubscribed or subscription expired).
    kRemoveLocalSubscription = 0x04U,
    /// TX: a local application offers service/instance via SOME/IP. The routing module takes the events of the
    /// local instance and sends them from local_port with interface version major. fields lists the events
    /// that are fields, whose current value is sent to each new remote subscriber.
    kAddLocalService = 0x05U,
    /// TX: undoes kAddLocalService (local offer stopped).
    kRemoveLocalService = 0x06U,
    /// TX: the remote consumer remote_ip:remote_port subscribed event of a local service. The routing module
    /// sends notifications of event to it. Repeated on every renewal; a repetition changes nothing.
    kAddRemoteSubscription = 0x07U,
    /// TX: undoes kAddRemoteSubscription (remote consumer unsubscribed or subscription expired).
    kRemoveRemoteSubscription = 0x08U,
};

enum class ErrorCode : std::uint8_t
{
    kBufferTooSmall = 0x01U,
    kInvalidSize = 0x02U,
    kUnsupportedVersion = 0x03U,
    kUnknownOpcode = 0x04U,
    kReservedNotZero = 0x05U,
    kTooManyEvents = 0x06U,
    kUnexpectedParameter = 0x07U,
    kFieldNotInEvents = 0x08U,
};

using EventIds = score::cpp::inplace_vector<std::uint16_t, kMaxEvents>;
using Ipv4Address = std::array<std::uint8_t, detail::kIpv4AddressSize>;

/// Parameters not used by the opcode must be zero or empty; fields must be a subset of events.
struct Message
{
    Opcode opcode{Opcode::kAddRemoteService};
    std::uint16_t service{0U};
    std::uint16_t instance{0U};
    std::uint16_t event{0U};
    std::uint16_t local_port{0U};
    std::uint16_t remote_port{0U};
    Ipv4Address remote_ip{};
    std::uint8_t major{0U};
    EventIds events{};
    EventIds fields{};
};

/// Returns the number of bytes written to buffer; buffer is not modified on error.
[[nodiscard]] score::cpp::expected<std::size_t, ErrorCode> Encode(const Message& message,
                                                                  score::cpp::span<std::uint8_t> buffer) noexcept;

[[nodiscard]] score::cpp::expected<Message, ErrorCode> Decode(score::cpp::span<const std::uint8_t> data) noexcept;

}  // namespace score::someip::router_control_protocol

#endif  // SCORE_SOMEIP_ROUTER_CONTROL_PROTOCOL_ROUTER_CONTROL_PROTOCOL_H
