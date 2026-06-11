/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/
#ifndef SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_CONFIGURATION_HYPERVISOR_SOCKET_CONFIGURATION_H
#define SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_CONFIGURATION_HYPERVISOR_SOCKET_CONFIGURATION_H

#include "score/network/ipv4_address.h"

#include <cstdint>

namespace score::mw::com::gateway
{

class HyperVisorSocketConfiguration
{
  public:
    HyperVisorSocketConfiguration() = default;

    score::os::Ipv4Address remote_ip_{};
    std::uint16_t local_port_{0};
    std::uint16_t remote_port_{0};
    std::uint32_t request_timeout_ms_{5000};
};

}  // namespace score::mw::com::gateway

#endif  // SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_CONFIGURATION_HYPERVISOR_SOCKET_CONFIGURATION_H
