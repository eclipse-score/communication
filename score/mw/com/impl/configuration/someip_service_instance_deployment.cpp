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
#include "score/mw/com/impl/configuration/someip_service_instance_deployment.h"

#include "score/mw/com/impl/configuration/configuration_common_resources.h"

#include "score/json/json_writer.h"

#include <cstdlib>
#include <exception>
#include <limits>
#include <sstream>
#include <string>

namespace score::mw::com::impl
{

namespace
{

constexpr auto kSerializationVersionKeySerInstDepl = "serializationVersion";
constexpr auto kInstanceIdKeySerInstDepl = "instanceId";
constexpr auto kEventsKeySerInstDepl = "events";
constexpr auto kFieldsKeySerInstDepl = "fields";

}  // namespace

auto areCompatible(const SomeIpServiceInstanceDeployment& lhs, const SomeIpServiceInstanceDeployment& rhs) -> bool
{
    if ((lhs.instance_id_.has_value() == false) || (rhs.instance_id_.has_value() == false))
    {
        return true;
    }
    return lhs.instance_id_ == rhs.instance_id_;
}

bool operator==(const SomeIpServiceInstanceDeployment& lhs, const SomeIpServiceInstanceDeployment& rhs) noexcept
{
    return (lhs.instance_id_ == rhs.instance_id_);
}

// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall not be called
//                                                                   implicitly"
// coverity reports that json.As<T>().value might throw due to std::bad_variant_access. Which will cause an implicit
// terminate.
// The .value() call will only throw if the  As<T> function didn't succeed in parsing the json. In this case termination
// is the intended behaviour.
//
// Suppress "AUTOSAR C++14 A12-6-1" rule finding. This rule declares: "All class data members that are
// initialized by the constructor shall be initialized using member initializers".
// This is false positive, all data members are initialized using member initializers in the delegation constructor.
//
// coverity[autosar_cpp14_a15_5_3_violation]
// coverity[autosar_cpp14_a12_6_1_violation]
SomeIpServiceInstanceDeployment::SomeIpServiceInstanceDeployment(const score::json::Object& json_object) noexcept
    : SomeIpServiceInstanceDeployment{
          {},
          ConvertJsonToServiceElementMap<EventInstanceMapping>(json_object, kEventsKeySerInstDepl),
          ConvertJsonToServiceElementMap<FieldInstanceMapping>(json_object, kFieldsKeySerInstDepl)}
{
    const auto instance_id_it = json_object.find(kInstanceIdKeySerInstDepl);
    if (instance_id_it != json_object.end())
    {
        instance_id_ = instance_id_it->second.As<json::Object>().value();
    }

    const auto serialization_version =
        GetValueFromJson<std::uint32_t>(json_object, kSerializationVersionKeySerInstDepl);
    if (serialization_version != serializationVersion)
    {
        std::terminate();
    }
}

score::json::Object SomeIpServiceInstanceDeployment::Serialize() const noexcept
{
    score::json::Object json_object{};
    json_object[kSerializationVersionKeySerInstDepl] = score::json::Any{serializationVersion};

    if (instance_id_.has_value())
    {
        json_object[kInstanceIdKeySerInstDepl] = instance_id_.value().Serialize();
    }

    json_object[kEventsKeySerInstDepl] = ConvertServiceElementMapToJson(events_);
    json_object[kFieldsKeySerInstDepl] = ConvertServiceElementMapToJson(fields_);

    return json_object;
}

}  // namespace score::mw::com::impl
