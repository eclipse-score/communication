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

#ifndef LG_SOMEIP_APPLICATION_APPLICATION_COMMON_TYPES_H
#define LG_SOMEIP_APPLICATION_APPLICATION_COMMON_TYPES_H

#include <cstdint>

#include <functional>
#include <map>
#include <memory>
#include <message/Message.h>
#include <set>
#include <socket/Address.h>

namespace lgsomeip {

using available_t = std::map<std::uint16_t, std::map<std::uint16_t, std::map<std::uint8_t, std::uint32_t>>>;

// Service Information Management Types
using servicelist_t =
    std::map<std::uint16_t, std::map<std::uint16_t, std::map<std::uint8_t, std::map<std::uint32_t, std::uint8_t>>>>;
using eventlist_t =
    std::map<std::uint16_t, std::map<std::uint16_t, std::map<std::uint8_t, std::map<std::uint16_t, std::uint8_t>>>>;
using reqeventlist_t =
    std::map<std::uint16_t, std::map<std::uint16_t, std::map<std::uint16_t, std::map<std::uint16_t, std::uint8_t>>>>;

// Handler Types
// using message_handler_t = std::function<void(const std::uint8_t*, const std::uint32_t)>;
using message_handler_t = std::function<void(std::shared_ptr<Message>)>;
using service_availability_handler_t = std::function<void(std::uint16_t, std::uint16_t, bool)>;
using subscription_handler_t = std::function<bool(std::uint16_t, bool)>;
using async_subscription_handler_t = std::function<void(std::uint16_t, bool, std::function<void(const bool)>)>;
using subscription_status_handler_t =
    std::function<void(std::uint16_t, std::uint16_t, std::uint16_t, std::uint16_t, std::uint16_t)>;
using application_state_handler_t = std::function<void(std::uint16_t)>;
using error_handler_t = std::function<void(const uint16_t)>;

using epsilon_change_func_t = std::function<bool(const std::shared_ptr<Payload>&, const std::shared_ptr<Payload>&)>;

enum class SubscribeState : std::uint32_t { SUBSCRIBED, UPDATE, UNSUBSCRIBED };

// Service Management Types
struct RequestedSubscribe {
    SubscribeState state{SubscribeState::UNSUBSCRIBED};
    std::uint32_t ttl{0};

    std::uint32_t app_id{0};
    std::uint32_t request_id_received{0};
    std::uint32_t request_id_management{0};
    std::string ip_address;
    std::shared_ptr<lgsomeip::osabstraction::Address> tcp_address{nullptr}; // for external service
    std::shared_ptr<lgsomeip::osabstraction::Address> udp_address{nullptr}; // for external service
};

// Service Management Types
struct AvailableService {
    AvailableService() {}
    AvailableService(std::uint16_t app_id, bool in_config_file, std::uint8_t major_version, std::uint32_t minor_version,
                     std::uint32_t ttl, std::shared_ptr<lgsomeip::osabstraction::Address> tcp,
                     std::shared_ptr<lgsomeip::osabstraction::Address> udp,
                     std::shared_ptr<lgsomeip::osabstraction::Address> multicast = nullptr) {
        this->app_id = app_id;
        is_in_config_file = in_config_file;
        is_internal = (this->app_id != 0);
        major = major_version;
        minor = minor_version;
        this->ttl = ttl;
        tcp_address = tcp;
        udp_address = udp;
        this->multicast = multicast;
    }

    AvailableService(const AvailableService& obj) {
        app_id = obj.app_id;
        is_internal = obj.is_internal;
        is_in_config_file = obj.is_in_config_file;
        major = obj.major;
        minor = obj.minor;
        ttl = obj.ttl;
        tcp_address = obj.tcp_address;
        udp_address = obj.udp_address;
        multicast = obj.multicast;
    }

    std::uint32_t ttl{0};
    std::uint8_t major{0};
    std::uint32_t minor{0};

    std::uint32_t state{0};
    std::uint32_t timer{0};

    bool is_internal{false};
    bool is_in_config_file{false};
    std::uint16_t app_id{0};                                                // for internal service
    std::shared_ptr<lgsomeip::osabstraction::Address> tcp_address{nullptr}; // for external service
    std::shared_ptr<lgsomeip::osabstraction::Address> udp_address{nullptr}; // for external service
    std::shared_ptr<lgsomeip::osabstraction::Address> multicast{nullptr};   // for external service

    std::map<std::uint16_t, std::vector<struct RequestedSubscribe>> subscribe;
};

struct RequestedService {
    RequestedService(std::uint16_t app_id, std::uint8_t major_version, std::uint32_t minor_version) {
        this->app_id = app_id;
        major = major_version;
        minor = minor_version;
    }

    std::uint8_t major{0};
    std::uint32_t minor{0};
    std::uint16_t app_id{0};

    std::uint32_t state{0};
    std::uint32_t timer{0};
};

} // namespace lgsomeip

#endif // LG_SOMEIP_APPLICATION_APPLICATION_COMMON_TYPES_H
