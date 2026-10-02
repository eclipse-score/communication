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

#include "ServiceInfo.h"
#include "ConfigConstant.h"
#include <exception/Exception.h>
#include <functional>
#include <iostream>
#include <memory>
#include <socket/IP4Address.h>
#include <socket/IP6Address.h>

namespace lgsomeip {

void ServiceInfo::initialize(const rapidjson::Value& service, std::string& ip, std::uint16_t ip_type) {
    multicast_addresses_ = std::make_shared<std::vector<std::shared_ptr<lgsomeip::osabstraction::Address>>>();

    // Load the service ID.
    if (service.HasMember(kConfigService)) {
        service_id_ = std::stoi(service[kConfigService].GetString(), nullptr, 16);
    } else {
        throw LSAR_CONFIGURATION_ERROR("field[service] must be included in service");
    }
    // Load the service name.
    if (service.HasMember(kConfigName)) {
        service_name_ = service[kConfigName].GetString();
    } else {
        service_name_ = "no-name";
    }
    // Load the instance ID.
    if (service.HasMember(kConfigInstance)) {
        instance_id_ = std::stoi(service[kConfigInstance].GetString(), nullptr, 16);
        has_instance_id_ = true;
    } else {
        throw LSAR_CONFIGURATION_ERROR("field[instance] must be included in service");
    }
    // Load the major service version.
    if (service.HasMember(kConfigMajorVersion)) {
        major_version_ = std::stoi(service[kConfigMajorVersion].GetString(), nullptr, 16);
        has_major_version_ = true;
    }

    // Load the minor service version.
    if (service.HasMember(kConfigMinorVersion)) {
        minor_version_ = std::stoul(service[kConfigMinorVersion].GetString(), nullptr, 16);
        has_minor_version_ = true;
    }

    // Load the minimum minor service version.
    if (service.HasMember(kConfigMinimumMinorVersion)) {
        minimum_minor_version_ = std::stoul(service[kConfigMinimumMinorVersion].GetString(), nullptr, 16);
        has_minimum_minor_version_ = true;
    }

    // Load the reliable transport configuration.
    if (service.HasMember(kConfigReliable)) {
        const rapidjson::Value& reliable = service[kConfigReliable];

        if (reliable.HasMember(kConfigPort)) {
            reliable_port_ = std::stoi(reliable[kConfigPort].GetString());
            if (service.HasMember(kConfigVlanPriority)) {
                vlan_priority_ = std::stoi(service[kConfigVlanPriority].GetString());
            }

            if (ip_type == 6) {
                reliable_address_ = std::make_shared<lgsomeip::osabstraction::IP6Address>();
            } else {
                reliable_address_ = std::make_shared<lgsomeip::osabstraction::IP4Address>();
            }
            reliable_address_->set_ip_address(ip);
            reliable_address_->set_reliable(true);
            reliable_address_->set_port_address(reliable_port_);
            reliable_address_->set_vlan_priority(vlan_priority_);
        }
        if (reliable.HasMember(kConfigMagicCookies)) {
            if (reliable[kConfigMagicCookies].IsBool()) {
                magic_cookies_enabled_ = reliable[kConfigMagicCookies].GetBool();
            } else {
                magic_cookies_enabled_ = strcmp(reliable[kConfigMagicCookies].GetString(), "true") == 0 ? true : false;
            }
        }
    }

    // Load the unreliable transport configuration.
    if (service.HasMember(kConfigUnreliable)) {
        unreliable_port_ = std::stoi(service[kConfigUnreliable].GetString());
        if (service.HasMember(kConfigVlanPriority)) {
            vlan_priority_ = std::stoi(service[kConfigVlanPriority].GetString());
        }

        if (ip_type == 6) {
            unreliable_address_ = std::make_shared<lgsomeip::osabstraction::IP6Address>();
        } else {
            unreliable_address_ = std::make_shared<lgsomeip::osabstraction::IP4Address>();
        }
        unreliable_address_->set_ip_address(ip);
        unreliable_address_->set_reliable(false);
        unreliable_address_->set_port_address(unreliable_port_);
        unreliable_address_->set_vlan_priority(vlan_priority_);
    }

    // Load SOME/IP-TP method and event identifiers.
    if (service.HasMember(kConfigSomeIpTp)) {
        const rapidjson::Value& t_pids = service[kConfigSomeIpTp];
        for (rapidjson::SizeType j = 0; j < t_pids.Size(); j++) {
            std::uint16_t methodeventid = std::stoi(t_pids[j].GetString(), nullptr, 16);
            tp_list_.push_back(methodeventid);
        }
    }

    // Load the multicast address configuration.
    if (service.HasMember(kConfigMulticast)) {
        const rapidjson::Value& multicast = service[kConfigMulticast];

        if (multicast.HasMember(kConfigMulticastAddress) && multicast.HasMember(kConfigPort)) {
            auto multicast_addr = multicast[kConfigMulticastAddress].GetString();
            auto multicast_port = std::stoi(multicast[kConfigPort].GetString());
            std::shared_ptr<lgsomeip::osabstraction::Address> multicast;
            if (ip_type == 6) {
                multicast = std::make_shared<lgsomeip::osabstraction::IP6Address>();
            } else {
                multicast = std::make_shared<lgsomeip::osabstraction::IP4Address>();
            }

            multicast->set_ip_address(multicast_addr);
            multicast->set_reliable(false);
            multicast->set_port_address(multicast_port);
            multicast_addresses_->push_back(multicast);
        }
    }

    // Load whether the service is provided locally.
    if (service.HasMember(kConfigIsProvider)) {
        if (service[kConfigIsProvider].IsBool()) {
            provider_ = service[kConfigIsProvider].GetBool();
        } else {
            provider_ = strcmp(service[kConfigIsProvider].GetString(), "true") == 0 ? true : false;
        }
    }

    // Load the secure-connection setting.
    if (service.HasMember(kConfigSecureConnection)) {
        if (service[kConfigSecureConnection].IsBool()) {
            secure_connection_ = service[kConfigSecureConnection].GetBool();
        } else {
            secure_connection_ = strcmp(service[kConfigSecureConnection].GetString(), "true") == 0 ? true : false;
        }
    }

    if (service.FindMember(kConfigEvents) != service.MemberEnd()) {
        std::uint16_t eventid;
        const rapidjson::Value& events = service[kConfigEvents];

        for (rapidjson::SizeType i = 0;
             i < events.Size() && events[i].FindMember(kConfigEvent) != events[i].MemberEnd(); i++) {
            eventid = std::stoi(events[i][kConfigEvent].GetString(), nullptr, 16);
            bool isfield = false;

            if (events[i].HasMember(kConfigIsField)) {
                if (events[i][kConfigIsField].IsBool()) {
                    isfield = events[i][kConfigIsField].GetBool();
                } else {
                    isfield = strcmp(events[i][kConfigIsField].GetString(), "true") == 0 ? true : false;
                }
            }

            std::uint32_t updatecycle = 0;
            if (events[i].HasMember(kConfigUpdateCycle)) {
                if (events[i][kConfigUpdateCycle].IsUint()) {
                    updatecycle = events[i][kConfigUpdateCycle].GetUint();
                } else if (events[i][kConfigUpdateCycle].IsString()) {
                    updatecycle = std::stoi(events[i][kConfigUpdateCycle].GetString());
                }
            }
            events_.emplace_back(eventid, isfield, updatecycle);
        }
    }

    // loading eventgroups of service
    if (service.FindMember(kConfigEventGroups) != service.MemberEnd()) {
        const rapidjson::Value& eventgroups = service[kConfigEventGroups];
        std::uint16_t eventgroup_id;
        for (rapidjson::SizeType i = 0;
             i < eventgroups.Size() && eventgroups[i].FindMember(kConfigEventGroup) != eventgroups[i].MemberEnd();
             i++) {
            eventgroup_id = std::stoi(eventgroups[i][kConfigEventGroup].GetString(), nullptr, 16);

            // create eventgroup object
            auto& eventgroup = event_groups_[eventgroup_id];
            eventgroup.event_group_id_ = eventgroup_id;

            // multicast address, port
            if (eventgroups[i].HasMember(kConfigMulticast)) {
                const rapidjson::Value& multicast = eventgroups[i][kConfigMulticast];

                if (multicast.HasMember(kConfigMulticastAddress) && multicast.HasMember(kConfigPort)) {
                    auto multicast_addr = multicast[kConfigMulticastAddress].GetString();
                    auto multicast_port = std::stoi(multicast[kConfigPort].GetString());

                    if (ip_type == 6) {
                        eventgroup.multicast_address_ = std::make_shared<lgsomeip::osabstraction::IP6Address>();
                    } else {
                        eventgroup.multicast_address_ = std::make_shared<lgsomeip::osabstraction::IP4Address>();
                    }

                    eventgroup.multicast_address_->set_ip_address(multicast_addr);
                    eventgroup.multicast_address_->set_reliable(false);
                    eventgroup.multicast_address_->set_port_address(multicast_port);
                    eventgroup.is_multicast_ = true;
                    multicast_addresses_->push_back(eventgroup.multicast_address_);
                }
            }

            // is_multicast
            if (eventgroups[i].HasMember(kConfigIsMulticast)) {
                if (eventgroups[i][kConfigIsMulticast].IsBool()) {
                    eventgroup.is_multicast_ = eventgroups[i][kConfigIsMulticast].GetBool();
                } else {
                    eventgroup.is_multicast_ =
                        strcmp(eventgroups[i][kConfigIsMulticast].GetString(), "true") == 0 ? true : false;
                }
            }

            // threshold
            if (eventgroups[i].HasMember(kConfigThreshold)) {
                if (eventgroups[i][kConfigThreshold].IsUint()) {
                    eventgroup.threshold_ = eventgroups[i][kConfigThreshold].GetUint();
                } else if (eventgroups[i][kConfigThreshold].IsString()) {
                    eventgroup.threshold_ = std::stoi(eventgroups[i][kConfigThreshold].GetString());
                }
            }

            if (eventgroup.threshold_ == 0) {
                eventgroup.is_multicast_ = false;
            }

            auto& events_arr = eventgroup.events_;
            const rapidjson::Value& events = eventgroups[i][kConfigEvents];
            for (rapidjson::SizeType j = 0; j < events.Size(); j++) {
                std::uint16_t eventid = std::stoi(events[j].GetString(), nullptr, 16);
                events_arr.push_back(eventid); // event_id was added to groups_by_event_[event_group_id]
                auto& temp = groups_by_event_[eventid];
                temp.push_back(eventgroup_id); // event_group_id was added to groups_by_event_[event_id]
            }
        }
    }
}

std::uint16_t ServiceInfo::get_instance_id() const {
    return instance_id_;
}

bool ServiceInfo::has_instance_id() const {
    return has_instance_id_;
}

std::uint8_t ServiceInfo::get_major_version() const {
    return major_version_;
}

bool ServiceInfo::has_major_version() const {
    return has_major_version_;
}

std::uint32_t ServiceInfo::get_minor_version() const {
    return minor_version_;
}

bool ServiceInfo::has_minor_version() const {
    return has_minor_version_;
}

std::uint32_t ServiceInfo::get_minimum_minor_version() const {
    return minimum_minor_version_;
}

bool ServiceInfo::has_minimum_minor_version() const {
    return has_minimum_minor_version_;
}

std::uint16_t ServiceInfo::get_reliable_port() const {
    return reliable_port_;
}

std::uint16_t ServiceInfo::get_unreliable_port() const {
    return unreliable_port_;
}

std::uint8_t ServiceInfo::get_vlan_priority() const {
    return vlan_priority_;
}

std::shared_ptr<lgsomeip::osabstraction::Address> ServiceInfo::get_reliable_address() const {
    return reliable_address_;
}

std::shared_ptr<lgsomeip::osabstraction::Address> ServiceInfo::get_unreliable_address() const {
    return unreliable_address_;
}

std::shared_ptr<std::vector<std::shared_ptr<lgsomeip::osabstraction::Address>>>
ServiceInfo::get_multicast_address() const {
    return multicast_addresses_;
}

std::uint16_t ServiceInfo::get_service_id() const {
    return service_id_;
}

std::string ServiceInfo::get_service_name() {
    return service_name_;
}

bool ServiceInfo::get_state_magic_cookies() const {
    return magic_cookies_enabled_;
}

ConfigEvent* ServiceInfo::get_event(std::uint16_t event_id) {
    for (auto it = events_.begin(); it != events_.end(); it++) {
        if (it->get_event_id() == event_id) {
            return &(*it);
        }
    }

    return nullptr;
}

std::vector<std::uint16_t>* ServiceInfo::get_event_group(std::uint16_t event_group_id) {
    auto it = event_groups_.find(event_group_id);
    if (it != event_groups_.end()) {
        return &((it->second).events_);
    } else {
        return nullptr;
    }
}

ConfigEventgroup* ServiceInfo::get_event_group_object(std::uint16_t event_group_id) {
    auto it = event_groups_.find(event_group_id);
    if (it != event_groups_.end()) {
        return &(it->second);
    } else {
        return nullptr;
    }
}

std::unordered_map<std::uint16_t, std::vector<std::uint16_t>>* ServiceInfo::get_events() {
    return &groups_by_event_;
}

std::vector<std::uint16_t> ServiceInfo::get_tp_list() {
    return tp_list_;
}

bool ServiceInfo::is_provider() const {
    return provider_;
}

bool ServiceInfo::is_secure_connection() const {
    return secure_connection_;
}

} // namespace lgsomeip
