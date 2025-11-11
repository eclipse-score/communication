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
#ifndef SCORE_MW_COM_IMPL_CONFIGURATION_SOMEIP_SERVICE_INSTANCE_DEPLOYMENT_H
#define SCORE_MW_COM_IMPL_CONFIGURATION_SOMEIP_SERVICE_INSTANCE_DEPLOYMENT_H

#include "score/mw/com/impl/configuration/someip_event_instance_deployment.h"
#include "score/mw/com/impl/configuration/someip_field_instance_deployment.h"
#include "score/mw/com/impl/configuration/someip_service_instance_id.h"

#include "score/json/json_parser.h"

#include <score/optional.hpp>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>

namespace score::mw::com::impl
{

class SomeIpServiceInstanceDeployment
{
  public:
    using EventInstanceMapping = std::unordered_map<std::string, SomeIpEventInstanceDeployment>;
    using FieldInstanceMapping = std::unordered_map<std::string, SomeIpFieldInstanceDeployment>;

    explicit SomeIpServiceInstanceDeployment(const score::json::Object& json_object) noexcept;
    explicit SomeIpServiceInstanceDeployment(score::cpp::optional<SomeIpServiceInstanceId> instance_id = {},
                                             EventInstanceMapping events = {},
                                             FieldInstanceMapping fields = {})
        : instance_id_{instance_id}, events_{std::move(events)}, fields_{std::move(fields)}
    {
    }

    constexpr static std::uint32_t serializationVersion{1U};

    // Note the struct is not compliant to POD type containing non-POD member.
    // The struct is used as a config storage obtained by performing the parsing json object.
    // Public access is required by the implementation to reach the following members of the struct.
    // coverity[autosar_cpp14_m11_0_1_violation]
    score::cpp::optional<SomeIpServiceInstanceId> instance_id_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    EventInstanceMapping events_;  // key = event name
    // coverity[autosar_cpp14_m11_0_1_violation]
    FieldInstanceMapping fields_;  // key = field name

    score::json::Object Serialize() const noexcept;
};

bool areCompatible(const SomeIpServiceInstanceDeployment& lhs, const SomeIpServiceInstanceDeployment& rhs);
bool operator==(const SomeIpServiceInstanceDeployment& lhs, const SomeIpServiceInstanceDeployment& rhs) noexcept;
bool operator<(const SomeIpServiceInstanceDeployment& lhs, const SomeIpServiceInstanceDeployment& rhs) noexcept;

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_CONFIGURATION_SOMEIP_SERVICE_INSTANCE_DEPLOYMENT_H
