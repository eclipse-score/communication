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
#include "score/mw/com/impl/configuration/someip_service_instance_deployment.h"

#include "score/mw/com/impl/configuration/configuration_common_resources.h"

#include "score/json/json_parser.h"

#include <exception>

namespace score::mw::com::impl
{

namespace
{

constexpr auto kInstanceIdKeySomeIpSerInstDepl = "instanceId";
constexpr auto kSerializationVersionKeySomeIpSerInstDepl = "serializationVersion";

std::optional<SomeIpServiceInstanceId> GetInstanceIdFromJson(const score::json::Object& json_object)
{
    const auto instance_id_it = json_object.find(kInstanceIdKeySomeIpSerInstDepl);
    if (instance_id_it == json_object.cend())
    {
        return std::nullopt;
    }
    return SomeIpServiceInstanceId{
        GetValueFromJson<SomeIpServiceInstanceId::InstanceId>(json_object, kInstanceIdKeySomeIpSerInstDepl)};
}

}  // namespace

// Suppress "AUTOSAR C++14 A12-1-5" rule finding.
// This rule states: Common class initialization for non-constant members shall be done by a delegating constructor.
// Justification: This constructor is used by other constructors for delegation.
// coverity[autosar_cpp14_a12_1_5_violation]
SomeIpServiceInstanceDeployment::SomeIpServiceInstanceDeployment(
    const std::optional<SomeIpServiceInstanceId>& instance_id) noexcept
    : instance_id_{instance_id}
{
}

// Suppress "AUTOSAR C++14 A12-1-5" rule finding.
// This rule states: Common class initialization for non-constant members shall be done by a delegating constructor.
// Justification: Delegating constructor is used.
// coverity[autosar_cpp14_a12_1_5_violation]
SomeIpServiceInstanceDeployment::SomeIpServiceInstanceDeployment(const score::json::Object& json_object)
    : SomeIpServiceInstanceDeployment{GetInstanceIdFromJson(json_object)}
{
    const auto serialization_version =
        GetValueFromJson<std::uint32_t>(json_object, kSerializationVersionKeySomeIpSerInstDepl);
    if (serialization_version != serializationVersion)
    {
        std::terminate();
    }
}

score::json::Object SomeIpServiceInstanceDeployment::Serialize() const
{
    score::json::Object json_object{};
    json_object[kSerializationVersionKeySomeIpSerInstDepl] = json::Any{serializationVersion};
    if (instance_id_.has_value())
    {
        json_object[kInstanceIdKeySomeIpSerInstDepl] = score::json::Any{instance_id_->GetId()};
    }
    return json_object;
}

bool areCompatible(const SomeIpServiceInstanceDeployment& lhs, const SomeIpServiceInstanceDeployment& rhs) noexcept
{
    if ((!lhs.instance_id_.has_value()) || (!rhs.instance_id_.has_value()))
    {
        return true;
    }
    return lhs.instance_id_ == rhs.instance_id_;
}

bool operator==(const SomeIpServiceInstanceDeployment& lhs, const SomeIpServiceInstanceDeployment& rhs) noexcept
{
    return lhs.instance_id_ == rhs.instance_id_;
}

bool operator<(const SomeIpServiceInstanceDeployment& lhs, const SomeIpServiceInstanceDeployment& rhs) noexcept
{
    return lhs.instance_id_ < rhs.instance_id_;
}

}  // namespace score::mw::com::impl
