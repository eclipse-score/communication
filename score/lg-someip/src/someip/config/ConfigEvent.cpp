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

#include "ConfigEvent.h"

namespace lgsomeip {

ConfigEvent::ConfigEvent(std::uint16_t event_id, bool is_field, std::uint32_t update_cycle)
    : event_id_(event_id), is_field_(is_field), update_cycle_(update_cycle) {}

std::uint16_t ConfigEvent::get_event_id() const {
    return event_id_;
}

bool ConfigEvent::is_field() const {
    return is_field_;
}

std::uint32_t ConfigEvent::get_update_cycle() const {
    return update_cycle_;
}

} // namespace lgsomeip
