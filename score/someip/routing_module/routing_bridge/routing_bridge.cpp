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
#include "score/someip/routing_module/routing_bridge/routing_bridge.h"

#include "score/someip/routing_module/udp_transport/udp_packet_reader.h"
#include "score/someip/routing_module/udp_transport/udp_transport.h"

namespace score::someip::routing_module
{

RoutingBridge::RoutingBridge(score::message_passing::IServerFactory& server_factory) noexcept
    : notification_handler_{}, routing_manager_{server_factory, notification_handler_, []() noexcept {
                                                    someip_transport_wakeup();
                                                }}
{
}

RoutingBridge::~RoutingBridge() noexcept = default;

score::cpp::expected<score::cpp::blank, score::os::Error> RoutingBridge::Start() noexcept
{
    return routing_manager_.Start();
}

void RoutingBridge::Poll() noexcept
{
    routing_manager_.ProcessPendingCommands();
}

void RoutingBridge::OnUdpDatagram(const std::uint16_t udp_local_port,
                                  const Endpoint& udp_sender,
                                  const void* const packet,
                                  const std::uint32_t size) noexcept
{
    notification_handler_.OnUdpDatagram(udp_local_port, udp_sender, UdpPacketReader{packet, size});
}

std::string ModuleConfigDirectory(const std::string_view module_path)
{
    const auto separator = module_path.rfind('/');
    const std::string_view module_directory =
        (separator == std::string_view::npos) ? std::string_view{"."} : module_path.substr(0U, separator);
    return std::string{module_directory} + "/../etc";
}

}  // namespace score::someip::routing_module
