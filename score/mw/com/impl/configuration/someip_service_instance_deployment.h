/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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
#ifndef SCORE_MW_COM_IMPL_CONFIGURATION_SOMEIP_SERVICE_INSTANCE_DEPLOYMENT_H
#define SCORE_MW_COM_IMPL_CONFIGURATION_SOMEIP_SERVICE_INSTANCE_DEPLOYMENT_H

#include "score/mw/com/impl/configuration/someip_event_instance_deployment.h"
#include "score/mw/com/impl/configuration/someip_field_instance_deployment.h"
#include "score/mw/com/impl/configuration/someip_service_instance_id.h"

#include "score/mw/com/impl/service_element_type.h"

#include "score/mw/log/logging.h"

#include <score/assert.hpp>

#include <cstdint>
#include <exception>
#include <optional>
#include <string>
#include <unordered_map>

namespace score::mw::com::impl
{

/// \brief Per service-instance deployment information of the SOME/IP binding.
///
/// \details Mirrors LolaServiceInstanceDeployment, minus the members which only make sense for a shared-memory based
///          binding (shared memory sizes, uid based access permissions and inter-VM support).
class SomeIpServiceInstanceDeployment
{
  public:
    using EventInstanceMapping = std::unordered_map<std::string, SomeIpEventInstanceDeployment>;
    using FieldInstanceMapping = std::unordered_map<std::string, SomeIpFieldInstanceDeployment>;

    SomeIpServiceInstanceDeployment() = default;
    explicit SomeIpServiceInstanceDeployment(const score::json::Object& json_object);
    explicit SomeIpServiceInstanceDeployment(const std::optional<SomeIpServiceInstanceId>& instance_id,
                                             EventInstanceMapping events = {},
                                             FieldInstanceMapping fields = {}) noexcept;

    // coverity[autosar_cpp14_a0_1_1_violation]
    constexpr static std::uint32_t serializationVersion{1U};

    // coverity[autosar_cpp14_m11_0_1_violation]
    std::optional<SomeIpServiceInstanceId> instance_id_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    EventInstanceMapping events_;  // key = event name
    // coverity[autosar_cpp14_m11_0_1_violation]
    FieldInstanceMapping fields_;  // key = field name

    score::json::Object Serialize() const;
    bool ContainsEvent(const std::string& event_name) const noexcept;
    bool ContainsField(const std::string& field_name) const noexcept;
};

bool areCompatible(const SomeIpServiceInstanceDeployment& lhs, const SomeIpServiceInstanceDeployment& rhs) noexcept;
bool operator==(const SomeIpServiceInstanceDeployment& lhs, const SomeIpServiceInstanceDeployment& rhs) noexcept;
bool operator<(const SomeIpServiceInstanceDeployment& lhs, const SomeIpServiceInstanceDeployment& rhs) noexcept;

template <ServiceElementType service_element_type>
const auto& GetServiceElementInstanceDeployment(
    const SomeIpServiceInstanceDeployment& someip_service_instance_deployment,
    const std::string& service_element_name)
{
    static_assert((service_element_type == ServiceElementType::EVENT) ||
                      (service_element_type == ServiceElementType::FIELD),
                  "The SOME/IP binding currently only supports events and fields.");

    const auto& service_element_instance_deployments = [&someip_service_instance_deployment]() -> const auto& {
        if constexpr (service_element_type == ServiceElementType::EVENT)
        {
            return someip_service_instance_deployment.events_;
        }
        else
        {
            return someip_service_instance_deployment.fields_;
        }
    }();

    const auto service_element_instance_deployment_it =
        service_element_instance_deployments.find(service_element_name);
    if (service_element_instance_deployment_it == service_element_instance_deployments.cend())
    {
        score::mw::log::LogFatal() << service_element_type << "name \"" << service_element_name
                                   << "\"does not exist in SomeIpServiceInstanceDeployment. Terminating.";
        std::terminate();
    }
    return service_element_instance_deployment_it->second;
}

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_CONFIGURATION_SOMEIP_SERVICE_INSTANCE_DEPLOYMENT_H
