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

#ifndef LG_SOMEIP_CONFIG_SERVICEINFO_H
#define LG_SOMEIP_CONFIG_SERVICEINFO_H

#include <cstdint>

#include "ConfigEvent.h"
#include "ConfigEventgroup.h"
#include "rapidjson/document.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include <iostream>
#include <unordered_map>
#include <vector>
#include <memory>
#include <socket/Address.h>

namespace lgsomeip {

class ServiceInfo {
public:
    ServiceInfo() {}
    ~ServiceInfo() {}

    void initialize(const rapidjson::Value& service, std::string& ip, std::uint16_t ip_type);

    std::uint16_t get_service_id() const;
    std::string get_service_name();
    std::uint16_t get_instance_id() const;
    bool has_instance_id() const;
    std::uint8_t get_major_version() const;
    bool has_major_version() const;
    std::uint32_t get_minor_version() const;
    bool has_minor_version() const;
    std::uint32_t get_minimum_minor_version() const;
    bool has_minimum_minor_version() const;

    std::uint16_t get_reliable_port() const;
    std::uint16_t get_unreliable_port() const;
    std::uint8_t get_vlan_priority() const;
    std::shared_ptr<lgsomeip::osabstraction::Address> get_reliable_address() const;
    std::shared_ptr<lgsomeip::osabstraction::Address> get_unreliable_address() const;
    std::shared_ptr<std::vector<std::shared_ptr<lgsomeip::osabstraction::Address>>> get_multicast_address() const;

    bool get_state_magic_cookies() const;

    std::vector<std::uint16_t>* get_event_group(std::uint16_t event_group_id);
    ConfigEventgroup* get_event_group_object(std::uint16_t event_group_id);
    std::unordered_map<std::uint16_t, std::vector<std::uint16_t>>* get_events();
    std::vector<std::uint16_t> get_tp_list();
    ConfigEvent* get_event(std::uint16_t event_id);
    bool is_provider() const;
    bool is_secure_connection() const;

private:
    std::vector<ConfigEvent> events_;
    std::unordered_map<std::uint16_t, ConfigEventgroup> event_groups_;
    std::unordered_map<std::uint16_t, std::vector<std::uint16_t>> groups_by_event_;

    // Manage TP method/event list
    // vector<method/eventid>>
    std::vector<std::uint16_t> tp_list_;

    std::uint16_t service_id_{0};
    std::string service_name_;
    std::uint16_t instance_id_{0};
    std::uint8_t major_version_{0};
    std::uint32_t minor_version_{0};
    std::uint32_t minimum_minor_version_{0};
    std::uint16_t reliable_port_{0};
    std::uint16_t unreliable_port_{0};
    std::uint8_t vlan_priority_{0xff};

    std::shared_ptr<lgsomeip::osabstraction::Address> reliable_address_{nullptr};
    std::shared_ptr<lgsomeip::osabstraction::Address> unreliable_address_{nullptr};
    std::shared_ptr<std::vector<std::shared_ptr<lgsomeip::osabstraction::Address>>> multicast_addresses_{nullptr};

    bool magic_cookies_enabled_{false};
    bool provider_{false};
    bool secure_connection_{false};
    bool has_instance_id_{false};
    bool has_major_version_{false};
    bool has_minor_version_{false};
    bool has_minimum_minor_version_{false};
};

} // namespace lgsomeip

#endif // LG_SOMEIP_CONFIG_SERVICEINFO_H
