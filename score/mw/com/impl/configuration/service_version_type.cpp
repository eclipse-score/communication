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
#include "score/mw/com/impl/configuration/service_version_type.h"

#include "score/mw/com/impl/configuration/configuration_common_resources.h"

#include "score/json/json_writer.h"

#include <score/assert.hpp>

#include <exception>

namespace score::mw::com::impl
{

namespace
{

constexpr auto kSerializationVersionKeySerVerType = "serializationVersion";
constexpr auto kMajorVersionKeySerVerType = "majorVersion";
constexpr auto kMinorVersionKeySerVerType = "minorVersion";

}  // namespace

ServiceVersionType::ServiceVersionType(const std::uint32_t major_version_number,
                                       const std::uint32_t minor_version_number)
    : major_{major_version_number}, minor_{minor_version_number}, serialized_string_{ToStringImpl(Serialize())}
{
}

// Suppress "AUTOSAR C++14 A12-6-1" rule finding. This rule declares: "All class data members that are
// initialized by the constructor shall be initialized using member initializers".
// This is false positive, all data members are initialized using member initializers in the delegation constructor.
// coverity[autosar_cpp14_a12_6_1_violation]
ServiceVersionType::ServiceVersionType(const score::json::Object& json_object) noexcept
    : ServiceVersionType{GetValueFromJson<std::uint32_t>(json_object, kMajorVersionKeySerVerType),
                         GetValueFromJson<std::uint32_t>(json_object, kMinorVersionKeySerVerType)}
{
    const auto serialization_version = GetValueFromJson<std::uint32_t>(json_object, kSerializationVersionKeySerVerType);
    serialized_string_ = ToStringImpl(json_object);
    if (serialization_version != serializationVersion)
    {
        std::terminate();
    }
}

auto ServiceVersionType::ToString() const noexcept -> std::string_view
{
    return serialized_string_;
}

auto ServiceVersionType::Serialize() const noexcept -> score::json::Object
{
    json::Object json_object{};
    json_object[kSerializationVersionKeySerVerType] = json::Any{serializationVersion};
    json_object[kMajorVersionKeySerVerType] = score::json::Any{major_};
    json_object[kMinorVersionKeySerVerType] = score::json::Any{minor_};

    return json_object;
}

auto operator==(const ServiceVersionType& lhs, const ServiceVersionType& rhs) noexcept -> bool
{
    return (((lhs.major_ == rhs.major_) && (lhs.minor_ == rhs.minor_)));
}

auto operator<(const ServiceVersionType& lhs, const ServiceVersionType& rhs) noexcept -> bool
{
    if (lhs.major_ == rhs.major_)
    {
        return lhs.minor_ < rhs.minor_;
    }
    return lhs.major_ < rhs.major_;
}

}  // namespace score::mw::com::impl
