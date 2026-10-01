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

#ifndef LG_SOMEIP_PACKET_ROUTER_CONSTANT_H
#define LG_SOMEIP_PACKET_ROUTER_CONSTANT_H

#include <cstdint>

#include <endpoint/EndpointConstant.h>

namespace lgsomeip {

#define SOMEIP_DAEMON_ID 0
#define SOMEIP_APPLICATION_NAME "NAME"

constexpr std::uint16_t kMagicCookiePeriodMs = 10000;

} // namespace lgsomeip

#endif // LG_SOMEIP_PACKET_ROUTER_CONSTANT_H
