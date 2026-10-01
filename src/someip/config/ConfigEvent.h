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

#ifndef LG_SOMEIP_CONFIG_EVENT_H
#define LG_SOMEIP_CONFIG_EVENT_H

#include <cstdint>

#include <iostream>
#include <vector>

namespace lgsomeip {

class ConfigEvent {
public:
    ConfigEvent(std::uint16_t event_id, bool is_field, std::uint32_t update_cycle);
    std::uint16_t get_event_id() const;
    bool is_field() const;
    std::uint32_t get_update_cycle() const;

private:
    std::uint16_t event_id_{0};
    bool is_field_{false};
    std::uint32_t update_cycle_{0};
};

} // namespace lgsomeip

#endif // LG_SOMEIP_CONFIG_EVENT_H
