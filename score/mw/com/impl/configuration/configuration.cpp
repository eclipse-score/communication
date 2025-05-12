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
#include "score/mw/com/impl/configuration/configuration.h"

#include "score/mw/log/logging.h"

#include <exception>
#include <utility>

namespace score::mw::com::impl
{

Configuration::Configuration(ServiceTypeDeployments service_types,
                             ServiceInstanceDeployments service_instances,
                             GlobalConfiguration global_configuration,
                             TracingConfiguration tracing_configuration) noexcept
    : service_types_{std::move(service_types)},
      service_instances_{std::move(service_instances)},
      global_configuration_{std::move(global_configuration)},
      tracing_configuration_{std::move(tracing_configuration)}
{
}

ServiceTypeDeployment* Configuration::AddServiceTypeDeployment(ServiceIdentifierType service_identifier_type,
                                                               ServiceTypeDeployment service_type_deployment) noexcept
{
    const auto emplace_result =
        service_types_.emplace(std::move(service_identifier_type), std::move(service_type_deployment));
    if (!emplace_result.second)
    {
        ::score::mw::log::LogFatal("lola")
            << "Could not insert service type deployment into Configuration map. Terminating";
        std::terminate();
    }
    return &emplace_result.first->second;
}

ServiceInstanceDeployment* Configuration::AddServiceInstanceDeployments(
    InstanceSpecifier instance_specifier,
    ServiceInstanceDeployment service_instance_deployment) noexcept
{
    const auto emplace_result =
        service_instances_.emplace(std::move(instance_specifier), std::move(service_instance_deployment));
    if (!emplace_result.second)
    {
        ::score::mw::log::LogFatal("lola")
            << "Could not insert service instance deployment into Configuration map. Terminating";
        std::terminate();
    }
    return &emplace_result.first->second;
}

}  // namespace score::mw::com::impl
