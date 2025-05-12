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
#include "score/mw/com/impl/configuration/test/configuration_store.h"

#include "score/mw/com/impl/configuration/service_instance_deployment.h"
#include "score/mw/com/impl/configuration/service_type_deployment.h"
#include "score/mw/com/impl/enriched_instance_identifier.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/instance_identifier.h"

#include <memory>
#include <utility>

namespace score::mw::com::impl
{

ConfigurationStore::ConfigurationStore(InstanceSpecifier instance_specifier,
                                       const ServiceIdentifierType service_identifier,
                                       const QualityType quality_type,
                                       const LolaServiceId lola_service_id,
                                       const score::cpp::optional<LolaServiceInstanceId> lola_instance_id) noexcept
    : service_identifier_{service_identifier},
      instance_specifier_{std::move(instance_specifier)},
      quality_type_{quality_type},
      lola_instance_id_{lola_instance_id},
      lola_service_type_deployment_{lola_service_id},
      lola_service_instance_deployment_{lola_instance_id_},
      service_type_deployment_{std::make_unique<ServiceTypeDeployment>(lola_service_type_deployment_)},
      service_instance_deployment_{std::make_unique<ServiceInstanceDeployment>(service_identifier_,
                                                                               lola_service_instance_deployment_,
                                                                               quality_type,
                                                                               instance_specifier_)}
{
}

ConfigurationStore::ConfigurationStore(InstanceSpecifier instance_specifier,
                                       const ServiceIdentifierType service_identifier,
                                       const QualityType quality_type,
                                       const LolaServiceTypeDeployment lola_service_type_deployment,
                                       const LolaServiceInstanceDeployment lola_service_instance_deployment) noexcept
    : service_identifier_{service_identifier},
      instance_specifier_{std::move(instance_specifier)},
      quality_type_{quality_type},
      lola_instance_id_{lola_service_instance_deployment.instance_id_.value()},
      lola_service_type_deployment_{lola_service_type_deployment},
      lola_service_instance_deployment_{lola_service_instance_deployment},
      service_type_deployment_{std::make_unique<ServiceTypeDeployment>(lola_service_type_deployment_)},
      service_instance_deployment_{std::make_unique<ServiceInstanceDeployment>(service_identifier_,
                                                                               lola_service_instance_deployment_,
                                                                               quality_type,
                                                                               instance_specifier_)}
{
}

InstanceIdentifier ConfigurationStore::GetInstanceIdentifier() const noexcept
{
    return make_InstanceIdentifier(*service_instance_deployment_, *service_type_deployment_);
}

EnrichedInstanceIdentifier ConfigurationStore::GetEnrichedInstanceIdentifier(
    score::cpp::optional<ServiceInstanceId> instance_id) const noexcept
{
    if (instance_id.has_value())
    {
        return EnrichedInstanceIdentifier(GetInstanceIdentifier(), instance_id.value());
    }
    return EnrichedInstanceIdentifier(GetInstanceIdentifier());
}

HandleType ConfigurationStore::GetHandle(score::cpp::optional<ServiceInstanceId> instance_id) const noexcept
{
    return make_HandleType(GetInstanceIdentifier(), instance_id);
}

}  // namespace score::mw::com::impl
