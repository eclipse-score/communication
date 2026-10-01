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

#ifndef LG_SOMEIP_CONFIG_E2E_H
#define LG_SOMEIP_CONFIG_E2E_H

#include <cstdint>

#include "ConfigConstant.h"
#include "rapidjson/document.h"
#include <iostream>

namespace lgsomeip {

class ConfigE2E {
public:
    ConfigE2E(const rapidjson::Value& e2e, std::uint16_t service_id, std::uint16_t event_id);
    ~ConfigE2E() {}
    std::uint16_t get_data_id() const;
    std::uint16_t get_service_id() const;
    std::uint16_t get_event_id() const;
    std::string get_variant() const;
    std::string get_profile() const;
    std::uint16_t get_crc_offset() const;
    std::uint16_t get_counter_offset() const;
    std::uint16_t get_data_id_mode() const;
    std::uint16_t get_data_id_nibble_offset() const;
    std::uint16_t get_data_length() const;

private:
    std::uint16_t data_id_{0};
    std::uint16_t service_id_{0};
    std::uint16_t event_id_{0};
    std::string variant_;
    std::string profile_;
    std::uint16_t crc_offset_{0};
    std::uint16_t counter_offset_{0};
    std::uint16_t data_id_mode_{0};
    std::uint16_t data_id_nibble_offset_{0};
    std::uint16_t data_length_{0};
};

} // namespace lgsomeip

#endif // LG_SOMEIP_CONFIG_E2E_H
