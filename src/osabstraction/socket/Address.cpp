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

#include "Address.h"

namespace lgsomeip {
namespace osabstraction {

Address::Address() {}

Address::~Address() {}

struct sockaddr* Address::get_address() {
    return nullptr;
}

int Address::get_address_size() const {
    return 0;
}

uint16_t Address::get_port_address() const {
    return port_;
}

int Address::get_type() const {
    return -1;
}

bool Address::is_multicast() const {
    return multicast_;
}

bool Address::get_reliable() const {
    return reliable_;
}

void Address::set_reliable(bool reliable) {
    reliable_ = reliable;
}

void Address::set_port_address(uint16_t port) {
    port_ = port;
}

void Address::set_vlan_priority(uint8_t vlan_priority) {
    vlan_priority_ = vlan_priority;
}

uint8_t Address::get_vlan_priority() const {
    return vlan_priority_;
}

std::string Address::to_string() {
    return "Address";
}

bool operator==(const Address& left, const Address& right) {
    if (left.reliable_ == right.reliable_ && left.port_ == right.port_ && left.address_ == right.address_) {
        return true;
    }
    return false;
}

} // namespace osabstraction
} // namespace lgsomeip
