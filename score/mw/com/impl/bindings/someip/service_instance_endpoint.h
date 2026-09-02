/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SERVICE_INSTANCE_ENDPOINT_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SERVICE_INSTANCE_ENDPOINT_H

#include "score/mw/com/impl/configuration/someip_service_id.h"
#include "score/mw/com/impl/configuration/someip_service_instance_id.h"

namespace score::mw::com::impl::someip
{

/// \brief Identifies the SOME/IP service instance that a Proxy or Skeleton binding belongs to.
///
/// The service id originates from the service type deployment and the instance id from the service instance
/// deployment. Combining both is the job of the binding, so that the binding independent plumbing only has to select
/// the binding and does not have to know how a SOME/IP service instance is addressed.
class ServiceInstanceEndpoint
{
  public:
    ServiceInstanceEndpoint() = default;
    ServiceInstanceEndpoint(const SomeIpServiceId service_id,
                            const SomeIpServiceInstanceId::InstanceId instance_id) noexcept
        : service_id_{service_id}, instance_id_{instance_id}
    {
    }

    SomeIpServiceId GetServiceId() const noexcept
    {
        return service_id_;
    }

    SomeIpServiceInstanceId::InstanceId GetInstanceId() const noexcept
    {
        return instance_id_;
    }

  private:
    SomeIpServiceId service_id_{0U};
    SomeIpServiceInstanceId::InstanceId instance_id_{0U};
};

}  // namespace score::mw::com::impl::someip

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SERVICE_INSTANCE_ENDPOINT_H
