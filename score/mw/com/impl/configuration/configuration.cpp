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
    for (const auto& service_instance : service_instances_)
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
    for (const auto& service_instance : service_instances_)
    {
        const auto foundServiceType = service_types_.find(service_instance.second.service_);
        if (foundServiceType == service_types_.cend())
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
score::Result<bool> Configuration::HasLolaServiceDeployment() const noexcept
{
    auto deployment_info_visitor = score::cpp::overload(
        [](const LolaServiceTypeDeployment&) {
            return true;
        },
        [](const score::cpp::blank&) noexcept {
            return false;
        });

    for (const auto& service_type : service_types_)
    {
        if (std::visit(deployment_info_visitor, service_type.second.binding_info_))
        {
            return true;
        }
    }
    return false;
}

std::set<std::string_view> Configuration::GetServiceTypeNames() const noexcept
{
    std::set<std::string_view> configured_service_types{};
    for (const auto& map_entry : service_types_)
    {
        const auto service_type_string_view = map_entry.first.ToString();
        score::cpp::ignore = configured_service_types.insert(service_type_string_view);
    }
    return configured_service_types;
}

std::set<std::string_view> Configuration::GetElementNamesOfServiceType(const std::string_view service_type,
                                                                       ServiceElementType element_type) const noexcept
{
    std::set<std::string_view> result{};
    auto service_type_deployment_visitor = score::cpp::overload(
        [&result, element_type](const LolaServiceTypeDeployment& lola_service_deployment) {
            if (element_type == ServiceElementType::EVENT)
            {
                // LCOV_EXCL_BR_START (Tool incorrectly marks the range-for loop as "Decision couldn't be analyzed"
                // despite all lines within the loop being covered. We also have a test for the case where
                // lola_service_deployment.events_ is empty. Suppression can be removed when the tooling bug is fixed.)
                for (const auto& event : lola_service_deployment.events_)
                // LCOV_EXCL_BR_STOP
                {
                    score::cpp::ignore = result.insert(event.first);
                }
            }
            // LCOV_EXCL_BR_START (Defensive programming: GetElementNamesOfServiceType is always called with either
            // ServiceElementType::EVENT or ServiceElementType::FIELD. Entering the false branch of this check is
            // therefore unreachable.
            else if (element_type == ServiceElementType::FIELD)
            // LCOV_EXCL_BR_STOP
            {
                // LCOV_EXCL_BR_START (Tool incorrectly marks the range-for loop as "Decision couldn't be analyzed"
                // despite all lines within the loop being covered. We also have a test for the case where
                // lola_service_deployment.fields_ is empty. Suppression can be removed when the tooling bug is fixed.)
                for (const auto& field : lola_service_deployment.fields_)
                // LCOV_EXCL_BR_STOP
                {
                    score::cpp::ignore = result.insert(field.first);
                }
            }
            // LCOV_EXCL_START (Defensive programming: See comment directly above. This branch is only included to
            // protect us from future programming mistakes)
            else
            {
                score::mw::log::LogFatal("lola")
                    << "GetElementNamesOfServiceType called with unsupported ServiceElementType: " << element_type;
                std::terminate();
            }
            // LCOV_EXCL_STOP
        },
        // LCOV_EXCL_START (Unreachable Code: GetElementNamesOfServiceType can only be called on an existing service
        // type. I.e. The ServiceTypeDeployment must be LolaServiceTypeDeployment and can never be score::cpp::blank.
        // This code is there because std::visitor must handle all std::variant types.
        [](const score::cpp::blank&) noexcept {
            return;
        }
        // LCOV_EXCL_STOP
    );

    // LCOV_EXCL_BR_START (Tool incorrectly marks the range-for loop as "Decision couldn't be analyzed". The false
    // case (empty GetServiceTypes()) is structurally unreachable: GetElementNamesOfServiceType is only called from
    // ParseEvents/ParseFields which are reached only after the service type was found in GetServiceTypes().
    // Suppression can be removed when the tooling bug is fixed.)
    for (const auto& service_type_deployment : service_types_)
    // LCOV_EXCL_BR_STOP
    {
        const ServiceIdentifierTypeView current_service_type_view{service_type_deployment.first};
        if (current_service_type_view.getInternalTypeName() == service_type)
        {
            std::visit(service_type_deployment_visitor, service_type_deployment.second.binding_info_);
        }
    }
    return result;
}

std::set<uid_t> Configuration::GetAggregatedAllowedUsers(const QualityType asil_level) const noexcept
{
    std::set<uid_t> aggregated_allowed_users{};
    for (const auto& instanceDeplElement : service_instances_)
    {
        const auto* const instance_deployment =
            std::get_if<LolaServiceInstanceDeployment>(&instanceDeplElement.second.bindingInfo_);
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
            instance_deployment != nullptr,
            "Instance deployment must contain Lola binding in order to create a lola runtime!");
        if (AggregateAllowedUsers(aggregated_allowed_users, instance_deployment->allowed_consumer_, asil_level))
        {
            break;
        }
        if (AggregateAllowedUsers(aggregated_allowed_users, instance_deployment->allowed_provider_, asil_level))
        {
            break;
        }
    }
    return aggregated_allowed_users;
}

bool Configuration::AggregateAllowedUsers(std::set<uid_t>& aggregated_allowed_users,
                                          const std::unordered_map<QualityType, std::vector<uid_t>>& allowed_user_ids,
                                          const QualityType asil_level) noexcept
{
    const auto user_ids = allowed_user_ids.find(asil_level);
    if (user_ids != allowed_user_ids.cend())
    {
        if (user_ids->second.empty())
        {
            aggregated_allowed_users.clear();
            return true;
        }
        // LCOV_EXCL_BR_START (Defensive programming: This for-loop's false/never-enters branch is unreachable.
        // The empty-vector case is already handled above by the user_ids->second.empty() check which returns early.)
        for (const auto& user_identifier : user_ids->second)
        // LCOV_EXCL_BR_STOP
        {
            // result of insert can be ignored, because at the end the element is in
            // the set and we have no expectation, if it already was in the set before or not.
            score::cpp::ignore = aggregated_allowed_users.insert(user_identifier);
        }
    }
    return false;
}

std::set<std::string_view> Configuration::GetInstancesOfServiceType(std::string_view service_type) const noexcept
{
    std::set<std::string_view> result{};
    // LCOV_EXCL_BR_START (Tool incorrectly marks the range-for loop as "Decision couldn't be analyzed" despite all
    // lines within the loop being covered. We also have a test for the case where GetServiceInstances() is empty.
    // Suppression can be removed when the tooling bug is fixed.)
    for (const auto& service_instance_element : service_instances_)
    // LCOV_EXCL_BR_STOP
    {
        if (service_instance_element.second.service_.ToString() == service_type)
        {
            const auto element_string_view = service_instance_element.first.ToString();
            score::cpp::ignore = result.insert(element_string_view);
        }
    }
    return result;
}

}  // namespace score::mw::com::impl
