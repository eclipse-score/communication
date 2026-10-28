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

#ifndef LG_SOMEIP_SOMEIP_MESSAGE_SD_OPTION_H
#define LG_SOMEIP_SOMEIP_MESSAGE_SD_OPTION_H

#include <cstdint>
#include <map>
#include <memory>

#include <socket/Address.h>
#include <socket/IP4Address.h>
#include <socket/IP6Address.h>

namespace lgsomeip {

class SDOption {
public:
    SDOption();
    SDOption(std::uint8_t type) {
        type_ = type;
    }
    ~SDOption();

    std::uint32_t get_length() const;

    std::uint32_t serialize(std::uint8_t* data);
    std::uint32_t deserialize(std::uint8_t* data, std::uint32_t data_length);

    friend bool operator==(const SDOption& lhs, const SDOption& rhs);

    // Common Option
public:
    void set_type(std::uint8_t type);
    std::uint8_t get_type() const;

private:
    std::uint16_t option_length_ = 0;
    std::uint8_t type_ = 0;

    // Addtional Option : Network Address
public:
    void set_address_option(std::shared_ptr<lgsomeip::osabstraction::Address> address);
    std::shared_ptr<lgsomeip::osabstraction::Address> get_address_option();

private:
    std::uint8_t address_bytes_[16] = {
        0,
    };
    std::uint8_t protocol_ = 0;
    std::uint16_t port_ = 0;

    // Addtional Option : LoadBalance
public:
    void set_priority(std::uint16_t priority);
    std::uint16_t get_priority();
    void set_weight(std::uint16_t weight);
    std::uint16_t get_weight();

private:
    std::uint16_t priority_ = 0;
    std::uint16_t weight_ = 0;

    // Addtional Option : Configuration
public:
    std::uint32_t get_configuration_count() const {
        return configuration_.size();
    }
    void set_configuration(std::string key, std::string value);
    std::string get_configuration(std::string key);
    void remove_configuration(std::string key);

private:
    std::map<std::string, std::string> configuration_;
};

} // namespace lgsomeip

#endif // LG_SOMEIP_SOMEIP_MESSAGE_SD_OPTION_H
