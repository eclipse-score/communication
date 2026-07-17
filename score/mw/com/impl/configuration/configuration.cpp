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

#include "score/mw/com/impl/configuration/configuration_error.h"
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

Result<void> Configuration::MergeServiceEntries(Configuration additional_configuration) noexcept
{
    for (auto& service_type : additional_configuration.service_types_)
    {
        for (const auto& existing_service_type : service_types_)
        {
            if (existing_service_type.first.ToString() == service_type.first.ToString())
            {
                return Unexpected(MakeError(configuration_errc::configuration_merge_duplicate_service_type));
            }
        }

        std::ignore = service_types_.emplace(std::move(service_type.first), std::move(service_type.second));
    }

    for (auto& service_instance : additional_configuration.service_instances_)
    {
        for (const auto& existing_service_instance : service_instances_)
        {
            if (existing_service_instance.first.ToString() == service_instance.first.ToString())
            {
                return Unexpected(MakeError(configuration_errc::configuration_merge_duplicate_service_instance));
            }
        }
        std::ignore = service_instances_.emplace(std::move(service_instance.first), std::move(service_instance.second));
    }
    return {};
}
}  // namespace score::mw::com::impl
