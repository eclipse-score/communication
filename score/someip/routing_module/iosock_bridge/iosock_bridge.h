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
#ifndef SCORE_SOMEIP_ROUTING_MODULE_IOSOCK_BRIDGE_IOSOCK_BRIDGE_H
#define SCORE_SOMEIP_ROUTING_MODULE_IOSOCK_BRIDGE_IOSOCK_BRIDGE_H

#include "score/someip/routing_module/notification_handler/notification_handler.h"
#include "score/someip/routing_module/routing_manager/i_routing_command_handler.h"
#include "score/someip/routing_module/routing_manager/routing_manager.h"

#include "score/message_passing/i_server_factory.h"
#include "score/os/errno.h"

#include <score/expected.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace score::someip::routing_module
{

/// Connects the io-sock module (C) with the routing module (C++). Routing commands arrive on message_passing threads;
/// they are applied, and received datagrams processed, on the module's kernel thread only.
class IoSockBridge final
{
  public:
    explicit IoSockBridge(score::message_passing::IServerFactory& server_factory) noexcept;
    ~IoSockBridge() noexcept;
    IoSockBridge(const IoSockBridge&) = delete;
    IoSockBridge(IoSockBridge&&) = delete;
    IoSockBridge& operator=(const IoSockBridge&) = delete;
    IoSockBridge& operator=(IoSockBridge&&) = delete;

    /// Starts receiving routing commands; each one wakes the module's kernel thread.
    score::cpp::expected_blank<score::os::Error> Start() noexcept;

    /// Applies the routing commands received since the previous call.
    void Poll() noexcept;

    /// packet is the io-sock packet of a UDP datagram received on udp_local_port; it is only used during the call.
    void OnUdpDatagram(std::uint16_t udp_local_port,
                       const Endpoint& udp_sender,
                       const void* packet,
                       std::uint32_t size) noexcept;

  private:
    NotificationHandler notification_handler_;
    RoutingManager routing_manager_;
};

/// Directory of the configuration files of the module at module_path: <prefix>/etc for <prefix>/bin/<module>.
[[nodiscard]] std::string ModuleConfigDirectory(std::string_view module_path);

}  // namespace score::someip::routing_module

#endif  // SCORE_SOMEIP_ROUTING_MODULE_IOSOCK_BRIDGE_IOSOCK_BRIDGE_H
