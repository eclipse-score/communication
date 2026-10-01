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

#ifndef LG_SOMEIP_CONFIG_CONFIGURATION_H
#define LG_SOMEIP_CONFIG_CONFIGURATION_H

#include "ConfigurationSD.h"
#include "ServiceInfo.h"
#include "ConfigE2E.h"
#include "rapidjson/document.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>
#include <e2exf/config.hpp>
#include <utils/log/logger.h>

namespace lgsomeip {

class Configuration {
public:
    Configuration(std::string path = "");
    ~Configuration();

    std::string get_address() const;
    std::uint16_t get_ip_type() const;
    int get_max_payload_size() const;
    std::uint16_t get_application_id(std::string name);
    ServiceInfo* get_service_info(std::uint16_t service_id, std::uint16_t instance_id);
    ConfigurationSD* get_service_discovery_info();
    bool is_e2e_enabled() const;
    std::map<vsomeip::e2exf::data_identifier, std::shared_ptr<ConfigE2E>> get_e2e_configs();
    bool has_config() const;

private:
    bool has_config_{false};
    void initialize(std::string path);
    bool e2e_enabled_{false};
    std::uint16_t ip_type_ = 4;
    int max_payload_size_ = 65536;
    std::string address_;
    std::string log_level_;
    bool log_console_enabled_{true};
    bool log_file_enabled_{false};
    std::string log_file_path_;
    // TODO : change ServiceInfo to shared_ptr<ServiceInfo>
    std::unordered_map<std::uint16_t, std::unordered_map<std::uint16_t, ServiceInfo>> services_;
    std::map<vsomeip::e2exf::data_identifier, std::shared_ptr<ConfigE2E>> e2e_configs_;
    ConfigurationSD service_discovery_;

private:
    std::unordered_map<std::string, std::uint16_t> applications_;

#if defined(ENABLE_SOMEIP_PACKET_FILTERING)
public:
    std::map<std::uint32_t, std::uint16_t> get_someip_packet_filter_list(void) {
        return packet_filter_list_;
    }
    void configure_someip_packet_filter(void);

private:
    std::map<std::uint32_t, std::uint16_t> packet_filter_list_;
#endif // ENABLE_SOMEIP_PACKET_FILTERING
};

} // namespace lgsomeip

#endif // LG_SOMEIP_CONFIG_CONFIGURATION_H
