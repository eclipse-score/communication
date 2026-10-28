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

#include "IP6Address.h"
#include "NetworkDevice.h"
#include <cstring>
#include <string>

namespace lgsomeip {
namespace osabstraction {

IP6Address::IP6Address() {
    socket_address_.sin6_family = AF_INET6;
    socket_address_.sin6_addr = in6addr_any; // structure assignment
}

IP6Address::~IP6Address() {}

struct sockaddr* IP6Address::get_address() {
    socket_address_.sin6_port = htons(port_);
    socket_address_.sin6_scope_id = NetworkDevice::instance().get_device_id();

    return reinterpret_cast<struct sockaddr*>(&socket_address_);
}

int IP6Address::get_address_size() const {
    return sizeof(socket_address_);
}

std::string IP6Address::get_ip_address() const {
    return address_;
}

int IP6Address::get_type() const {
    return AF_INET6;
}

void IP6Address::set_ip_address(std::string address) {
    address_ = address;

    // IPv6 Address Exam = 2001:720:1500:1::a100
    inet_pton(AF_INET6, address_.c_str(), &(socket_address_.sin6_addr));

    multicast_ = ((std::uint16_t)socket_address_.sin6_addr.s6_addr[0] == 0xFF);
}

std::string IP6Address::to_string() {
    return address_ + ":" + std::to_string(port_) + (multicast_ ? " (multicast)" : " (unicast)");
}

bool operator==(const IP6Address& left, const IP6Address& right) {
    return left.reliable_ == right.reliable_ && left.port_ == right.port_ && left.address_ == right.address_;
}

} // namespace osabstraction
} // namespace lgsomeip
