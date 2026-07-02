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
#include "score/mw/com/gateway/transport_layer/transport_factory.h"

#include "score/mw/com/gateway/transport_layer/sample/bidirectional_transport.h"
#include "score/mw/com/gateway/transport_layer/sample/configuration/sample_transport_config_parser.h"
#include "score/mw/com/gateway/transport_layer/sample/sample_hypervisor_transport.h"

#include <cstdlib>
#include <string>

namespace score::mw::com::gateway
{

std::unique_ptr<Transport> TransportFactory::Create(GatewayCore& gateway_core,
                                                    const std::string& transport_layer_id,
                                                    const std::string& transport_config_path)
{
    if (transport_layer_id == "sample_hypervisor")
    {
        // Parse sample-hypervisor-specific transport configuration
        const HyperVisorSocketConfiguration socket_cfg = ParseSampleTransportConfig(transport_config_path);
        (void)socket_cfg;

        auto bidirectional = std::make_unique<BidirectionalTransport>(socket_cfg);
        return std::make_unique<SampleHyperVisorTransport>(gateway_core, std::move(bidirectional));
    }

    // Unknown transport layer id — no implementation registered.
    std::terminate();
}

}  // namespace score::mw::com::gateway
