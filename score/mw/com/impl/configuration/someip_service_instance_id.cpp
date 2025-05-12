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
#include "score/mw/com/impl/configuration/someip_service_instance_id.h"

#include "score/mw/com/impl/configuration/configuration_common_resources.h"

#include "score/json/json_parser.h"

#include <exception>
#include <iomanip>
#include <limits>
#include <sstream>

namespace score::mw::com::impl
{

namespace
{

constexpr auto kInstanceIdKey = "instanceId";

}  // namespace

SomeIpServiceInstanceId::SomeIpServiceInstanceId(InstanceId instance_id) noexcept
    : id_{instance_id}, hash_string_{ToHashStringImpl(id_, hashStringSize)}
{
}

SomeIpServiceInstanceId::SomeIpServiceInstanceId(const score::json::Object& json_object) noexcept
    : id_{GetValueFromJson<InstanceId>(json_object, kInstanceIdKey)},
      hash_string_{ToHashStringImpl(id_, hashStringSize)}
{
    const auto serialization_version = GetValueFromJson<std::uint32_t>(json_object, kSerializationVersionKey);
    if (serialization_version != serializationVersion)
    {
        std::terminate();
    }
}

score::json::Object SomeIpServiceInstanceId::Serialize() const noexcept
{
    score::json::Object json_object{};
    json_object[kInstanceIdKey] = score::json::Any{id_};
    json_object[kSerializationVersionKey] = json::Any{serializationVersion};
    return json_object;
}

std::string_view SomeIpServiceInstanceId::ToHashString() const noexcept
{
    return hash_string_;
}

SomeIpServiceInstanceId::InstanceId SomeIpServiceInstanceId::GetId() const noexcept
{
    return id_;
}

bool operator==(const SomeIpServiceInstanceId& lhs, const SomeIpServiceInstanceId& rhs) noexcept
{
    return lhs.GetId() == rhs.GetId();
}

bool operator<(const SomeIpServiceInstanceId& lhs, const SomeIpServiceInstanceId& rhs) noexcept
{
    return lhs.GetId() < rhs.GetId();
}

}  // namespace score::mw::com::impl
