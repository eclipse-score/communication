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
#ifndef SCORE_MW_COM_IMPL_CONFIGURATION_LOLA_SERVICE_INSTANCE_DEPLOYMENT_H
#define SCORE_MW_COM_IMPL_CONFIGURATION_LOLA_SERVICE_INSTANCE_DEPLOYMENT_H

#include "score/mw/com/impl/configuration/lola_event_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_field_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"
#include "score/mw/com/impl/configuration/quality_type.h"

#include "score/mw/com/impl/service_element_type.h"

#include "score/mw/log/logging.h"

#include <score/optional.hpp>

#include <cstdint>
#include <exception>
#include <string>
#include <unordered_map>
#include <vector>

namespace score::mw::com::impl
{

class LolaServiceInstanceDeployment
{
  public:
    using EventInstanceMapping = std::unordered_map<std::string, LolaEventInstanceDeployment>;
    using FieldInstanceMapping = std::unordered_map<std::string, LolaFieldInstanceDeployment>;

    LolaServiceInstanceDeployment() = default;
    explicit LolaServiceInstanceDeployment(const score::json::Object& json_object) noexcept;
    explicit LolaServiceInstanceDeployment(const score::cpp::optional<LolaServiceInstanceId> instance_id,
                                           EventInstanceMapping events = {},
                                           FieldInstanceMapping fields = {},
                                           const bool strict_permission = false) noexcept;

    constexpr static std::uint32_t serializationVersion{1U};
    // Note the struct is not compliant to POD type containing non-POD member.
    // The struct is used as a config storage obtained by performing the parsing json object.
    // Public access is required by the implementation to reach the following members of the struct.
    // coverity[autosar_cpp14_m11_0_1_violation]
    score::cpp::optional<LolaServiceInstanceId> instance_id_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    score::cpp::optional<std::size_t> shared_memory_size_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    EventInstanceMapping events_;  // key = event name
    // coverity[autosar_cpp14_m11_0_1_violation]
    FieldInstanceMapping fields_;  // key = field name
    // coverity[autosar_cpp14_m11_0_1_violation]
    bool strict_permissions_{false};
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::unordered_map<QualityType, std::vector<uid_t>> allowed_consumer_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::unordered_map<QualityType, std::vector<uid_t>> allowed_provider_;

    score::json::Object Serialize() const noexcept;
    bool ContainsEvent(const std::string& event_name) const noexcept;
    bool ContainsField(const std::string& field_name) const noexcept;
};

bool areCompatible(const LolaServiceInstanceDeployment& lhs, const LolaServiceInstanceDeployment& rhs) noexcept;
bool operator==(const LolaServiceInstanceDeployment& lhs, const LolaServiceInstanceDeployment& rhs) noexcept;

template <ServiceElementType service_element_type>
const auto& GetServiceElementInstanceDeployment(const LolaServiceInstanceDeployment& lola_service_instance_deployment,
                                                const std::string& event_name)
{
    const auto& service_element_instance_deployments = [&lola_service_instance_deployment]() -> const auto& {
        if constexpr (service_element_type == ServiceElementType::EVENT)
        {
            return lola_service_instance_deployment.events_;
        }
        if constexpr (service_element_type == ServiceElementType::FIELD)
        {
            return lola_service_instance_deployment.fields_;
        }
        score::mw::log::LogFatal()
            << "Invalid service element type. Could not get service element instance deployment. Terminating";
        std::terminate();
    }();

    const auto service_element_instance_deployment_it = service_element_instance_deployments.find(event_name);
    if (service_element_instance_deployment_it == service_element_instance_deployments.cend())
    {
        score::mw::log::LogFatal() << service_element_type << "name \"" << event_name
                                 << "\"does not exist in LolaServiceInstanceDeployment. Terminating.";
        std::terminate();
    }
    return service_element_instance_deployment_it->second;
}

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_CONFIGURATION_LOLA_SERVICE_INSTANCE_DEPLOYMENT_H
