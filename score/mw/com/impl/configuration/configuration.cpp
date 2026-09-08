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
    : service_types_{std::make_shared<std::list<std::shared_ptr<ServiceTypeDeployments>>>()},
      service_instances_{std::make_shared<std::list<std::shared_ptr<ServiceInstanceDeployments>>>()},
      global_configuration_{std::move(global_configuration)},
      tracing_configuration_{std::move(tracing_configuration)}
{
    service_types_->push_front(std::make_shared<ServiceTypeDeployments>(std::move(service_types)));
    service_instances_->push_front(std::make_shared<ServiceInstanceDeployments>(std::move(service_instances)));
}

Configuration::Configuration(Configuration&& other) noexcept
    // Use lock_guard to guard against a concurrent MergeServiceEntries() call on `other` racing with this move.
    : Configuration(other, std::lock_guard<std::mutex>(other.merge_mutex_))
{
}

Configuration::Configuration(Configuration& other, const std::lock_guard<std::mutex>&) noexcept
    : service_types_{std::move(other.service_types_)},
      service_instances_{std::move(other.service_instances_)},
      global_configuration_{std::move(other.global_configuration_)},
      tracing_configuration_{std::move(other.tracing_configuration_)}
// merge_mutex_ itself is not movable and is left default-constructed in the new object.
{
}

ServiceTypeDeployment* Configuration::AddServiceTypeDeployment(ServiceIdentifierType service_identifier_type,
                                                               ServiceTypeDeployment service_type_deployment) noexcept
{
    // If this service type has not yet been added (not in any map of the list), this service type will be added to the
    // latest map of service types.

    const auto current_list = GetListOfServiceTypeMaps();

    const bool type_found = CheckServiceTypeExists(service_identifier_type, current_list);

    if (!type_found)
    {
        const auto last_map_entry = current_list->back();
        const auto emplace_result =
            last_map_entry->emplace(std::move(service_identifier_type), std::move(service_type_deployment));

        if (emplace_result.second)
        {
            return &emplace_result.first->second;
        }
    }

    ::score::mw::log::LogFatal("lola")
        << "Could not insert service type deployment into Configuration map. Terminating";
    std::terminate();
}

ServiceInstanceDeployment* Configuration::AddServiceInstanceDeployments(
    InstanceSpecifier instance_specifier,
    ServiceInstanceDeployment service_instance_deployment) noexcept
{
    // If this service instance has not yet been added (not in any map of the list), this service instance will be added
    // to the latest map of service instances.

    const auto current_list = GetListOfServiceInstanceMaps();

    const bool instance_found = CheckServiceInstanceExists(instance_specifier, current_list);

    if (!instance_found)
    {
        const auto last_map_entry = current_list->back();
        const auto emplace_result =
            last_map_entry->emplace(std::move(instance_specifier), std::move(service_instance_deployment));

        if (emplace_result.second)
        {
            return &emplace_result.first->second;
        }
    }

    ::score::mw::log::LogFatal("lola")
        << "Could not insert service instance deployment into Configuration map. Terminating";
    std::terminate();
}

Result<void> Configuration::MergeServiceEntries(const Configuration& additional_configuration) noexcept
{
    std::lock_guard<std::mutex> lock(merge_mutex_);

    // Construct a new map of service type deployments and add all service_type_deployments of
    // additional_configuration to it. Add this map as a front entry in the list of those entries, so that it will
    // be returned as the latest version.
    auto new_type_map = std::make_shared<ServiceTypeDeployments>();
    bool new_type_element_inserted = false;

    const auto current_type_list = std::atomic_load_explicit(&service_types_, std::memory_order_acquire);
    if (current_type_list != nullptr)
    {
        for (const auto& service_type : additional_configuration.GetServiceTypes())
        {
            const bool type_found = CheckServiceTypeExists(service_type.first, current_type_list);

            if (!type_found)
            {
                new_type_map->emplace(service_type.first, service_type.second);
                new_type_element_inserted = true;
            }
            else
            {
                return Unexpected(MakeError(configuration_errc::configuration_merge_duplicate_service_type));
            }
        }
    }
    else
    {
        return Unexpected(MakeError(configuration_errc::configuration_merged_invalid_configuration_state));
    }

    if (new_type_element_inserted)
    {
        auto updated_list = std::make_shared<std::list<std::shared_ptr<ServiceTypeDeployments>>>(*service_types_);
        updated_list->push_front(new_type_map);

        std::atomic_store_explicit(&service_types_, updated_list, std::memory_order_release);
    }

    // Update service instances

    auto new_instance_map = std::make_shared<ServiceInstanceDeployments>();
    bool new_instance_element_inserted = false;

    const auto current_instance_list = std::atomic_load_explicit(&service_instances_, std::memory_order_acquire);
    if (current_instance_list != nullptr)
    {
        for (const auto& service_instance : additional_configuration.GetServiceInstances())
        {
            const bool instance_found = CheckServiceInstanceExists(service_instance.first, current_instance_list);

            if (!instance_found)
            {
                new_instance_map->emplace(service_instance.first, service_instance.second);
                new_instance_element_inserted = true;
            }
            else
            {
                return Unexpected(MakeError(configuration_errc::configuration_merge_duplicate_service_instance));
            }
        }
    }
    else
    {
        return Unexpected(MakeError(configuration_errc::configuration_merged_invalid_configuration_state));
    }

    if (new_instance_element_inserted)
    {
        auto updated_list =
            std::make_shared<std::list<std::shared_ptr<ServiceInstanceDeployments>>>(*service_instances_);
        updated_list->push_front(new_instance_map);

        std::atomic_store_explicit(&service_instances_, updated_list, std::memory_order_release);
    }

    return {};
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
        const auto service_types = GetServiceTypes();
        const auto foundServiceType = service_types.find(service_instance.second.service_);
        if (foundServiceType == service_types.cend())
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

    for (const auto& service_type : GetServiceTypes())
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
    ForEachServiceType([&configured_service_types](const auto& map_entry) {
        const auto service_type_string_view = map_entry.first.ToString();
        score::cpp::ignore = configured_service_types.insert(service_type_string_view);
    });
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

    ForEachServiceType([&service_type, &service_type_deployment_visitor](const auto& service_type_deployment) {
        const ServiceIdentifierTypeView current_service_type_view{service_type_deployment.first};
        if (current_service_type_view.getInternalTypeName() == service_type)
        {
            std::visit(service_type_deployment_visitor, service_type_deployment.second.binding_info_);
        }
    });
    return result;
}

std::set<uid_t> Configuration::GetAggregatedAllowedUsers(const QualityType asil_level) const noexcept
{
    std::set<uid_t> aggregated_allowed_users{};
    for (const auto& instanceDeplElement : GetServiceInstances())
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
    ForEachServiceInstance([&result, service_type](const auto& service_instance_element) {
        if (service_instance_element.second.service_.ToString() == service_type)
        {
            score::cpp::ignore = result.insert(service_instance_element.first.ToString());
        }
    });
    return result;
}

std::optional<std::reference_wrapper<const ServiceTypeDeployment>> Configuration::GetServiceTypeDeployment(
    const ServiceIdentifierType& service_identifier_type) const noexcept
{
    const auto current_list = std::atomic_load_explicit(&service_types_, std::memory_order_acquire);
    if ((current_list == nullptr) || current_list->empty())
    {
        return std::nullopt;
    }
    for (const auto& element : *current_list.get())
    {
        const auto it = element->find(service_identifier_type);
        if (it != element->end())
        {
            return std::cref(it->second);
        }
    }
    return std::nullopt;
}

std::optional<std::reference_wrapper<const ServiceInstanceDeployment>> Configuration::GetServiceInstanceDeployment(
    const InstanceSpecifier& specifier) const noexcept
{
    const auto current_list = std::atomic_load_explicit(&service_instances_, std::memory_order_acquire);
    if ((current_list == nullptr) || current_list->empty())
    {
        return std::nullopt;
    }
    for (const auto& element : *current_list.get())
    {
        const auto it = element->find(specifier);
        if (it != element->end())
        {
            return std::cref(it->second);
        }
    }
    return std::nullopt;
}

size_t Configuration::GetNumberOfServiceTypes() const noexcept
{
    const auto current_list = GetListOfServiceTypeMaps();
    size_t number_of_types = 0;
    for (const auto& element : *current_list.get())
    {
        number_of_types += element->size();
    }
    return number_of_types;
}

size_t Configuration::GetNumberOfServiceInstances() const noexcept
{
    const auto current_list = GetListOfServiceInstanceMaps();

    size_t number_of_instances = 0;
    for (const auto& element : *current_list.get())
    {
        number_of_instances += element->size();
    }
    return number_of_instances;
}

bool Configuration::CheckServiceTypeExists(
    const ServiceIdentifierType& service_identifier,
    const std::shared_ptr<std::list<std::shared_ptr<std::unordered_map<ServiceIdentifierType, ServiceTypeDeployment>>>>&
        current_list)
{
    return std::any_of(current_list->begin(), current_list->end(), [&service_identifier](const auto& element) {
        return element->find(service_identifier) != element->end();
    });
}

bool Configuration::CheckServiceInstanceExists(
    const InstanceSpecifier& instance_identifier,
    const std::shared_ptr<std::list<std::shared_ptr<std::unordered_map<InstanceSpecifier, ServiceInstanceDeployment>>>>&
        current_list)
{
    return std::any_of(current_list->begin(), current_list->end(), [&instance_identifier](const auto& element) {
        return element->find(instance_identifier) != element->end();
    });
}

std::shared_ptr<std::list<std::shared_ptr<Configuration::ServiceTypeDeployments>>>
Configuration::GetListOfServiceTypeMaps() const
{
    const auto current_list = std::atomic_load_explicit(&service_types_, std::memory_order_acquire);
    if (current_list != nullptr && !current_list->empty())
    {
        return current_list;
    }
    ::score::mw::log::LogFatal("lola")
        << "Could not access Configuration's list of service type deployments. Terminating";
    std::terminate();
}

std::shared_ptr<std::list<std::shared_ptr<Configuration::ServiceInstanceDeployments>>>
Configuration::GetListOfServiceInstanceMaps() const
{
    const auto current_list = std::atomic_load_explicit(&service_instances_, std::memory_order_acquire);
    if (current_list != nullptr && !current_list->empty())
    {
        return current_list;
    }
    ::score::mw::log::LogFatal("lola")
        << "Could not access Configuration's list of service instance deployments. Terminating";
    std::terminate();
}

Configuration::ServiceTypeDeployments Configuration::GetServiceTypes() const noexcept
{
    ServiceTypeDeployments result{};

    const auto current_list = GetListOfServiceTypeMaps();

    for (const auto& element : *current_list.get())
    {
        for (const auto& entry : *element.get())
        {
            result.emplace(entry.first, entry.second);
        }
    }
    return result;
}

Configuration::ServiceInstanceDeployments Configuration::GetServiceInstances() const noexcept
{
    ServiceInstanceDeployments result{};

    const auto current_list = GetListOfServiceInstanceMaps();

    for (const auto& element : *current_list.get())
    {
        for (const auto& entry : *element.get())
        {
            result.emplace(entry.first, entry.second);
        }
    }
    return result;
}

}  // namespace score::mw::com::impl
