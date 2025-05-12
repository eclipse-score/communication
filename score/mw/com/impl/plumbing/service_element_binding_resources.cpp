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
#include "score/mw/com/impl/plumbing/service_element_binding_resources.h"

#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"

#include <score/assert.hpp>

#include <variant>

namespace score::mw::com::impl
{

const LolaServiceInstanceDeployment& GetLolaServiceInstanceDeploymentFromServiceInstanceDeployment(
    const ServiceInstanceDeployment& instance_deployment)
{
    const auto* const lola_service_instance_deployment =
        std::get_if<LolaServiceInstanceDeployment>(&instance_deployment.bindingInfo_);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(lola_service_instance_deployment != nullptr,
                           "Service instance deployment should contain a Lola binding!");
    return *lola_service_instance_deployment;
}

const LolaServiceTypeDeployment& GetLolaServiceTypeDeploymentFromServiceTypeDeployment(
    const ServiceTypeDeployment& type_deployment)
{
    const auto* const lola_service_type_deployment =
        std::get_if<LolaServiceTypeDeployment>(&type_deployment.binding_info_);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(lola_service_type_deployment != nullptr,
                           "Service type deployment should contain a Lola binding!");
    return *lola_service_type_deployment;
}

}  // namespace score::mw::com::impl
