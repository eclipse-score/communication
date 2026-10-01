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

#include "ConfigE2E.h"
#include "ConfigConstant.h"
#include <exception/Exception.h>

/*
    "e2e" :
    {
        "e2e_enabled" : "true",
        "protected" :
        [
            {
                "data_id" : "28",
                "service_id" : "0x7532",
                "event_id" : "0x8002",
                "variant" : "checker",
                "profile" : "CRC8",
                "crc_offset" : "0",
                "counter_offset" : "8",
                "data_id_mode" : "0",
                "data_id_nibble_offset" : "0",
                "data_length" : "56"
            }
        ]
    }
*/

namespace lgsomeip {

ConfigE2E::ConfigE2E(const rapidjson::Value& e2e, std::uint16_t service_id, std::uint16_t event_id)
    : service_id_(service_id), event_id_(event_id) {
    auto get_uint16 = [&](const char* member) -> std::uint16_t {
        if (e2e.HasMember(member)) {
            if (e2e[member].IsString()) {
                return std::stoi(e2e[member].GetString());
            } else if (e2e[member].IsUint()) {
                return static_cast<std::uint16_t>(e2e[member].GetUint());
            }
        } else {
            std::string member_name = member;
            throw LSAR_CONFIGURATION_ERROR(member_name + " must be included in e2e");
        }
        return 0;
    };

    data_id_ = get_uint16(kConfigDataId);
    crc_offset_ = get_uint16(kConfigCrcOffset);
    counter_offset_ = get_uint16(kConfigCounterOffset);
    data_id_mode_ = get_uint16(kConfigDataIdMode);
    data_id_nibble_offset_ = get_uint16(kConfigDataIdNibbleOffset);
    data_length_ = get_uint16(kConfigDataLength);

    if (e2e.HasMember(kConfigVariant)) {
        variant_ = e2e[kConfigVariant].GetString();
    } else {
        throw LSAR_CONFIGURATION_ERROR(std::string(kConfigVariant) + " must be included in e2e");
    }

    if (e2e.HasMember(kConfigProfile)) {
        profile_ = e2e[kConfigProfile].GetString();
    } else {
        throw LSAR_CONFIGURATION_ERROR(std::string(kConfigProfile) + " must be included in e2e");
    }
}

std::uint16_t ConfigE2E::get_data_id() const {
    return data_id_;
}

std::uint16_t ConfigE2E::get_service_id() const {
    return service_id_;
}

std::uint16_t ConfigE2E::get_event_id() const {
    return event_id_;
}

std::string ConfigE2E::get_variant() const {
    return variant_;
}

std::string ConfigE2E::get_profile() const {
    return profile_;
}

std::uint16_t ConfigE2E::get_crc_offset() const {
    return crc_offset_;
}

std::uint16_t ConfigE2E::get_counter_offset() const {
    return counter_offset_;
}

std::uint16_t ConfigE2E::get_data_id_mode() const {
    return data_id_mode_;
}

std::uint16_t ConfigE2E::get_data_id_nibble_offset() const {
    return data_id_nibble_offset_;
}

std::uint16_t ConfigE2E::get_data_length() const {
    return data_length_;
}

} // namespace lgsomeip
