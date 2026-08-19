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
#ifndef SCORE_MW_COM_SERVICE_DISCOVERY_SERVICE_DISCOVERY_DAEMON_H
#define SCORE_MW_COM_SERVICE_DISCOVERY_SERVICE_DISCOVERY_DAEMON_H

#include "score/mw/com/service_discovery/i_service_discovery_registry.h"
#include "score/mw/com/service_discovery/service_discovery_protocol.h"

#include <score/span.hpp>

#include <cstddef>
#include <vector>

namespace score::mw::com::service_discovery
{

class ServiceDiscoveryDaemon final
{
  public:
    explicit ServiceDiscoveryDaemon(IServiceDiscoveryRegistry& registry) noexcept;

    auto OnDisconnected(ProviderContext provider_context) noexcept -> ServiceKeySet;

    [[nodiscard]] auto Resolve(const ServiceKey& key) const noexcept -> RegistrationSet;

    auto HandlePayload(score::cpp::span<const std::uint8_t> request_payload,
                       score::cpp::span<std::uint8_t> response_payload,
                       ProviderContext provider_context,
                       std::size_t& serialized_response_size) noexcept -> bool;

    [[nodiscard]] auto HandlePayload(const std::vector<std::uint8_t>& request_payload) -> std::vector<std::uint8_t>;

  private:
    IServiceDiscoveryRegistry& registry_;
};

}  // namespace score::mw::com::service_discovery

#endif  // SCORE_MW_COM_SERVICE_DISCOVERY_SERVICE_DISCOVERY_DAEMON_H
