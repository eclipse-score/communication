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
#ifndef SCORE_MW_COM_IMPL_CONFIGURATION_TEST_CONFIGURATION_STORE_H
#define SCORE_MW_COM_IMPL_CONFIGURATION_TEST_CONFIGURATION_STORE_H

#include "score/mw/com/impl/configuration/lola_service_id.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/configuration/service_identifier_type.h"
#include "score/mw/com/impl/configuration/service_instance_deployment.h"
#include "score/mw/com/impl/configuration/service_instance_id.h"
#include "score/mw/com/impl/configuration/service_type_deployment.h"
#include "score/mw/com/impl/enriched_instance_identifier.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/instance_specifier.h"

#include <score/callback.hpp>
#include <score/optional.hpp>

#include <memory>

namespace score::mw::com::impl
{

class ConfigurationStore final
{
  public:
    ConfigurationStore(InstanceSpecifier instance_specifier,
                       const ServiceIdentifierType service_identifier,
                       const QualityType quality_type,
                       const LolaServiceId lola_service_id,
                       const score::cpp::optional<LolaServiceInstanceId> lola_instance_id) noexcept;

    ConfigurationStore(InstanceSpecifier instance_specifier,
                       const ServiceIdentifierType service_identifier,
                       const QualityType quality_type,
                       const LolaServiceTypeDeployment lola_service_type_deployment,
                       const LolaServiceInstanceDeployment lola_service_instance_deployment) noexcept;

    InstanceIdentifier GetInstanceIdentifier() const noexcept;
    EnrichedInstanceIdentifier GetEnrichedInstanceIdentifier(
        score::cpp::optional<ServiceInstanceId> instance_id = {}) const noexcept;
    HandleType GetHandle(score::cpp::optional<ServiceInstanceId> instance_id = {}) const noexcept;

    ServiceIdentifierType service_identifier_;
    InstanceSpecifier instance_specifier_;
    QualityType quality_type_;
    score::cpp::optional<LolaServiceInstanceId> lola_instance_id_;

    LolaServiceTypeDeployment lola_service_type_deployment_;
    LolaServiceInstanceDeployment lola_service_instance_deployment_;

    /// We store the ServiceTypeDeployment and ServiceInstanceDeployment on the heap to ensure that their addresses
    /// never change (since we store pointers to them in InstanceIdentifiers).
    std::unique_ptr<ServiceTypeDeployment> service_type_deployment_;
    std::unique_ptr<ServiceInstanceDeployment> service_instance_deployment_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_CONFIGURATION_TEST_CONFIGURATION_STORE_H
