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

#include <score/optional.hpp>

#include <cstdint>
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

const LolaEventInstanceDeployment& GetEventInstanceDeployment(
    const LolaServiceInstanceDeployment& lola_service_instance_deployment,
    const std::string& event_name);
const LolaFieldInstanceDeployment& GetFieldInstanceDeployment(
    const LolaServiceInstanceDeployment& lola_service_instance_deployment,
    const std::string& field_name);

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_CONFIGURATION_LOLA_SERVICE_INSTANCE_DEPLOYMENT_H
