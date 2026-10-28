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

#ifndef LG_SOMEIP_SOMEIP_MESSAGE_SD_ENTRY_H
#define LG_SOMEIP_SOMEIP_MESSAGE_SD_ENTRY_H

#include <cstdint>

namespace lgsomeip {

class SDEntry {
public:
    SDEntry();
    SDEntry(std::uint8_t type) {
        type_ = type;
    }
    ~SDEntry();

    std::uint32_t get_length();

    std::uint32_t serialize(std::uint8_t* data);
    bool deserialize(std::uint8_t* data, std::uint32_t length);

public: // Common Item of Entity
    std::uint8_t get_type() const;
    std::uint8_t get_option1st_index() const;
    std::uint8_t get_option1st_count() const;
    std::uint8_t get_option2nd_index() const;
    std::uint8_t get_option2nd_count() const;
    std::uint16_t get_service_id() const;
    std::uint16_t get_instance_id() const;
    std::uint8_t get_major_version() const;
    std::uint32_t get_ttl() const;

    void set_type(std::uint8_t type);
    void set_option1st_index(std::uint8_t first_option_index);
    void set_option1st_count(std::uint8_t first_option_count);
    void set_option2nd_index(std::uint8_t second_option_index);
    void set_option2nd_count(std::uint8_t second_option_count);
    void set_service_id(std::uint16_t service_id);
    void set_instance_id(std::uint16_t instance_id);
    void set_major_version(std::uint8_t major_version);
    void set_ttl(std::uint32_t ttl);

public: // For FINDSERVICE and OFFERSERVICE
    std::uint32_t get_minor_version() const;
    void set_minor_version(std::uint32_t minor_version);

public: // For SUBSCRIBE and SUBSCRIBEACK
    std::uint8_t get_flag() const;
    std::uint16_t get_event_group_id() const;

    void set_flag(std::uint8_t flag);
    void set_event_group_id(std::uint16_t event_group_id);

public: // For CHECK LENGTH OF ENTRY
    bool get_wrong_length() const;

    void set_wrong_length(bool wrong_length);

private:
    std::uint8_t type_ = 0;
    std::uint8_t first_option_index_ = 0;
    std::uint8_t second_option_index_ = 0;
    std::uint8_t first_option_count_ = 0;
    std::uint8_t second_option_count_ = 0;

    std::uint16_t service_id_ = 0;
    std::uint16_t instance_id_ = 0;

    std::uint8_t major_version_ = 0;
    std::uint32_t ttl_ = 0;

    // For FINDSERVICE and OFFERSERVICE
    std::uint32_t minor_version_ = 0;

    // For SUBSCRIBE and SUBSCRIBEACK
    std::uint8_t flags_ = 0;
    std::uint16_t event_group_id_ = 0;

    // For CHECK LENGTH OF ENTRY
    bool wrong_length_ = false;
};

} // namespace lgsomeip

#endif // LG_SOMEIP_SOMEIP_MESSAGE_SD_ENTRY_H
