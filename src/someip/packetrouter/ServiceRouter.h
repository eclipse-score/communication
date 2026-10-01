/********************************************************************************
 * Copyright (C) 2017-2026 LG Electronics Inc.
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ********************************************************************************/

#ifndef LG_SOMEIP_SERVICE_ROUTER_H
#define LG_SOMEIP_SERVICE_ROUTER_H

#include <cstdint>
#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

#include <endpoint/Endpoint.h>

namespace lgsomeip {

namespace osabstraction {
class Address;
class Multiplexer;
} // namespace osabstraction

class ServiceRouter {
public:
    virtual ~ServiceRouter() = default;

    virtual void init() = 0;
    virtual void start() = 0;
    virtual void stop() = 0;

    virtual std::shared_ptr<osabstraction::Multiplexer> get_multiplexer() = 0;
    virtual std::shared_ptr<Endpoint> get_service_discovery_multicast_endpoint() = 0;
    virtual std::shared_ptr<Endpoint> get_service_discovery_unicast_endpoint() = 0;
    virtual std::shared_ptr<osabstraction::Address> make_address() = 0;

    virtual void set_instance_id(std::uint16_t service_id, std::uint16_t instance_id, std::uint16_t app_id) = 0;
    virtual bool
    add_route(std::uint16_t service_id, std::uint16_t instance_id, std::uint16_t app_id,
              std::shared_ptr<osabstraction::Address> remote_tcp_address, std::uint16_t local_tcp_port,
              std::shared_ptr<osabstraction::Address> remote_udp_address, std::uint16_t local_udp_port,
              std::uint8_t vlan_priority,
              std::shared_ptr<std::vector<std::shared_ptr<osabstraction::Address>>> local_multicast_list) = 0;
    virtual void remove_route(std::uint16_t service_id, std::uint16_t instance_id) = 0;
    virtual std::int32_t find_connection(std::uint16_t service_id, std::uint16_t instance_id,
                                         std::shared_ptr<osabstraction::Address> address) = 0;

    virtual void add_subscribe_route(std::uint16_t service_id, std::uint16_t instance_id, std::uint16_t event_id,
                                     std::uint16_t app_id = 0,
                                     std::shared_ptr<osabstraction::Address> address = nullptr,
                                     std::uint16_t event_group_id = 0) = 0;
    virtual void remove_subscribe_route(std::uint16_t service_id, std::uint16_t instance_id, std::uint16_t event_id,
                                        std::uint16_t app_id, std::shared_ptr<osabstraction::Address> address) = 0;
    virtual void remove_subscribe_route(std::uint16_t service_id, std::uint16_t instance_id, std::uint16_t event_id,
                                        std::uint16_t app_id) = 0;

    virtual void send_internal_message(std::uint8_t* message, std::size_t message_length,
                                       std::uint16_t target_app_id) = 0;
    virtual void send_external_sd_message(std::uint8_t* message, std::size_t message_length,
                                          std::shared_ptr<osabstraction::Address> target_address, bool multicast) = 0;
    virtual void check_and_send_magic_cookies() = 0;
    virtual void reboot_route(std::shared_ptr<osabstraction::Address> address) = 0;

#if defined(ENABLE_QNX_MESSAGE_PASSING)
    virtual void message_passing_set_timer(const std::int32_t id, std::uint32_t interval_milliseconds, bool periodic,
                                           std::function<void(void)> handler) = 0;
    virtual void message_passing_kill_timer(const std::int32_t id) = 0;
#endif // ENABLE_QNX_MESSAGE_PASSING
};

} // namespace lgsomeip

#endif // LG_SOMEIP_SERVICE_ROUTER_H
