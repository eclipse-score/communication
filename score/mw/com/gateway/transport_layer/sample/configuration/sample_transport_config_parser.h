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
#ifndef SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_CONFIGURATION_SAMPLE_TRANSPORT_CONFIG_PARSER_H
#define SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_CONFIGURATION_SAMPLE_TRANSPORT_CONFIG_PARSER_H

#include "score/json/json_parser.h"
#include "score/mw/com/gateway/transport_layer/sample/configuration/hypervisor_socket_configuration.h"

#include <string_view>

namespace score::mw::com::gateway
{

HyperVisorSocketConfiguration ParseSampleTransportConfig(const std::string_view path) noexcept;
HyperVisorSocketConfiguration ParseSampleTransportConfig(score::json::Any json) noexcept;

}  // namespace score::mw::com::gateway

#endif  // SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_CONFIGURATION_SAMPLE_TRANSPORT_CONFIG_PARSER_H
