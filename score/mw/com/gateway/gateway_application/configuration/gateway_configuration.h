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
#ifndef SCORE_MW_COM_GATEWAY_GATEWAY_APPLICATION_CONFIGURATION_GATEWAY_CONFIGURATION_H
#define SCORE_MW_COM_GATEWAY_GATEWAY_APPLICATION_CONFIGURATION_GATEWAY_CONFIGURATION_H

#include <string>
#include <utility>
#include <vector>

namespace score::mw::com::gateway
{

class GatewayConfiguration
{
  public:
    GatewayConfiguration(std::vector<std::string> forwarded_services,
                         std::vector<std::string> received_services,
                         std::string transport_layer_id,
                         std::string transport_config_path)
        : forwarded_services_{std::move(forwarded_services)},
          received_services_{std::move(received_services)},
          transport_layer_id_{std::move(transport_layer_id)},
          transport_config_path_{std::move(transport_config_path)}
    {
    }
    ~GatewayConfiguration() noexcept = default;

    std::vector<std::string> GetForwardedServices() const
    {
        return forwarded_services_;
    }

    std::vector<std::string> GetReceivedServices() const
    {
        return received_services_;
    }

    std::string GetTransportLayerId() const
    {
        return transport_layer_id_;
    }

    std::string GetTransportConfigPath() const
    {
        return transport_config_path_;
    }

  private:
    std::vector<std::string> forwarded_services_;
    std::vector<std::string> received_services_;
    std::string transport_layer_id_;
    std::string transport_config_path_;
};

}  // namespace score::mw::com::gateway

#endif  // SCORE_MW_COM_GATEWAY_GATEWAY_APPLICATION_CONFIGURATION_GATEWAY_CONFIGURATION_H
