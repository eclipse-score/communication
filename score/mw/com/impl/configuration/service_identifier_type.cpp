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
#include "score/mw/com/impl/configuration/service_identifier_type.h"

#include "score/mw/com/impl/configuration/configuration_common_resources.h"

#include <exception>
#include <string_view>

namespace score::mw::com::impl
{

namespace
{

constexpr auto kSerializationVersionKeyServIdentType = "serializationVersion";
constexpr auto kServiceTypeKeyServIdentType = "serviceType";
constexpr auto kVersionKeyServIdentType = "version";

}  // namespace

// False positive: Other constructors are delegated to this one, so this one can not be further delegated
// coverity[autosar_cpp14_a12_1_5_violation] See above
ServiceIdentifierType::ServiceIdentifierType(std::string serviceTypeName, ServiceVersionType version)
    : serviceTypeName_{std::move(serviceTypeName)}, version_{version}, serialized_string_{ToStringImpl(Serialize())}
{
}

ServiceIdentifierType::ServiceIdentifierType(std::string serviceTypeName,
                                             const std::uint32_t major_version_number,
                                             const std::uint32_t minor_version_number)
    : ServiceIdentifierType(std::move(serviceTypeName),
                            make_ServiceVersionType(major_version_number, minor_version_number))
{
}

ServiceIdentifierType::ServiceIdentifierType(const score::json::Object& json_object) noexcept
    : ServiceIdentifierType(
          GetValueFromJson<std::string_view>(json_object, kServiceTypeKeyServIdentType).data(),
          static_cast<ServiceVersionType>(GetValueFromJson<json::Object>(json_object, kVersionKeyServIdentType)))
{
    const auto serialization_version =
        GetValueFromJson<std::uint32_t>(json_object, kSerializationVersionKeyServIdentType);
    if (serialization_version != serializationVersion)
    {
        std::terminate();
    }
}

auto ServiceIdentifierType::ToString() const noexcept -> std::string_view
{
    return serviceTypeName_;
}

auto ServiceIdentifierType::Serialize() const noexcept -> score::json::Object
{
    json::Object json_object{};
    json_object[kServiceTypeKeyServIdentType] = score::json::Any{std::string{serviceTypeName_}};
    json_object[kVersionKeyServIdentType] = score::json::Any{version_.Serialize()};
    json_object[kSerializationVersionKeyServIdentType] = json::Any{serializationVersion};
    return json_object;
}

auto operator==(const ServiceIdentifierType& lhs, const ServiceIdentifierType& rhs) noexcept -> bool
{
    return (((lhs.serviceTypeName_ == rhs.serviceTypeName_) && (lhs.version_ == rhs.version_)));
}

auto operator<(const ServiceIdentifierType& lhs, const ServiceIdentifierType& rhs) noexcept -> bool
{
    if (lhs.serviceTypeName_ == rhs.serviceTypeName_)
    {
        return lhs.version_ < rhs.version_;
    }
    return lhs.serviceTypeName_ < rhs.serviceTypeName_;
}

}  // namespace score::mw::com::impl
