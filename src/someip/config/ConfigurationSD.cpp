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

#include "ConfigurationSD.h"
#include <functional>
#include <iostream>
#include <signal.h>
#include <sstream>

#include <exception/Exception.h>

namespace lgsomeip {

void ConfigurationSD::initialize() {
    // default value
    enabled_ = true;
    multicast_address_ = "224.244.224.245";
    port_ = 30490;
    vlan_priority_ = 0xff;
    protocol_ = "udp";
    initial_delay_min_ = 1050;
    initial_delay_max_ = 1450;
    repetitions_base_delay_ = 100;
    repetitions_max_ = 3;
    ttl_ = 3;
    cyclic_offer_delay_ = 1000;
    request_response_delay_ = 1500;
}

void ConfigurationSD::initialize(const rapidjson::Value& sd) {
    if (sd[kConfigEnable].IsBool()) {
        enabled_ = sd[kConfigEnable].GetBool();
    } else if (sd[kConfigEnable].IsString()) {
        enabled_ = strcmp(sd[kConfigEnable].GetString(), "true") == 0 ? true : false;
    }

    if (enabled_ != true) {
        return;
    }

    // Set initial value
    vlan_priority_ = 0xff;
    initial_delay_min_ = 1050;
    initial_delay_max_ = 1450;
    repetitions_base_delay_ = 100;
    repetitions_max_ = 3;
    ttl_ = 3;
    cyclic_offer_delay_ = 1000;
    request_response_delay_ = 1500;

    if (sd.HasMember(kConfigMulticast)) {
        multicast_address_ = sd[kConfigMulticast].GetString();
    }

    if (sd.HasMember(kConfigProtocol)) {
        protocol_ = sd[kConfigProtocol].GetString();
    }

    auto get_uint16 = [&](const char* member, std::uint16_t& value) -> void {
        if (sd.HasMember(member)) {
            if (sd[member].IsString()) {
                value = std::stoi(sd[member].GetString());
            } else if (sd[member].IsUint()) {
                value = static_cast<std::uint16_t>(sd[member].GetUint());
            }
        }
    };

    auto get_uint8 = [&](const char* member, std::uint8_t& value) -> void {
        if (sd.HasMember(member)) {
            if (sd[member].IsString()) {
                value = std::stoi(sd[member].GetString());
            } else if (sd[member].IsUint()) {
                value = static_cast<std::uint8_t>(sd[member].GetUint());
            }
        }
    };

    get_uint16(kConfigPort, port_);
    get_uint8(kConfigVlanPriority, vlan_priority_);
    get_uint16(kConfigInitialDelayMin, initial_delay_min_);
    get_uint16(kConfigInitialDelayMax, initial_delay_max_);
    get_uint16(kConfigRepetitionsBaseDelay, repetitions_base_delay_);
    get_uint16(kConfigRepetitionsMax, repetitions_max_);
    get_uint16(kConfigTtl, ttl_);
    get_uint16(kConfigCyclicOfferDelay, cyclic_offer_delay_);
    get_uint16(kConfigRequestResponseDelay, request_response_delay_);

    // Check if port and multicast address are valid
    if (port_ == 0 || multicast_address_.empty()) {
        std::ostringstream sout;
        sout << "Configuration Error! : Invalid port and multicast address [" << "port: " << port_
             << ", multicast address: " << multicast_address_ << "]";
        throw LSAR_CONFIGURATION_ERROR(sout.str());
    }
}

bool ConfigurationSD::is_enabled() const {
    return enabled_;
}

std::string ConfigurationSD::get_multicast() const {
    return multicast_address_;
}

std::string ConfigurationSD::get_protocol() const {
    return protocol_;
}

std::uint16_t ConfigurationSD::get_port() const {
    return port_;
}

std::uint8_t ConfigurationSD::get_vlan_priority() const {
    return vlan_priority_;
}

std::uint16_t ConfigurationSD::get_initial_delay_min() const {
    return initial_delay_min_;
}

std::uint16_t ConfigurationSD::get_initial_delay_max() const {
    return initial_delay_max_;
}

std::uint16_t ConfigurationSD::get_repetitions_base_delay() const {
    return repetitions_base_delay_;
}

std::uint16_t ConfigurationSD::get_repetitions_max() const {
    return repetitions_max_;
}

std::uint16_t ConfigurationSD::get_ttl() const {
    return ttl_;
}

std::uint16_t ConfigurationSD::get_cyclic_offer_delay() const {
    return cyclic_offer_delay_;
}

std::uint16_t ConfigurationSD::get_request_response_delay() const {
    return request_response_delay_;
}

} // namespace lgsomeip
