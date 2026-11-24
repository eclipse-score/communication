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
#include "score/someip/routing_module/iosock_bridge/iosock_bridge.h"

#include "score/someip/routing_module/iosock_transport/iosock_packet_reader.h"
#include "score/someip/routing_module/iosock_transport/iosock_transport.h"

namespace score::someip::routing_module
{

IoSockBridge::IoSockBridge(score::message_passing::IServerFactory& server_factory) noexcept
    : notification_handler_{}, routing_manager_{server_factory, notification_handler_, []() noexcept {
                                                    someip_iosock_wakeup();
                                                }}
{
}

IoSockBridge::~IoSockBridge() noexcept = default;

score::cpp::expected_blank<score::os::Error> IoSockBridge::Start() noexcept
{
    return routing_manager_.Start();
}

void IoSockBridge::Poll() noexcept
{
    routing_manager_.ProcessPendingCommands();
}

void IoSockBridge::OnUdpDatagram(const std::uint16_t udp_local_port,
                                 const Endpoint& udp_sender,
                                 const void* const packet,
                                 const std::uint32_t size) noexcept
{
    notification_handler_.OnUdpDatagram(udp_local_port, udp_sender, IoSockPacketReader{packet, size});
}

std::string ModuleConfigDirectory(const std::string_view module_path)
{
    const auto separator = module_path.rfind('/');
    const std::string_view module_directory =
        (separator == std::string_view::npos) ? std::string_view{"."} : module_path.substr(0U, separator);
    return std::string{module_directory} + "/../etc";
}

}  // namespace score::someip::routing_module
