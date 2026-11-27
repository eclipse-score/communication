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
#ifndef SCORE_SOMEIP_ROUTING_MODULE_ROUTING_BRIDGE_ROUTING_BRIDGE_H
#define SCORE_SOMEIP_ROUTING_MODULE_ROUTING_BRIDGE_ROUTING_BRIDGE_H

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

/// Connects the C transport with the C++ routing module; routing state is accessed on the data-path thread only.
class RoutingBridge final
{
  public:
    explicit RoutingBridge(score::message_passing::IServerFactory& server_factory) noexcept;
    ~RoutingBridge() noexcept;
    RoutingBridge(const RoutingBridge&) = delete;
    RoutingBridge(RoutingBridge&&) = delete;
    RoutingBridge& operator=(const RoutingBridge&) = delete;
    RoutingBridge& operator=(RoutingBridge&&) = delete;

    /// Starts receiving routing commands; each one wakes the data-path thread.
    score::cpp::expected<score::cpp::blank, score::os::Error> Start() noexcept;

    /// Applies the routing commands received since the previous call.
    void Poll() noexcept;

    /// packet identifies a UDP datagram received on udp_local_port and is only used during the call.
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

namespace detail
{

[[nodiscard]] int StartRoutingModuleFromConfig(const std::string& lola_config) noexcept;

}  // namespace detail

}  // namespace score::someip::routing_module

#endif  // SCORE_SOMEIP_ROUTING_MODULE_ROUTING_BRIDGE_ROUTING_BRIDGE_H
