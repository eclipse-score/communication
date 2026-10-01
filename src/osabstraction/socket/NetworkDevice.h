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

#ifndef LG_SOMEIP_OSABSTRACTION_NETWORK_DEVICE
#define LG_SOMEIP_OSABSTRACTION_NETWORK_DEVICE

#include <arpa/inet.h>
#include <iostream>
#include <memory>
#include <netdb.h>
#include <cstring>
#include <string>

#include "Address.h"
#include "IP6Address.h"

namespace lgsomeip {
namespace osabstraction {

class NetworkDevice {
public:
    static NetworkDevice& instance() {
        return device_;
    }

private:
    static NetworkDevice device_;

private:
    NetworkDevice(){};
    NetworkDevice(const NetworkDevice&) = delete;
    NetworkDevice operator=(const NetworkDevice&) = delete;

public:
    bool initialize(std::string address, int iptype);
    bool is_initialized() {
        return initialized_;
    };

    std::string get_device_name() const;
    std::string get_device_addr() const;
    int get_device_id() const;

private:
    bool initialized_{false};
    std::string device_name_{""};
    std::string device_address_{""};
    int device_id_{0};
};

} // namespace osabstraction
} // namespace lgsomeip

#endif // LG_SOMEIP_OSABSTRACTION_NETWORK_DEVICE
