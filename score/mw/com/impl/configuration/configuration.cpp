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

score::Result<void> Configuration::Validate() const noexcept
{
    if (const auto result = CrossCheckAsilLevels(); !result.has_value())
    {
        return result;
    }
    if (const auto result = CrossCheckServiceInstancesToTypes(); !result.has_value())
    {
        return result;
    }
    return {};
}

score::Result<void> Configuration::CrossCheckAsilLevels() const noexcept
{
    for (const auto& service_instance : GetServiceInstances())
    {
        if ((service_instance.second.asilLevel_ == QualityType::kASIL_B) &&
            (GetGlobalConfiguration().GetProcessAsilLevel() != QualityType::kASIL_B))
        {
            return MakeUnexpected(configuration_errc::configuration_invalid_asil_configuration,
                                  "Service instance has a higher ASIL than the process. This is invalid, terminating");
        }
    }
    return {};
}

score::Result<void> Configuration::CrossCheckServiceInstancesToTypes() const noexcept
{
    for (const auto& service_instance : GetServiceInstances())
    {
        const auto foundServiceType = GetServiceTypes().find(service_instance.second.service_);
        if (foundServiceType == GetServiceTypes().cend())
        {
            return MakeUnexpected(
                configuration_errc::configuration_invalid_type_reference_from_instance,
                "Service instance refers to a service type, which is not configured. This is invalid, terminating");
        }
        // LCOV_EXCL_BR_START: Defensive programming: Parse() currently terminates if the ServiceInstanceDeployment
        // contains anything other than a Lola binding. Therefore, it's impossible to reach this point without
        // a LolaServiceInstanceDeployment.
        if (!std::holds_alternative<LolaServiceInstanceDeployment>(service_instance.second.bindingInfo_))
        {
            return MakeUnexpected(
                configuration_errc::configuration_unsupported_instance_binding,
                "Service instance refers to an not yet supported binding. This is invalid, terminating");
        }
        if (!std::holds_alternative<LolaServiceTypeDeployment>(foundServiceType->second.binding_info_))
        {
            return MakeUnexpected(configuration_errc::configuration_unsupported_type_binding,
                                  "Service type refers to an not yet supported binding. This is invalid, terminating");
        }
        const auto& serviceInstanceDeployment =
            std::get<LolaServiceInstanceDeployment>(service_instance.second.bindingInfo_);
        for (const auto& eventInstanceElement : serviceInstanceDeployment.events_)
        {
            const auto& serviceTypeDeployment =
                std::get<LolaServiceTypeDeployment>(foundServiceType->second.binding_info_);
            const auto search = serviceTypeDeployment.events_.find(eventInstanceElement.first);
            if (search == serviceTypeDeployment.events_.cend())
            {
                return MakeUnexpected(configuration_errc::configuration_invalid_event_reference_from_instance,
                                      "Service instance refers to an event, which doesn't exist in the referenced "
                                      "service type. This is invalid, terminating");
            }
        }
        for (const auto& fieldInstanceElement : serviceInstanceDeployment.fields_)
        {
            const auto& serviceTypeDeployment =
                std::get<LolaServiceTypeDeployment>(foundServiceType->second.binding_info_);
            const auto search = serviceTypeDeployment.fields_.find(fieldInstanceElement.first);
            if (search == serviceTypeDeployment.fields_.cend())
            {
                return MakeUnexpected(configuration_errc::configuration_invalid_field_reference_from_instance,
                                      "Service instance refers to a field, which doesn't exist in the referenced "
                                      "service type. This is invalid, terminating");
            }
        }
    }
    return {};
}
}  // namespace score::mw::com::impl
