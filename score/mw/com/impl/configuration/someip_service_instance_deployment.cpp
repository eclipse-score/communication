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

#include <exception>
#include <utility>

namespace score::mw::com::impl
{

namespace
{

constexpr auto kSerializationVersionKeySomeIpInstDepl = "serializationVersion";
constexpr auto kInstanceIdKeySomeIpInstDepl = "instanceId";
constexpr auto kEventsKeySomeIpInstDepl = "events";
constexpr auto kFieldsKeySomeIpInstDepl = "fields";

}  // namespace

auto areCompatible(const SomeIpServiceInstanceDeployment& lhs, const SomeIpServiceInstanceDeployment& rhs) noexcept
    -> bool
{
    if ((!lhs.instance_id_.has_value()) || (!rhs.instance_id_.has_value()))
    {
        return true;
    }
    return lhs.instance_id_ == rhs.instance_id_;
}

bool operator==(const SomeIpServiceInstanceDeployment& lhs, const SomeIpServiceInstanceDeployment& rhs) noexcept
{
    // Suppress "AUTOSAR C++14 A5-2-6" rule finding. This rule states: "The operands of a logical && or || shall be
    // parenthesized if the operands contain binary operators.". All logical operators here are the same (&&), so the
    // evaluation order is unambiguous.
    // coverity[autosar_cpp14_a5_2_6_violation]
    return ((lhs.instance_id_ == rhs.instance_id_) && (lhs.events_ == rhs.events_) && (lhs.fields_ == rhs.fields_));
}

bool operator<(const SomeIpServiceInstanceDeployment& lhs, const SomeIpServiceInstanceDeployment& rhs) noexcept
{
    return lhs.instance_id_ < rhs.instance_id_;
}

// In this case the constructor delegation does not provide additional code structuring because of the std::optional.
// coverity[autosar_cpp14_a12_1_5_violation]
// coverity[autosar_cpp14_a15_5_3_violation]
SomeIpServiceInstanceDeployment::SomeIpServiceInstanceDeployment(const score::json::Object& json_object)
    : SomeIpServiceInstanceDeployment{
          {},
          ConvertJsonToServiceElementMap<EventInstanceMapping>(json_object, kEventsKeySomeIpInstDepl),
          ConvertJsonToServiceElementMap<FieldInstanceMapping>(json_object, kFieldsKeySomeIpInstDepl)}
{
    const auto serialization_version =
        GetValueFromJson<std::uint32_t>(json_object, kSerializationVersionKeySomeIpInstDepl);
    if (serialization_version != serializationVersion)
    {
        std::terminate();
    }

    const auto instance_id_it = json_object.find(kInstanceIdKeySomeIpInstDepl);
    if (instance_id_it != json_object.end())
    {
        instance_id_ = SomeIpServiceInstanceId{instance_id_it->second.As<json::Object>().value()};
    }
}

// Suppress "AUTOSAR C++14 A12-1-5" rule finding.
// This rule states: Common class initialization for non-constant members shall be done by a delegating constructor.
// Justification: This constructor is used by other constructors for delegation.
// coverity[autosar_cpp14_a12_1_5_violation]
SomeIpServiceInstanceDeployment::SomeIpServiceInstanceDeployment(
    const std::optional<SomeIpServiceInstanceId>& instance_id,
    EventInstanceMapping events,
    FieldInstanceMapping fields) noexcept
    : instance_id_{instance_id}, events_{std::move(events)}, fields_{std::move(fields)}
{
}

score::json::Object SomeIpServiceInstanceDeployment::Serialize() const
{
    json::Object json_object{};
    json_object[kSerializationVersionKeySomeIpInstDepl] = score::json::Any{serializationVersion};

    if (instance_id_.has_value())
    {
        json_object[kInstanceIdKeySomeIpInstDepl] = instance_id_.value().Serialize();
    }

    json_object[kEventsKeySomeIpInstDepl] = ConvertServiceElementMapToJson(events_);
    json_object[kFieldsKeySomeIpInstDepl] = ConvertServiceElementMapToJson(fields_);

    return json_object;
}

bool SomeIpServiceInstanceDeployment::ContainsEvent(const std::string& event_name) const noexcept
{
    return (events_.find(event_name) != events_.end());
}

bool SomeIpServiceInstanceDeployment::ContainsField(const std::string& field_name) const noexcept
{
    return (fields_.find(field_name) != fields_.end());
}

}  // namespace score::mw::com::impl
