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

#include "ConfigEventgroup.h"

namespace lgsomeip {

ConfigEventgroup::ConfigEventgroup() {}

std::uint16_t ConfigEventgroup::get_event_group_id() const {
    return event_group_id_;
}

std::uint16_t ConfigEventgroup::get_threshold() const {
    return threshold_;
}

bool ConfigEventgroup::is_multicast() const {
    return is_multicast_;
}

std::vector<std::uint16_t> ConfigEventgroup::get_events() const {
    return events_;
}

std::shared_ptr<lgsomeip::osabstraction::Address> ConfigEventgroup::get_multicast_address() const {
    return multicast_address_;
}

} // namespace lgsomeip
