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
#include <list>
#include <mutex>
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

    // Suppress "AUTOSAR C++14 A11-3-1", The rule declares: "Friend declarations shall not be used".
    // Test only use to check internal state of configuration after (merge) operations.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class ConfigurationFixture;

    Configuration(ServiceTypeDeployments service_types,
                  ServiceInstanceDeployments service_instances,
                  GlobalConfiguration global_configuration,
                  TracingConfiguration tracing_configuration) noexcept;
    ~Configuration() noexcept = default;

    /**
     * \brief Class is moveable but not copyable
     */
    Configuration(const Configuration& other) = delete;
    Configuration(Configuration&& other) noexcept;
    Configuration& operator=(const Configuration& other) & = delete;
    Configuration& operator=(Configuration&& other) & = delete;

  private:
    /// \brief Delegate target used by the move constructor to hold `other.merge_mutex_` locked while the member-wise
    /// moves happen, preventing race conditions with a concurrent 'write operations' on `other`.
    Configuration(Configuration& other, const std::lock_guard<std::mutex>&) noexcept;

  public:
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
        const ServiceIdentifierType& service_identifier_type) const noexcept;

    std::optional<std::reference_wrapper<const ServiceInstanceDeployment>> GetServiceInstanceDeployment(
        const InstanceSpecifier& specifier) const noexcept;

    /// \brief Merge service types and instances into this configuration.
    /// Checks for clashes in type names and instance names and will return error in this case.
    /// \attention In case that the merge fails, the configuration will be left in an undefined state and should not be
    /// used anymore.
    Result<void> MergeServiceEntries(const Configuration& additional_configuration) noexcept;

    size_t GetNumberOfServiceTypes() const noexcept;

    bool IsServiceTypesEmpty() const noexcept
    {
        return GetNumberOfServiceTypes() == 0;
    }

    size_t GetNumberOfServiceInstances() const noexcept;

    bool IsServiceInstancesEmpty() const noexcept
    {
        return GetNumberOfServiceInstances() == 0;
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
    /// \brief Returns a copy of the merged entries across all persisted maps of service type deployments.
    /// \attention This returns an owned copy, so no references, pointers or string_views should be extracted and used
    ///         beyond the copy's lifetime. For that use case, use ForEachServiceType() instead, which iterates directly
    ///         over the persisted storage.
    ServiceTypeDeployments GetServiceTypes() const noexcept;

    /// \brief Returns a copy of the merged entries across all persisted maps of service isntance deployments.
    /// \attention This returns an owned copy, so no references, pointers or string_views should be extracted and used
    ///         beyond the copy's lifetime. For that use case, use ForEachServiceInstance() instead, which iterates
    ///         directly over the persisted storage.
    ServiceInstanceDeployments GetServiceInstances() const noexcept;

    /// \brief Invokes a callback with a const reference to every service type deployment entry
    ///        held across all persisted maps of service type deployments.
    /// \attention This function was introduced to have a copy free access to the entries in those maps.  This makes it
    ///            safe to extract references/string_views from the entries passed to callback and use them beyond
    ///            the lifetime of a single call, as long as this Configuration outlives them.
    template <typename Callback>
    void ForEachServiceType(Callback&& callback) const noexcept
    {
        const auto current_list = std::atomic_load_explicit(&service_types_, std::memory_order_acquire);
        if (current_list == nullptr)
        {
            return;
        }
        for (const auto& element : *current_list)
        {
            for (const auto& entry : *element)
            {
                callback(entry);
            }
        }
    }

    /// \brief Invokes a callback with a const reference to every service instance deployment entry
    ///        held across all persisted maps of service instance deployments.
    /// \attention Same lifetime guarantees as ForEachServiceType() above, but for service instance deployments.
    template <typename Callback>
    void ForEachServiceInstance(Callback&& callback) const noexcept
    {
        const auto current_list = std::atomic_load_explicit(&service_instances_, std::memory_order_acquire);
        if (current_list == nullptr)
        {
            return;
        }
        for (const auto& element : *current_list)
        {
            for (const auto& entry : *element)
            {
                callback(entry);
            }
        }
    }

    /// \brief Get list of maps of service types. If list is not defined or empty, this function will terminate the
    /// program.
    std::shared_ptr<std::list<std::shared_ptr<ServiceTypeDeployments>>> GetListOfServiceTypeMaps() const;

    /// \brief Get list of maps of service instances. If list is not defined or empty, this function will terminate the
    /// program.
    std::shared_ptr<std::list<std::shared_ptr<ServiceInstanceDeployments>>> GetListOfServiceInstanceMaps() const;

    /// \brief Helper function to check if entry for this service_identifier is already stored in list of service type
    /// deployments
    static bool CheckServiceTypeExists(
        const ServiceIdentifierType& service_identifier,
        const std::shared_ptr<
            std::list<std::shared_ptr<std::unordered_map<ServiceIdentifierType, ServiceTypeDeployment>>>>&
            current_list);

    /// \brief Helper function to check if entry for this instance_identifier is already stored in list of instance
    /// deployments
    static bool CheckServiceInstanceExists(
        const InstanceSpecifier& instance_identifier,
        const std::shared_ptr<
            std::list<std::shared_ptr<std::unordered_map<InstanceSpecifier, ServiceInstanceDeployment>>>>&
            current_list);

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
     * @brief List of all generations of the map containing the configured service type deployments.
     *
     * Key is the ServiceIdentifierType, value is the ServiceTypeDeployment.
     *
     * Each write update, i.e. AddServiceTypeDeployment() and MergeServiceEntries() will create a new map
     * that will be added to this list. The different versions are stored in a list so that the pointers to the elements
     * in it, stay valid for the lifetime of this configuration. Stored as std::shared_ptr so that atomic updates can be
     * done to the list of maps, while the previous generations are still accessible by readers.
     */
    std::shared_ptr<std::list<std::shared_ptr<ServiceTypeDeployments>>> service_types_;

    /**
     * @brief List of all generations of the map containing the configured service instance deployments.
     *
     * Key is the InstanceSpecifier, value is the ServiceInstanceDeployment.
     *
     * Each write update, i.e. AddServiceInstanceDeployment() and MergeServiceEntries() will create a new map
     * that will be added to this list. The different versions are stored in a list so that the pointers to the elements
     * in it, stay valid for the lifetime of this configuration. Stored as std::shared_ptr so that atomic updates can be
     * done to the list of maps, while the previous generations are still accessible by readers.
     */
    std::shared_ptr<std::list<std::shared_ptr<ServiceInstanceDeployments>>> service_instances_;
    GlobalConfiguration global_configuration_;
    TracingConfiguration tracing_configuration_;

    /// This mutex is only used to avoid multiple simultaneous merges, i.e. calls of MergeServiceEntries(). On the read
    /// path the usage of this mutex can be avoided because the list of configuration elements will be loaded via atomic
    /// operations.
    std::mutex merge_mutex_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_CONFIGURATION_CONFIGURATION_H
