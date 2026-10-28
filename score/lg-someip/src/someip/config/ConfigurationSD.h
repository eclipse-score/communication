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

#ifndef LG_SOMEIP_CONFIG_SERVICEDISCOVERY_H
#define LG_SOMEIP_CONFIG_SERVICEDISCOVERY_H

#include <cstdint>

#include "ConfigConstant.h"
#include "rapidjson/document.h"
#include <iostream>

namespace lgsomeip {

class ConfigurationSD {
public:
    ConfigurationSD() {}
    virtual ~ConfigurationSD() {}
    void initialize();
    void initialize(const rapidjson::Value& sd);
    bool is_enabled() const;
    std::string get_multicast() const;
    std::string get_protocol() const;
    std::uint16_t get_port() const;
    std::uint8_t get_vlan_priority() const;
    std::uint16_t get_initial_delay_min() const;
    std::uint16_t get_initial_delay_max() const;
    std::uint16_t get_repetitions_base_delay() const;
    std::uint16_t get_repetitions_max() const;
    std::uint16_t get_ttl() const;
    std::uint16_t get_cyclic_offer_delay() const;
    std::uint16_t get_request_response_delay() const;

private:
    bool enabled_{false};
    std::string multicast_address_;
    std::string protocol_;
    std::uint16_t port_{0};
    std::uint8_t vlan_priority_{0xff};
    std::uint16_t initial_delay_min_{100};
    std::uint16_t initial_delay_max_{300};
    std::uint16_t repetitions_base_delay_{300};
    std::uint16_t repetitions_max_{5};
    std::uint16_t ttl_{0};
    std::uint16_t cyclic_offer_delay_{0};
    std::uint16_t request_response_delay_{0};
};

} // namespace lgsomeip

#endif // LG_SOMEIP_CONFIG_SERVICEDISCOVERY_H
