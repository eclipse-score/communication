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
#ifndef SCORE_SOMEIP_ROUTING_MODULE_LOLA_MAPPING_LOLA_MAPPING_H
#define SCORE_SOMEIP_ROUTING_MODULE_LOLA_MAPPING_LOLA_MAPPING_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>

/// LoLa representation of SOME/IP notifications, shared by the routing module and LoLa applications.
namespace score::someip::routing_module
{

constexpr std::size_t kMaxPayloadSize{1400U};

/// Sample of every SOME/IP event in both directions; transmitted samples only use size and payload.
struct EventSample
{
    std::uint32_t size;
    std::uint16_t session;
    std::uint8_t interface_version;
    std::uint8_t message_type;
    std::array<std::uint8_t, kMaxPayloadSize> payload;
};

static_assert(std::is_standard_layout_v<EventSample> && std::is_trivially_copyable_v<EventSample>,
              "EventSample is exchanged through LoLa shared memory");

/// LoLa instance specifier "someip/<service>/<instance>", IDs as four-digit lowercase hex.
[[nodiscard]] std::string LolaInstanceSpecifier(std::uint16_t service, std::uint16_t instance);

/// LoLa event name "event_<event>", the ID as four-digit lowercase hex.
[[nodiscard]] std::string LolaEventName(std::uint16_t event);

}  // namespace score::someip::routing_module

#endif  // SCORE_SOMEIP_ROUTING_MODULE_LOLA_MAPPING_LOLA_MAPPING_H
