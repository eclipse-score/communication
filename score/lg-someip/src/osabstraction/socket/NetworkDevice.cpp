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

#include <socket/NetworkDevice.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <memory>

#include <utils/log/logger.h>

namespace lgsomeip {
namespace osabstraction {

NetworkDevice NetworkDevice::device_;

bool NetworkDevice::initialize(std::string address, int ip_type) {
    initialized_ = false;
    device_name_.clear();
    device_address_.clear();
    device_id_ = 0;

    char buf_rcv_v6_addr[INET6_ADDRSTRLEN] = {0};
    char buf_dev_v6_addr[INET6_ADDRSTRLEN] = {0};
    struct sockaddr_in6 rcv_v6_addr;

    struct ifaddrs *ifaddr, *ifa;

    if (getifaddrs(&ifaddr) == -1) {
        LGSOMEIP_LOG_ERROR << "getifaddrs error";
        return false;
    }

    if (ip_type == 6) {
        inet_pton(AF_INET6, address.c_str(), &(rcv_v6_addr.sin6_addr));
        inet_ntop(AF_INET6, &rcv_v6_addr.sin6_addr, buf_rcv_v6_addr, INET6_ADDRSTRLEN);
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) {
            continue;
        }

        if (ip_type == 6 && ifa->ifa_addr->sa_family == AF_INET6) {
            struct sockaddr_in6* addr = (struct sockaddr_in6*)ifa->ifa_addr;

            inet_ntop(AF_INET6, &addr->sin6_addr, buf_dev_v6_addr, INET6_ADDRSTRLEN);
            if (strcmp(buf_rcv_v6_addr, buf_dev_v6_addr) == 0) {
                device_name_ = std::string(ifa->ifa_name);
                initialized_ = true;
                break;
            }
        } else if (ip_type == 4 && ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in* addr = (struct sockaddr_in*)ifa->ifa_addr;

            std::string ip_address{inet_ntoa(addr->sin_addr)};
            if (strcmp(inet_ntoa(addr->sin_addr), address.c_str()) == 0) {
                device_name_ = std::string(ifa->ifa_name);
                initialized_ = true;
                break;
            }
        }
    }

    freeifaddrs(ifaddr);

    if (initialized_) {
        device_id_ = if_nametoindex(device_name_.c_str());
        device_address_ = address;
    }

    return initialized_;
}

std::string NetworkDevice::get_device_name() const {
    if (initialized_)
        return device_name_;
    return std::string("");
}

std::string NetworkDevice::get_device_addr() const {
    if (initialized_)
        return device_address_;
    return std::string("");
}

int NetworkDevice::get_device_id() const {
    if (initialized_)
        return device_id_;
    return 0;
}

} // namespace osabstraction
} // namespace lgsomeip
