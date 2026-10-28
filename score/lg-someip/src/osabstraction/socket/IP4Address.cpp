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

#include "IP4Address.h"
#include <cstring>
#include <string>

namespace lgsomeip {
namespace osabstraction {

IP4Address::IP4Address() {
    socket_address_.sin_family = AF_INET;
    socket_address_.sin_addr.s_addr = htonl(INADDR_ANY);
}

IP4Address::~IP4Address() {}

struct sockaddr* IP4Address::get_address() {
    socket_address_.sin_port = htons(port_);
    return reinterpret_cast<struct sockaddr*>(&socket_address_);
}

int IP4Address::get_address_size() const {
    return sizeof(socket_address_);
}

std::string IP4Address::get_ip_address() const {
    return address_;
}

int IP4Address::get_type() const {
    return AF_INET;
}

void IP4Address::set_ip_address(std::string address) {
    address_ = address;
    socket_address_.sin_addr.s_addr = inet_addr(address_.c_str());

    multicast_ = (((ntohl(socket_address_.sin_addr.s_addr) & 0xFF000000) == 0xE0000000));
}

std::string IP4Address::to_string() {
    return address_ + ":" + std::to_string(port_) + (multicast_ ? " (multicast)" : " (unicast)");
}

bool operator==(const IP4Address& left, const IP4Address& right) {
    return left.reliable_ == right.reliable_ && left.port_ == right.port_ && left.address_ == right.address_;
}

} // namespace osabstraction
} // namespace lgsomeip
