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
 *******************************************************************************/
#ifndef SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_TRANSPORT_FACTORY_H
#define SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_TRANSPORT_FACTORY_H

#include "score/mw/com/gateway/gateway_application/gateway_core.h"
#include "score/mw/com/gateway/transport_layer/transport.h"

#include <memory>
#include <string>

namespace score::mw::com::gateway
{

class TransportFactory
{
  public:
    static std::unique_ptr<Transport> Create(GatewayCore& gateway_core,
                                             const std::string& transport_layer_id,
                                             const std::string& transport_config_path);
};

}  // namespace score::mw::com::gateway

#endif  // SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_TRANSPORT_FACTORY_H
