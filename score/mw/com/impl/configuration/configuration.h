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
#ifndef SCORE_MW_COM_IMPL_CONFIGURATION_CONFIGURATION_H
#define SCORE_MW_COM_IMPL_CONFIGURATION_CONFIGURATION_H

#include "score/mw/com/impl/configuration/global_configuration.h"
#include "score/mw/com/impl/configuration/service_identifier_type.h"
#include "score/mw/com/impl/configuration/service_instance_deployment.h"
#include "score/mw/com/impl/configuration/service_type_deployment.h"
#include "score/mw/com/impl/configuration/tracing_configuration.h"
#include "score/mw/com/impl/instance_specifier.h"

#include "score/result/result.h"

#include <score/overload.hpp>

#include <cstdint>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace score::mw::com::impl
{

/**
 * @brief Configuration class which stores configuration data parsed from mw com config file.
 *
 * This class will be stored in a static context by the runtime. Therefore, the deployment objects contained in this
 * class will exist for the lifetime of the program. Therefore, any pointers to the objects or values within the objects
 * will be destroyed before the objects are destroyed so lifetime issues can be avoided (unless the pointers are created
 * within a static context).
 *
 * To prevent the memory addresses of the objects changing, we make the maps containing the objects const so that they
 * cannot be updated / reordered after construction. Any additional objects must be added to a fixed size static buffer
 * allocated on the heap so that they also will never change addresses. This of course also requires that the runtime
 * never moves the global Configuration object.
 */
class Configuration final
{
  public:
    using ServiceTypeDeployments = std::unordered_map<ServiceIdentifierType, ServiceTypeDeployment>;
    using ServiceInstanceDeployments = std::unordered_map<InstanceSpecifier, ServiceInstanceDeployment>;
    using BindingInformation = std::variant<LolaServiceTypeDeployment, score::cpp::blank>;

    Configuration(ServiceTypeDeployments service_types,
                  ServiceInstanceDeployments service_instances,
                  GlobalConfiguration global_configuration,
                  TracingConfiguration tracing_configuration) noexcept;
    ~Configuration() noexcept = default;

    /**
     * \brief Class is moveable but not copyable
     */
    Configuration(const Configuration& other) = delete;
    Configuration(Configuration&& other) noexcept = default;
    Configuration& operator=(const Configuration& other) & = delete;
    Configuration& operator=(Configuration&& other) & = delete;

    ServiceTypeDeployment* AddServiceTypeDeployment(ServiceIdentifierType service_identifier_type,
                                                    ServiceTypeDeployment service_type_deployment) noexcept;
    ServiceInstanceDeployment* AddServiceInstanceDeployments(
        InstanceSpecifier instance_specifier,
        ServiceInstanceDeployment service_instance_deployment) noexcept;

    const GlobalConfiguration& GetGlobalConfiguration() const& noexcept
    {
        return global_configuration_;
    }
    const TracingConfiguration& GetTracingConfiguration() const& noexcept
    {
        return tracing_configuration_;
    }

    std::optional<std::reference_wrapper<const ServiceTypeDeployment>> GetServiceTypeDeployment(
        const ServiceIdentifierType& service_identifier_type) const noexcept
    {
        const auto it = service_types_.find(service_identifier_type);
        if (it == service_types_.end())
        {
            return std::nullopt;
        }
        return std::cref(it->second);
    }

    std::optional<std::reference_wrapper<const ServiceInstanceDeployment>> GetServiceInstanceDeployment(
        const InstanceSpecifier& specifier) const noexcept
    {
        const auto it = service_instances_.find(specifier);
        if (it == service_instances_.end())
        {
            return std::nullopt;
        }
        return std::cref(it->second);
    }

    size_t GetNumberOfServiceTypes() const noexcept
    {
        return service_types_.size();
    }

    bool IsServiceTypesEmpty() const noexcept
    {
        return service_types_.empty();
    }

    size_t GetNumberOfServiceInstances() const noexcept
    {
        return service_instances_.size();
    }

    bool IsServiceInstancesEmpty() const noexcept
    {
        return service_instances_.empty();
    }

    /// \brief Public interface to trigger a validation of this configuration.
    score::Result<void> Validate() const noexcept;

    /// \brief Determine if any service with a LoLa binding is defined in this configuration
    score::Result<bool> HasLolaServiceDeployment() const noexcept;

    /// \brief Returns the list of names (ToString()) of all configured ServiceIdentifierTypes
    std::set<std::string_view> GetServiceTypeNames() const noexcept;

    /// \brief Returns a set of element names, used within the given service_type. The names in the set are string_views
    ///        pointing to strings owned by members of this Configuration. So the life-time of those string-views is
    ///        bound to the life-time of this Configuration.
    /// \param service_type service type from which to get element names
    /// \param element_type element type of which to get names
    /// \return a set of string_views denoting the service element names.
    std::set<std::string_view> GetElementNamesOfServiceType(const std::string_view service_type,
                                                            ServiceElementType element_type) const noexcept;

    /// \brief Returns a set of UIDs of all allowed users of all service instances defined in this configuration for the
    /// given
    ///         ASIL level.
    /// \param asil_level ASIL level of interest for which to get allowed users
    /// \return a set of uid_t of all allowed providers and consumers
    std::set<uid_t> GetAggregatedAllowedUsers(const QualityType asil_level) const noexcept;

    /// \brief Returns the configured instances of the given service type
    /// \param service_type identification of the service type (which is an AUTOSAR short-name-path representation)
    /// \return set of string_views reflecting an InstanceSpecifier. Those string_views reference into strings held by
    ///         this configuration. Their lifetime is the same as the LoLa runtime!
    std::set<std::string_view> GetInstancesOfServiceType(std::string_view service_type) const noexcept;

  private:
    /// \brief Validate if service ASIL levels match the application's assigned ASIL level.
    score::Result<void> CrossCheckAsilLevels() const noexcept;
    /// \brief Validate if service type definitions and service instance definitions fit together.
    score::Result<void> CrossCheckServiceInstancesToTypes() const noexcept;

    /// \brief Helper func aggregates allowed_user_ids of the given quality type into aggregated_allowed_users. If
    ///        allowed_user_ids is empty (no access restriction!), then aggregated_allowed_users is cleared!
    /// \param aggregated_allowed_users aggregated user ids (for access control) for the given asil_level
    /// \param allowed_user_ids user ids to be aggregated/added into aggregated_allowed_users
    /// \param asil_level asil level
    /// \return true, in case aggregated_allowed_users has been cleared
    static bool AggregateAllowedUsers(std::set<uid_t>& aggregated_allowed_users,
                                      const std::unordered_map<QualityType, std::vector<uid_t>>& allowed_user_ids,
                                      const QualityType asil_level) noexcept;

    /**
     * @brief map containing all the configured ports/InstanceSpecifiers for an executable.
     *
     * Key is the string representation of the InstanceSpecifier aka port name.
     * Value is the ServiceIdentifierType, the port is typed with.
     */
    ServiceTypeDeployments service_types_;
    ServiceInstanceDeployments service_instances_;
    GlobalConfiguration global_configuration_;
    TracingConfiguration tracing_configuration_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_CONFIGURATION_CONFIGURATION_H
