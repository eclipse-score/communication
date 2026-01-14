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
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"

#include "score/mw/com/impl/configuration/configuration_common_resources.h"

#include "score/json/json_parser.h"

#include <score/assert.hpp>

#include <exception>

namespace score::mw::com::impl
{

namespace
{

constexpr auto kInstanceIdKeyLolaSerInstID = "instanceId";
constexpr auto kSerializationVersionKeyLolaSerInstID = "serializationVersion";

}  // namespace

// Suppress "AUTOSAR C++14 A12-1-5" rule finding.
// This rule states: Common class initialization for non-constant members shall be done by a delegating constructor.
// Justification: This constructor is used by other constructors for delegation.
// coverity[autosar_cpp14_a12_1_5_violation]
LolaServiceInstanceId::LolaServiceInstanceId(InstanceId instance_id) noexcept
    : id_{instance_id}, hash_string_{ToHashStringImpl(id_, hashStringSize)}
{
}

// Suppress "AUTOSAR C++14 A12-1-5" rule finding.
// This rule states: Common class initialization for non-constant members shall be done by a delegating constructor.
// Justification: Delegating constructor is used.
// coverity[autosar_cpp14_a12_1_5_violation]
LolaServiceInstanceId::LolaServiceInstanceId(const score::json::Object& json_object) noexcept
    : LolaServiceInstanceId{GetValueFromJson<InstanceId>(json_object, kInstanceIdKeyLolaSerInstID)}
{
    const auto serialization_version =
        GetValueFromJson<std::uint32_t>(json_object, kSerializationVersionKeyLolaSerInstID);
    if (serialization_version != serializationVersion)
    {
        std::terminate();
    }
}

score::json::Object LolaServiceInstanceId::Serialize() const noexcept
{
    score::json::Object json_object{};
    json_object[kInstanceIdKeyLolaSerInstID] = score::json::Any{id_};
    json_object[kSerializationVersionKeyLolaSerInstID] = json::Any{serializationVersion};
    return json_object;
}

std::string_view LolaServiceInstanceId::ToHashString() const noexcept
{
    return hash_string_;
}

bool operator==(const LolaServiceInstanceId& lhs, const LolaServiceInstanceId& rhs) noexcept
{
    return lhs.GetId() == rhs.GetId();
}

bool operator<(const LolaServiceInstanceId& lhs, const LolaServiceInstanceId& rhs) noexcept
{
    return lhs.GetId() < rhs.GetId();
}

}  // namespace score::mw::com::impl
