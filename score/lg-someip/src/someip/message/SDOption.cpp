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

#include <socket/Address.h>
#include <socket/IP4Address.h>
#include <socket/IP6Address.h>

#include <message/MessageConstant.h>
#include <message/SDOption.h>
#include <utils/byteorder/bytestream.h>
#include <utils/log/logger.h>

#include <sstream>
#include <cstring>
#include <iterator>
#include <vector>

namespace lgsomeip {

SDOption::SDOption() {}

SDOption::~SDOption() {}

std::uint32_t SDOption::get_length() const {
    return option_length_ + SOMEIP_SD_OPTION::HEADER::SIZE;
}

std::uint32_t SDOption::deserialize(std::uint8_t* data, std::uint32_t data_length) {
    if (data == nullptr || data_length < SOMEIP_SD_OPTION::HEADER::SIZE) {
        LGSOMEIP_LOG_ERROR << "SDOption::deserialize / length : " << data_length << " is wrong";
        return 0;
    }

    std::uint16_t index, option_length = 0;
    std::uint8_t type = 0;

    get_byte_stream(&option_length, data);
    get_byte_stream(&type, data + SOMEIP_SD_OPTION::HEADER::POS::TYPE);
    const std::uint32_t serialized_length = option_length + SOMEIP_SD_OPTION::HEADER::SIZE;
    if (serialized_length > data_length) {
        LGSOMEIP_LOG_ERROR << "SDOption::deserialize / option length is wrong";
        return 0;
    }

    switch (type) {
    case SOMEIP_SD_OPTION::CONFIGURATION::TYPEID: {
        option_length_ = option_length;
        type_ = type;

        std::uint32_t pos = SOMEIP_SD_OPTION::CONFIGURATION::POS::OPTIONS;
        while (pos < serialized_length) {
            const std::uint8_t string_length = data[pos++];
            if (string_length == 0) {
                break;
            }

            if (string_length > serialized_length - pos) {
                return 0;
            }

            const std::uint32_t field_end = pos + string_length;
            std::uint32_t separator = pos;
            while (separator < field_end && data[separator] != '=') {
                ++separator;
            }
            if (separator == field_end) {
                return 0;
            }

            set_configuration(
                std::string(reinterpret_cast<const char*>(data + pos), separator - pos),
                std::string(reinterpret_cast<const char*>(data + separator + 1), field_end - separator - 1));
            pos = field_end;
        }

        break;
    }

    case SOMEIP_SD_OPTION::LOADBALANCING::TYPEID:
        option_length_ = option_length;
        type_ = type;
        get_byte_stream(&priority_, data + SOMEIP_SD_OPTION::LOADBALANCING::POS::PRIORITY);
        get_byte_stream(&weight_, data + SOMEIP_SD_OPTION::LOADBALANCING::POS::WEIGHT);
        break;

    case SOMEIP_SD_OPTION::IP4::TYPEID:
    case SOMEIP_SD_OPTION::IP4MULTI::TYPEID:
        option_length_ = option_length;
        type_ = type;
        if (option_length != 0x09) { // static value
            return 0;
        }
        get_byte_stream(&protocol_, data + SOMEIP_SD_OPTION::IP4::POS::PROTOCOL);
        get_byte_stream(&port_, data + SOMEIP_SD_OPTION::IP4::POS::PORT);

        for (index = 0; index < 4; index++) {
            address_bytes_[index] = data[index + SOMEIP_SD_OPTION::IP4::POS::ADDRESS];
        }
        break;

    case SOMEIP_SD_OPTION::IP6::TYPEID:
    case SOMEIP_SD_OPTION::IP6MULTI::TYPEID:
        if (option_length != 0x15) { // static value
            return 0;
        }
        option_length_ = option_length;
        type_ = type;
        get_byte_stream(&protocol_, data + SOMEIP_SD_OPTION::IP6::POS::PROTOCOL);
        get_byte_stream(&port_, data + SOMEIP_SD_OPTION::IP6::POS::PORT);

        for (index = 0; index < 16; index++) {
            address_bytes_[index] = data[index + SOMEIP_SD_OPTION::IP6::POS::ADDRESS];
        }
        break;

    default:
        LGSOMEIP_LOG_ERROR << "SDOption::deserialize / type : " << type << " is wrong";
        return 0;
    }

    return serialized_length;
}

std::uint32_t SDOption::serialize(std::uint8_t* data) {
    std::uint32_t index = 0, serialized_length = 0;
    for (index = 0; index < option_length_ + SOMEIP_SD_OPTION::HEADER::SIZE; index++) {
        data[index] = 0;
    }

    set_byte_stream(data, &option_length_);
    set_byte_stream(data + SOMEIP_SD_OPTION::HEADER::POS::TYPE, &type_);

    switch (type_) {
    case SOMEIP_SD_OPTION::CONFIGURATION::TYPEID: {
        std::vector<std::uint8_t> buffer;
        for (auto item = configuration_.begin(); item != configuration_.end(); item++) {
            buffer.push_back(static_cast<std::uint8_t>(item->first.size() + item->second.size() + 1));
            buffer.insert(buffer.end(), item->first.begin(), item->first.end());
            buffer.push_back(static_cast<std::uint8_t>('='));
            buffer.insert(buffer.end(), item->second.begin(), item->second.end());
        }
        buffer.push_back(0);
        std::copy(buffer.begin(), buffer.end(), data + SOMEIP_SD_OPTION::CONFIGURATION::POS::OPTIONS);
        serialized_length = buffer.size() + SOMEIP_SD_OPTION::HEADER::SIZE + 1;
        break;
    }

    case SOMEIP_SD_OPTION::LOADBALANCING::TYPEID:
        set_byte_stream(data + SOMEIP_SD_OPTION::LOADBALANCING::POS::PRIORITY, &priority_);
        set_byte_stream(data + SOMEIP_SD_OPTION::LOADBALANCING::POS::WEIGHT, &weight_);

        serialized_length = SOMEIP_SD_OPTION::LOADBALANCING::SIZE;
        break;

    case SOMEIP_SD_OPTION::IP4::TYPEID:
    case SOMEIP_SD_OPTION::IP4MULTI::TYPEID:
        set_byte_stream(data + SOMEIP_SD_OPTION::IP4::POS::PROTOCOL, &protocol_);
        set_byte_stream(data + SOMEIP_SD_OPTION::IP4::POS::PORT, &port_);

        for (index = 0; index < 4; index++) {
            data[index + SOMEIP_SD_OPTION::IP4::POS::ADDRESS] = address_bytes_[index];
        }

        serialized_length = SOMEIP_SD_OPTION::IP4::SIZE;
        break;

    case SOMEIP_SD_OPTION::IP6::TYPEID:
    case SOMEIP_SD_OPTION::IP6MULTI::TYPEID:
        set_byte_stream(data + SOMEIP_SD_OPTION::IP6::POS::PROTOCOL, &protocol_);
        set_byte_stream(data + SOMEIP_SD_OPTION::IP6::POS::PORT, &port_);

        for (index = 0; index < 16; index++) {
            data[index + SOMEIP_SD_OPTION::IP6::POS::ADDRESS] = address_bytes_[index];
        }

        serialized_length = SOMEIP_SD_OPTION::IP6::SIZE;
        break;

    default:
        break;
    }

    return serialized_length;
}

// Common Option
void SDOption::set_type(std::uint8_t type) {
    type_ = type;
}

std::uint8_t SDOption::get_type() const {
    return type_;
}

// Addtional Option : Network Address
void SDOption::set_address_option(std::shared_ptr<lgsomeip::osabstraction::Address> address) {
    std::uint32_t index = 0;
    std::uint32_t address_type;

    address_type = address->get_type();
    switch (address_type) {
    case AF_INET: {
        option_length_ = SOMEIP_SD_OPTION::IP4::SIZE - SOMEIP_SD_OPTION::HEADER::SIZE;
        if (type_ == 0) {
            type_ = SOMEIP_SD_OPTION::IP4::TYPEID;
        }

        protocol_ = address->get_reliable() ? /* TCP */ 0x06 : /* UDP */ 0x11;
        port_ = address->get_port_address();

        std::uint32_t ip_address = inet_addr(address->get_ip_address().c_str());
        std::uint8_t* address_bytes = reinterpret_cast<std::uint8_t*>(&ip_address);
        for (index = 0; index < 4; index++) {
            address_bytes_[index] = address_bytes[index];
        }
        break;
    }

    case AF_INET6:
        option_length_ = SOMEIP_SD_OPTION::IP6::SIZE - SOMEIP_SD_OPTION::HEADER::SIZE;
        if (type_ == 0) {
            type_ = SOMEIP_SD_OPTION::IP6::TYPEID;
        }

        protocol_ = address->get_reliable() ? /* TCP */ 0x06 : /* UDP */ 0x11;
        port_ = address->get_port_address();

        struct in6_addr sin6_address;
        inet_pton(AF_INET6, address->get_ip_address().c_str(), &sin6_address);
        for (index = 0; index < 16; index++) {
            address_bytes_[index] = sin6_address.s6_addr[index];
        }
        break;

    case AF_UNIX:
        // TODO(lg-someip): Implement SD options for local addresses.
        break;

    default:
        // TODO(lg-someip): Throw a runtime exception for unsupported address types.
        break;
    }
}

std::shared_ptr<lgsomeip::osabstraction::Address> SDOption::get_address_option() {
    std::shared_ptr<lgsomeip::osabstraction::Address> result = nullptr;

    switch (type_) {
    case SOMEIP_SD_OPTION::IP4::TYPEID:
    case SOMEIP_SD_OPTION::IP4MULTI::TYPEID: {
        struct sockaddr_in sockaddr;
        sockaddr.sin_addr.s_addr = *reinterpret_cast<std::uint32_t*>(address_bytes_);
        char* str = inet_ntoa(sockaddr.sin_addr);

        result = std::make_shared<lgsomeip::osabstraction::IP4Address>();
        result->set_ip_address(str);
        result->set_reliable(protocol_ == /* TCP */ 0x11 ? false : true);
        result->set_port_address(port_);

        break;
    }

    case SOMEIP_SD_OPTION::IP6::TYPEID:
    case SOMEIP_SD_OPTION::IP6MULTI::TYPEID:
        char ipv6_address_string[40];
        inet_ntop(AF_INET6, (void*)&address_bytes_, ipv6_address_string, sizeof(ipv6_address_string));

        result = std::make_shared<lgsomeip::osabstraction::IP6Address>();
        result->set_ip_address(ipv6_address_string);
        result->set_reliable(protocol_ == /* TCP */ 0x11 ? false : true);
        result->set_port_address(port_);
        break;

    default:
        // TODO(lg-someip): Throw a runtime exception for unsupported option types.
        break;
    }

    return result;
}

// Addtional Option : LoadBalance
void SDOption::set_priority(std::uint16_t priority) {
    if (type_ == SOMEIP_SD_OPTION::LOADBALANCING::TYPEID) {
        priority_ = priority;
        option_length_ = SOMEIP_SD_OPTION::LOADBALANCING::SIZE - SOMEIP_SD_OPTION::HEADER::SIZE;
    }
}

std::uint16_t SDOption::get_priority() {
    if (type_ == SOMEIP_SD_OPTION::LOADBALANCING::TYPEID) {
        return priority_;
    } else {
        return -1;
    }
}

void SDOption::set_weight(std::uint16_t weight) {
    if (type_ == SOMEIP_SD_OPTION::LOADBALANCING::TYPEID) {
        weight_ = weight;
        option_length_ = SOMEIP_SD_OPTION::LOADBALANCING::SIZE - SOMEIP_SD_OPTION::HEADER::SIZE;
    }
}

std::uint16_t SDOption::get_weight() {
    if (type_ == SOMEIP_SD_OPTION::LOADBALANCING::TYPEID) {
        return weight_;
    } else {
        return -1;
    }
}

// Addtional Option : Configuration
void SDOption::set_configuration(std::string key, std::string value) {
    if (type_ == SOMEIP_SD_OPTION::CONFIGURATION::TYPEID) {
        configuration_[key] = value;
    }

    std::uint16_t serialized_length = 0;
    for (auto& item : configuration_) {
        serialized_length += item.first.size() + item.second.size() + 2;
    }

    // add 2 byte to length (Rev + End)
    option_length_ = serialized_length + 2;
}

std::string SDOption::get_configuration(std::string key) {
    if (type_ == SOMEIP_SD_OPTION::CONFIGURATION::TYPEID) {
        auto iterator = configuration_.find(key);
        if (iterator == configuration_.end())
            return std::string();
        return iterator->second;
    }
    return std::string();
}

void SDOption::remove_configuration(std::string key) {
    if (type_ == SOMEIP_SD_OPTION::CONFIGURATION::TYPEID) {
        auto iterator = configuration_.find(key);
        if (iterator != configuration_.end()) {
            configuration_.erase(key);
        }

        std::uint16_t serialized_length = 0;
        for (auto& item : configuration_) {
            serialized_length += item.first.size() + item.second.size() + 2;
        }
        option_length_ = serialized_length + 2;
    }
}

bool operator==(const SDOption& lhs, const SDOption& rhs) {
    int address_matches = false;
    if (lhs.type_ != rhs.type_) {
        return false;
    }

    std::uint8_t type = lhs.type_;

    switch (type) {
    case SOMEIP_SD_OPTION::IP4::TYPEID:
    case SOMEIP_SD_OPTION::IP4MULTI::TYPEID:
        address_matches = memcmp(lhs.address_bytes_, rhs.address_bytes_, 4);
        return (lhs.port_ == rhs.port_) && (lhs.protocol_ == rhs.protocol_) && address_matches == 0;
    case SOMEIP_SD_OPTION::IP6::TYPEID:
    case SOMEIP_SD_OPTION::IP6MULTI::TYPEID:
        address_matches = memcmp(lhs.address_bytes_, rhs.address_bytes_, 16);
        return (lhs.port_ == rhs.port_) && (lhs.protocol_ == rhs.protocol_) && address_matches == 0;

    default:
        // TODO(lg-someip): Throw a runtime exception for unsupported option types.
        break;
    }
    return false;
}

} // namespace lgsomeip
