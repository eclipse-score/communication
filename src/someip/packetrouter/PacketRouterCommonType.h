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

#ifndef LG_SOMEIP_PACKET_ROUTER_COMMON_TYPE_H
#define LG_SOMEIP_PACKET_ROUTER_COMMON_TYPE_H

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include <endpoint/Endpoint.h>

namespace lgsomeip {

struct RoutingApplicationInfo {
    std::string name;
    std::shared_ptr<Endpoint> sender = nullptr;
    std::shared_ptr<Endpoint> receiver = nullptr;
};

struct RoutingConnectionInfo {
    std::shared_ptr<Endpoint> endpoint = nullptr;
    bool is_internal = false;
    std::uint16_t app_id = 0;
    std::int32_t serverfd = 0; // server endpoint fd in clinet endpoint
};

struct RoutingServiceInfo {
    std::uint16_t app_id = 0;                        // for internal
    std::shared_ptr<Endpoint> svcendpoint = nullptr; // for internal (only internal service)

    std::shared_ptr<Endpoint> tcpendpoint = nullptr; // for external receiver(tcp)
    std::shared_ptr<Endpoint> udpendpoint = nullptr; // for external receiver(udp)
    std::shared_ptr<lgsomeip::osabstraction::Address> udpserveraddress{nullptr};
    std::vector<std::shared_ptr<Endpoint>> multicasts; // for multicast Sender receiver(udp)

    std::map<std::uint16_t, std::vector<struct RoutingSubscribeInfo>> eventinfos;
};

struct RoutingSubscribeInfo {
    // bool isack = false;
    std::uint16_t app_id = 0;
    std::uint16_t eventgroupid = 0;
    std::shared_ptr<lgsomeip::osabstraction::Address> destAddr = nullptr;
    std::shared_ptr<Endpoint> endpoint = nullptr;
};

struct RoutingRequestPacket {
    std::uint32_t reqid{0};                      // external ReqID
    std::uint32_t ttl{0};                        // TTL
    std::shared_ptr<Endpoint> endpoint{nullptr}; // external Endpoint
    std::shared_ptr<lgsomeip::osabstraction::Address> targeaddr = nullptr;
};

} // namespace lgsomeip

#endif // LG_SOMEIP_PACKET_ROUTER_COMMON_TYPE_H
