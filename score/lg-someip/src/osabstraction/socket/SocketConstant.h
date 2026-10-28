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

#ifndef LG_SOMEIP_OSABSTRACTION_SOCKET_SOCKETCONSTANT
#define LG_SOMEIP_OSABSTRACTION_SOCKET_SOCKETCONSTANT

namespace lgsomeip {
namespace osabstraction {

static constexpr int kInvalidSocket = -1;
static constexpr int kUnixPathMax = 108;
static constexpr int kDefaultBacklog = 10;

} // namespace osabstraction
} // namespace lgsomeip

#endif // LG_SOMEIP_OSABSTRACTION_SOCKET_SOCKETCONSTANT
