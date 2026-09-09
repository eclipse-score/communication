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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SERVICE_DISCOVERY_CLIENT_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SERVICE_DISCOVERY_CLIENT_H

#include "score/mw/com/impl/find_service_handle.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/i_service_discovery_client.h"
#include "score/mw/com/impl/instance_identifier.h"

#include "score/result/result.h"

namespace score::mw::com::impl::someip
{

/// \brief Service discovery client of the SOME/IP binding.
///
/// \details Offering and stop-offering succeed: the individual service elements announce themselves on the
///          ITransport during their own PrepareOffer()/PrepareStopOffer(), so there is nothing left for the service
///          discovery to do on the provider side.
///
/// \attention The consumer-side operations (Start/StopFindService, FindService) report kUnsupportedBindingType,
///            because the SOME/IP proxy side is not implemented yet. They return an error rather than aborting, so
///            that a configuration which points a consumer at a SOME/IP instance stays diagnosable.
class ServiceDiscoveryClient final : public IServiceDiscoveryClient
{
  public:
    ServiceDiscoveryClient() = default;
    ~ServiceDiscoveryClient() noexcept override = default;

    ServiceDiscoveryClient(const ServiceDiscoveryClient&) = delete;
    ServiceDiscoveryClient(ServiceDiscoveryClient&&) noexcept = delete;
    ServiceDiscoveryClient& operator=(const ServiceDiscoveryClient&) & = delete;
    ServiceDiscoveryClient& operator=(ServiceDiscoveryClient&&) & noexcept = delete;

    Result<void> OfferService(const InstanceIdentifier instance_identifier) override;
    Result<void> StopOfferService(const InstanceIdentifier instance_identifier,
                                  const IServiceDiscovery::QualityTypeSelector quality_type_selector) override;
    Result<void> StartFindService(const FindServiceHandle find_service_handle,
                                  FindServiceHandler<HandleType> handler,
                                  const EnrichedInstanceIdentifier enriched_instance_identifier) override;
    Result<void> StopFindService(const FindServiceHandle find_service_handle) override;
    Result<ServiceHandleContainer<HandleType>> FindService(
        const EnrichedInstanceIdentifier enriched_instance_identifier) override;
};

}  // namespace score::mw::com::impl::someip

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SERVICE_DISCOVERY_CLIENT_H
