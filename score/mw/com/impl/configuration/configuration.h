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

#include <cstdint>
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

    Configuration(ServiceTypeDeployments service_types,
                  ServiceInstanceDeployments service_instances,
                  GlobalConfiguration global_configuration,
                  TracingConfiguration tracing_configuration) noexcept;
    ~Configuration() noexcept = default;

    /**
     * \brief Class is moveable but not copyable
     */
    Configuration(const Configuration& other) = delete;
    Configuration(Configuration&& other) = default;
    Configuration& operator=(const Configuration& other) & = delete;
    Configuration& operator=(Configuration&& other) & = delete;

    ServiceTypeDeployment* AddServiceTypeDeployment(ServiceIdentifierType service_identifier_type,
                                                    ServiceTypeDeployment service_type_deployment) noexcept;
    ServiceInstanceDeployment* AddServiceInstanceDeployments(
        InstanceSpecifier instance_specifier,
        ServiceInstanceDeployment service_instance_deployment) noexcept;

    const ServiceTypeDeployments& GetServiceTypes() const noexcept
    {
        return service_types_;
    }
    const ServiceInstanceDeployments& GetServiceInstances() const noexcept
    {
        return service_instances_;
    }
    const GlobalConfiguration& GetGlobalConfiguration() const noexcept
    {
        return global_configuration_;
    }
    const TracingConfiguration& GetTracingConfiguration() const noexcept
    {
        return tracing_configuration_;
    }

  private:
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
