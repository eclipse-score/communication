/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
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
#ifndef SCORE_MW_COM_IMPL_I_SERVICE_DISCOVERY_CLIENT_H
#define SCORE_MW_COM_IMPL_I_SERVICE_DISCOVERY_CLIENT_H

#include "score/mw/com/impl/enriched_instance_identifier.h"
#include "score/mw/com/impl/find_service_handle.h"
#include "score/mw/com/impl/find_service_handler.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/i_service_discovery.h"
#include "score/mw/com/impl/instance_identifier.h"

#include "score/result/result.h"

namespace score::mw::com::impl
{

class IServiceDiscoveryClient
{
  public:
    virtual ~IServiceDiscoveryClient() noexcept = default;
    IServiceDiscoveryClient() = default;

    [[nodiscard]] virtual ResultBlank OfferService(const InstanceIdentifier instance_identifier) noexcept = 0;
    [[nodiscard]] virtual ResultBlank StopOfferService(
        const InstanceIdentifier instance_identifier,
        const IServiceDiscovery::QualityTypeSelector quality_type_selector) noexcept = 0;
    [[nodiscard]] virtual ResultBlank StartFindService(
        const FindServiceHandle find_service_handle,
        FindServiceHandler<HandleType> handler,
        const EnrichedInstanceIdentifier enriched_instance_identifier) noexcept = 0;
    [[nodiscard]] virtual ResultBlank StopFindService(const FindServiceHandle find_service_handle) noexcept = 0;
    [[nodiscard]] virtual Result<ServiceHandleContainer<HandleType>> FindService(
        const EnrichedInstanceIdentifier enriched_instance_identifier) noexcept = 0;

  protected:
    IServiceDiscoveryClient(const IServiceDiscoveryClient&) = default;
    IServiceDiscoveryClient& operator=(const IServiceDiscoveryClient&) = default;
    IServiceDiscoveryClient(IServiceDiscoveryClient&&) noexcept = default;
    IServiceDiscoveryClient& operator=(IServiceDiscoveryClient&&) noexcept = default;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_I_SERVICE_DISCOVERY_CLIENT_H
