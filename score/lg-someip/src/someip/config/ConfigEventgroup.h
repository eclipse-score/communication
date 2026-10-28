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

#ifndef LG_SOMEIP_CONFIG_EVENTGROUP_H
#define LG_SOMEIP_CONFIG_EVENTGROUP_H

#include <cstdint>

#include <iostream>
#include <unordered_map>
#include <vector>
#include <memory>
#include <socket/Address.h>

namespace lgsomeip {

class ConfigEventgroup {
public:
    ConfigEventgroup();
    std::uint16_t get_event_group_id() const;
    std::uint16_t get_threshold() const;
    bool is_multicast() const;
    std::shared_ptr<lgsomeip::osabstraction::Address> get_multicast_address() const;
    std::vector<std::uint16_t> get_events() const;

private:
    std::vector<std::uint16_t> events_;
    std::uint16_t event_group_id_{0};
    std::uint16_t threshold_{0};
    bool is_multicast_{false};
    std::shared_ptr<lgsomeip::osabstraction::Address> multicast_address_{nullptr};

    friend class ServiceInfo;
};

} // namespace lgsomeip

#endif // LG_SOMEIP_CONFIG_EVENTGROUP_H
