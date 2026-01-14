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
#include "score/mw/com/impl/configuration/service_instance_id.h"

#include "score/mw/com/impl/configuration/configuration_common_resources.h"

#include "score/json/json_parser.h"

#include <score/overload.hpp>

#include <exception>
#include <limits>
#include <sstream>

namespace score::mw::com::impl
{

namespace
{

constexpr auto kBindingInfoKeySerInstID = "bindingInfo";
constexpr auto kBindingInfoIndexKeySerInstID = "bindingInfoIndex";
constexpr auto kSerializationVersionKeySerInstID = "serializationVersion";

// This function with this signature is only defined here. And is in  anonymous namespace.
// coverity[autosar_cpp14_a2_10_4_violation : FALSE]
ServiceInstanceId::BindingInformation GetBindingInfoFromJson(const score::json::Object& json_object) noexcept
{
    const auto variant_index = GetValueFromJson<std::ptrdiff_t>(json_object, kBindingInfoIndexKeySerInstID);

    const auto binding_information = DeserializeVariant<0, ServiceInstanceId::BindingInformation>(
        json_object, variant_index, kBindingInfoKeySerInstID);

    return binding_information;
}

// This is a false positive, the function is in an anonymous name space and has a different signature from other
// functions with same name.
//
// Note 1:
// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall not be called
//                                                                   implicitly"
// coverity reports that GetAsStringView might throw since it calls std::visit, which itself
// might throw std::bad_variant_access.
// std::visit will only throw std::bad_variant_access if the variant is empty by exception. Since we do not use
// exceptions, a variant can never be in this state, i.e. std::visit can never throw std::bad_variant_access.
//
// This function with this signature is only defined here. And is in  anonymous namespace.
//
// coverity[autosar_cpp14_a2_10_1_violation]
// coverity[autosar_cpp14_a15_5_3_violation] Note 1
// coverity[autosar_cpp14_a2_10_4_violation : FALSE]
std::string ToHashStringImpl(const ServiceInstanceId::BindingInformation& binding_info) noexcept
{

    // The conversion to hex string below does not work with a std::uint8_t, so we cast it to an int. However, we
    // ensure that the value is less than 16 to ensure it will fit with a single char in the string representation.
    static_assert(std::variant_size<ServiceInstanceId::BindingInformation>::value <= 0xFU,
                  "BindingInformation variant size should be less than 16");

    // The above assert checks that the variant type, of which binding_info_is a type of holds less variants than can
    // fit in uint8_t. Thus any variant index is safely castable into an integer.
    // coverity[autosar_cpp14_a4_7_1_violation]
    auto binding_info_index = static_cast<int>(binding_info.index());

    /// \todo When we can use C++17 features, use std::to_chars so that we can convert from an int to a hex string
    /// with no dynamic allocations.
    std::stringstream binding_index_string_stream;
    // Passing std::hex to std::stringstream object with the stream operator follows the idiomatic way that both
    // features in conjunction were designed in the C++ standard.
    // coverity[autosar_cpp14_m8_4_4_violation] See above
    binding_index_string_stream << std::hex << binding_info_index;

    auto visitor = score::cpp::overload(
        [](const LolaServiceInstanceId& instance_id) noexcept -> std::string_view {
            return instance_id.ToHashString();
        },
        [](const SomeIpServiceInstanceId& instance_id) noexcept -> std::string_view {
            return instance_id.ToHashString();
        },
        [](const score::cpp::blank&) noexcept -> std::string_view {
            return "";
        });
    const auto binding_instance_id_hash_string_view = std::visit(visitor, binding_info);
    std::string binding_instance_id_hash_string{binding_instance_id_hash_string_view};

    std::string combined_hash_string = binding_index_string_stream.str() + binding_instance_id_hash_string;
    return combined_hash_string;
}

}  // namespace

// Suppress "AUTOSAR C++14 A12-1-5" rule finding.
// This rule states: Common class initialization for non-constant members shall be done by a delegating constructor.
// Justification: This constructor is used by other constructors for delegation.
// coverity[autosar_cpp14_a12_1_5_violation]
ServiceInstanceId::ServiceInstanceId(BindingInformation binding_info) noexcept
    : binding_info_{std::move(binding_info)}, hash_string_{ToHashStringImpl(binding_info_)}
{
}

// Suppress "AUTOSAR C++14 A12-1-5" rule finding.
// This rule states: Common class initialization for non-constant members shall be done by a delegating constructor.
// Justification: Delegating constructor is used.
// coverity[autosar_cpp14_a12_1_5_violation]
ServiceInstanceId::ServiceInstanceId(const score::json::Object& json_object) noexcept
    : ServiceInstanceId{GetBindingInfoFromJson(json_object)}
{
    const auto serialization_version = GetValueFromJson<std::uint32_t>(json_object, kSerializationVersionKeySerInstID);
    if (serialization_version != serializationVersion)
    {
        std::terminate();
    }
}

// See Note 1
// coverity[autosar_cpp14_a15_5_3_violation]
score::json::Object ServiceInstanceId::Serialize() const noexcept
{
    score::json::Object json_object{};
    json_object[kBindingInfoIndexKeySerInstID] = score::json::Any{binding_info_.index()};
    json_object[kSerializationVersionKeySerInstID] = json::Any{serializationVersion};

    auto visitor = score::cpp::overload(
        [&json_object](const LolaServiceInstanceId& instance_id) {
            json_object[kBindingInfoKeySerInstID] = instance_id.Serialize();
        },
        [&json_object](const SomeIpServiceInstanceId& instance_id) {
            json_object[kBindingInfoKeySerInstID] = instance_id.Serialize();
        },
        [](const score::cpp::blank&) noexcept {});
    std::visit(visitor, binding_info_);
    return json_object;
}

std::string_view ServiceInstanceId::ToHashString() const noexcept
{
    return hash_string_;
}

// See Note 1
// coverity[autosar_cpp14_a15_5_3_violation]
bool operator==(const ServiceInstanceId& lhs, const ServiceInstanceId& rhs) noexcept
{
    auto visitor = score::cpp::overload(
        [&rhs](const LolaServiceInstanceId& lhs_lola) noexcept -> bool {
            const auto* const rhs_lola = std::get_if<LolaServiceInstanceId>(&rhs.binding_info_);
            if (rhs_lola == nullptr)
            {
                return false;
            }
            return lhs_lola == *rhs_lola;
        },
        [lhs, rhs](const SomeIpServiceInstanceId& lhs_someip) noexcept -> bool {
            const auto* const rhs_someip = std::get_if<SomeIpServiceInstanceId>(&rhs.binding_info_);
            if (rhs_someip == nullptr)
            {
                return false;
            }
            return lhs_someip == *rhs_someip;
        },
        [](const score::cpp::blank&) noexcept -> bool {
            return true;
        });
    return std::visit(visitor, lhs.binding_info_);
}

// See Note 1
// coverity[autosar_cpp14_a15_5_3_violation]
bool operator<(const ServiceInstanceId& lhs, const ServiceInstanceId& rhs) noexcept
{
    auto visitor = score::cpp::overload(
        [&rhs](const LolaServiceInstanceId& lhs_lola) noexcept -> bool {
            const auto* const rhs_lola = std::get_if<LolaServiceInstanceId>(&rhs.binding_info_);
            if (rhs_lola == nullptr)
            {
                return false;
            }
            return lhs_lola < *rhs_lola;
        },
        [lhs, rhs](const SomeIpServiceInstanceId& lhs_someip) noexcept -> bool {
            const auto* const rhs_someip = std::get_if<SomeIpServiceInstanceId>(&rhs.binding_info_);
            if (rhs_someip == nullptr)
            {
                return false;
            }
            return lhs_someip < *rhs_someip;
        },
        // FP: only one statement in this line
        // coverity[autosar_cpp14_a7_1_7_violation]
        [](const score::cpp::blank&) noexcept -> bool {
            return true;
        });
    return std::visit(visitor, lhs.binding_info_);
}

}  // namespace score::mw::com::impl
