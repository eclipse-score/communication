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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_SERVICE_ELEMENT_BINDING_RESOURCES_H
#define SCORE_MW_COM_IMPL_PLUMBING_SERVICE_ELEMENT_BINDING_RESOURCES_H

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"
#include "score/mw/com/impl/configuration/service_instance_deployment.h"
#include "score/mw/com/impl/configuration/service_type_deployment.h"

#include "score/mw/log/logging.h"

#include <exception>
#include <string_view>

namespace score::mw::com::impl
{

const LolaServiceInstanceDeployment& GetLolaServiceInstanceDeploymentFromServiceInstanceDeployment(
    const ServiceInstanceDeployment& instance_deployment);

const LolaServiceTypeDeployment& GetLolaServiceTypeDeploymentFromServiceTypeDeployment(
    const ServiceTypeDeployment& type_deployment);

template <lola::ElementType element_type>
lola::ElementFqId GetElementFqIdFromLolaConfig(const LolaServiceTypeDeployment& lola_service_type_deployment,
                                               const LolaServiceInstanceId lola_service_instance_id,
                                               const std::string_view service_element_name)
{
    const std::string service_element_name_string{service_element_name.data(), service_element_name.size()};

    // coverity[autosar_cpp14_a7_1_8_violation: FALSE] this is a cpp-14 warning. if constexpr is cpp-17 syntax.
    // coverity[autosar_cpp14_m6_4_1_violation: FALSE]: "if constexpr" is a valid statement since C++17.
    if constexpr (element_type == lola::ElementType::EVENT)
    {
        const auto event_id = lola_service_type_deployment.events_.at(service_element_name_string);
        return lola::ElementFqId{
            lola_service_type_deployment.service_id_, event_id, lola_service_instance_id.GetId(), element_type};
    }
    // coverity[autosar_cpp14_a7_1_8_violation: FALSE] this is a cpp-14 warning. if constexpr is cpp-17 syntax.
    // coverity[autosar_cpp14_m6_4_1_violation: FALSE]: "if constexpr" is a valid statement since C++17.
    if constexpr (element_type == lola::ElementType::FIELD)
    {
        const auto field_id = lola_service_type_deployment.fields_.at(service_element_name_string);
        return lola::ElementFqId{
            lola_service_type_deployment.service_id_, field_id, lola_service_instance_id.GetId(), element_type};
    }
    score::mw::log::LogFatal() << "Invalid service element type. Could not get ElementFqId from config. Terminating";
    std::terminate();
}

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_SERVICE_ELEMENT_BINDING_RESOURCES_H
