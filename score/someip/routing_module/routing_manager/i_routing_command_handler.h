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
#ifndef SCORE_SOMEIP_ROUTING_MODULE_ROUTING_MANAGER_I_ROUTING_COMMAND_HANDLER_H
#define SCORE_SOMEIP_ROUTING_MODULE_ROUTING_MANAGER_I_ROUTING_COMMAND_HANDLER_H

#include "score/someip/routing_control_protocol/routing_control_protocol.h"

#include <cstdint>

namespace score::someip::routing_module
{

enum class CommandResult : std::uint8_t
{
    kApplied = 0x00U,
    /// The command is invalid for the current routing state, which is left unchanged.
    kRejected = 0x01U,
};

struct ServiceInstance
{
    std::uint16_t service;
    std::uint16_t instance;
};

struct Endpoint
{
    routing_control_protocol::Ipv4Address address;
    std::uint16_t port;
};

/// Routing state of the data path. The RoutingManager calls it from the data path thread only.
class IRoutingCommandHandler
{
  public:
    IRoutingCommandHandler() noexcept = default;
    virtual ~IRoutingCommandHandler() = default;
    IRoutingCommandHandler(const IRoutingCommandHandler&) = delete;
    IRoutingCommandHandler(IRoutingCommandHandler&&) = delete;
    IRoutingCommandHandler& operator=(const IRoutingCommandHandler&) = delete;
    IRoutingCommandHandler& operator=(IRoutingCommandHandler&&) = delete;

    virtual CommandResult AddRemoteService(const routing_control_protocol::Message& command) noexcept = 0;
    virtual CommandResult RemoveRemoteService(ServiceInstance service_instance) noexcept = 0;
    virtual CommandResult AddLocalSubscription(ServiceInstance service_instance, std::uint16_t event) noexcept = 0;
    virtual CommandResult RemoveLocalSubscription(ServiceInstance service_instance, std::uint16_t event) noexcept = 0;
    virtual CommandResult AddLocalService(const routing_control_protocol::Message& command) noexcept = 0;
    virtual CommandResult RemoveLocalService(ServiceInstance service_instance) noexcept = 0;
    virtual CommandResult AddRemoteSubscription(ServiceInstance service_instance,
                                                std::uint16_t event,
                                                Endpoint subscriber) noexcept = 0;
    virtual CommandResult RemoveRemoteSubscription(ServiceInstance service_instance,
                                                   std::uint16_t event,
                                                   Endpoint subscriber) noexcept = 0;
    /// Removes all remote services, local services and subscriptions.
    virtual void RemoveAll() noexcept = 0;
};

}  // namespace score::someip::routing_module

#endif  // SCORE_SOMEIP_ROUTING_MODULE_ROUTING_MANAGER_I_ROUTING_COMMAND_HANDLER_H
