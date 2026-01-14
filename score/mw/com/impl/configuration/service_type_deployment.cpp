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
#include "score/mw/com/impl/configuration/service_type_deployment.h"

#include "score/mw/com/impl/configuration/configuration_common_resources.h"

#include "score/json/json_parser.h"

#include <score/overload.hpp>
#include <limits>
#include <sstream>

namespace score::mw::com::impl
{

namespace
{

// coverity[autosar_cpp14_a2_10_4_violation] False positive, function is in anonymous namespace
ServiceTypeDeployment::BindingInformation GetBindingInfoFromJson(const score::json::Object& json_object) noexcept
{
    const auto variant_index = GetValueFromJson<std::ptrdiff_t>(json_object, kBindingInfoIndexKey);

    const auto binding_information =
        DeserializeVariant<0, ServiceTypeDeployment::BindingInformation>(json_object, variant_index, kBindingInfoKey);

    return binding_information;
}

// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall not be called
//                                                                   implicitly"
// coverity reports that GetAsStringView might throw since it calls std::visit, which itself
// might throw std::bad_variant_access.
//
// std::visit will only throw std::bad_variant_access if the variant is empty by exception. Since we do not use
// exceptions, a variant can never be in this state, i.e. std::visit can never throw std::bad_variant_access.
//
// False positive, other 'ToHashStringImpl' methods in namespace have different signature
//
// coverity[autosar_cpp14_a2_10_4_violation] False positive, function is in anonymous namespace
// coverity[autosar_cpp14_a2_10_1_violation] See above
// coverity[autosar_cpp14_a15_5_3_violation]
std::string ToHashStringImpl(const ServiceTypeDeployment::BindingInformation& binding_info) noexcept
{
    // The conversion to hex string below does not work with a std::uint8_t, so we cast it to an int. However, we
    // ensure that the value is less than 16 to ensure it will fit with a single char in the string representation.
    static_assert(std::variant_size<ServiceTypeDeployment::BindingInformation>::value <= 0xFU,
                  "BindingInformation variant size should be less than 16");
    // coverity[autosar_cpp14_a4_7_1_violation] Static assert ensures that only uint8_t values are used
    auto binding_info_index = static_cast<int>(binding_info.index());

    /// \todo When we can use C++17 features, use std::to_chars so that we can convert from an int to a hex string
    /// with no dynamic allocations.
    std::stringstream binding_index_string_stream;
    // Passing std::hex to std::stringstream object with the stream operator follows the idiomatic way that both
    // features in conjunction were designed in the C++ standard.
    // coverity[autosar_cpp14_m8_4_4_violation] See above
    binding_index_string_stream << std::hex << binding_info_index;

    auto visitor = score::cpp::overload(
        [](const LolaServiceTypeDeployment& service_type_deployment) noexcept -> std::string_view {
            return service_type_deployment.ToHashString();
        },
        // FP: only one statement in this line
        // coverity[autosar_cpp14_a7_1_7_violation]
        [](const score::cpp::blank&) noexcept -> std::string_view {
            return "";
        });
    const auto binding_service_type_deployment_hash_string_view = std::visit(visitor, binding_info);
    std::string binding_service_type_deployment_hash_string{binding_service_type_deployment_hash_string_view};

    std::string combined_hash_string = binding_index_string_stream.str() + binding_service_type_deployment_hash_string;
    return combined_hash_string;
}

}  // namespace

// Suppress "AUTOSAR C++14 A12-1-5" rule finding.
// This rule states: Common class initialization for non-constant members shall be done by a delegating constructor.
// Justification: This constructor is used by other constructors for delegation.
// coverity[autosar_cpp14_a12_1_5_violation]
ServiceTypeDeployment::ServiceTypeDeployment(const BindingInformation binding) noexcept
    : binding_info_{binding}, hash_string_{ToHashStringImpl(binding_info_)}
{
}

// Suppress "AUTOSAR C++14 A12-1-5" rule finding.
// This rule states: Common class initialization for non-constant members shall be done by a delegating constructor.
// Justification: Delegating constructor is used.
// coverity[autosar_cpp14_a12_1_5_violation]
ServiceTypeDeployment::ServiceTypeDeployment(const score::json::Object& json_object) noexcept
    : ServiceTypeDeployment{GetBindingInfoFromJson(json_object)}
{
    const auto serialization_version = GetValueFromJson<std::uint32_t>(json_object, kSerializationVersionKey);
    if (serialization_version != serializationVersion)
    {
        std::terminate();
    }
}

// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall not be called
//                                                                   implicitly"
// coverity reports that GetAsStringView might throw since it calls std::visit, which itself
// might throw std::bad_variant_access.
// std::visit will only throw std::bad_variant_access if the variant is empty by exception. Since we do not use
// exceptions, a variant can never be in this state, i.e. std::visit can never throw std::bad_variant_access.
// coverity[autosar_cpp14_a15_5_3_violation]
score::json::Object ServiceTypeDeployment::Serialize() const noexcept
{
    score::json::Object json_object{};
    json_object[kBindingInfoIndexKey] = score::json::Any{binding_info_.index()};
    json_object[kSerializationVersionKey] = json::Any{serializationVersion};

    auto visitor = score::cpp::overload(
        [&json_object](const LolaServiceTypeDeployment& deployment) {
            json_object[kBindingInfoKey] = deployment.Serialize();
        },
        [](const score::cpp::blank&) noexcept {});
    std::visit(visitor, binding_info_);

    return json_object;
}

std::string_view ServiceTypeDeployment::ToHashString() const noexcept
{
    return hash_string_;
}

bool operator==(const ServiceTypeDeployment& lhs, const ServiceTypeDeployment& rhs) noexcept
{
    return lhs.binding_info_ == rhs.binding_info_;
}

}  // namespace score::mw::com::impl
