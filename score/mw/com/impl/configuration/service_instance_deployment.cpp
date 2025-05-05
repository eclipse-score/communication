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
#include "score/mw/com/impl/configuration/service_instance_deployment.h"

#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"

#include "score/mw/com/impl/configuration/configuration_common_resources.h"

#include "score/json/json_parser.h"

#include <score/overload.hpp>

#include <exception>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace score::mw::com::impl
{

namespace
{

constexpr auto kAsilLevelKey = "asilLevel";
constexpr auto kInstanceSpecifierKey = "instanceSpecifier";
constexpr auto kServiceKey = "service";

// False positive, we first check if json_result has value before we call value() on it. I.e. no throw through
// bad_variant_access can occur.
// coverity[autosar_cpp14_a15_5_3_violation]
QualityType GetQualityTypeFromJson(const score::json::Object& json_object, score::cpp::string_view key) noexcept
{
    const auto it = json_object.find(key);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(it != json_object.end());
    const auto json_result = it->second.As<std::string>();
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(json_result.has_value());
    return FromString(json_result.value());
}

// coverity[autosar_cpp14_a2_10_4_violation] False positive, function is in anonymous namespace
ServiceInstanceDeployment::BindingInformation GetBindingInfoFromJson(const score::json::Object& json_object) noexcept
{
    const auto variant_index = GetValueFromJson<std::ptrdiff_t>(json_object, kBindingInfoIndexKey);

    const auto binding_information = DeserializeVariant<0, ServiceInstanceDeployment::BindingInformation>(
        json_object, variant_index, kBindingInfoKey);

    return binding_information;
}

}  // namespace

auto operator==(const ServiceInstanceDeployment& lhs, const ServiceInstanceDeployment& rhs) noexcept -> bool
{
    return (((lhs.asilLevel_ == rhs.asilLevel_) && (lhs.bindingInfo_ == rhs.bindingInfo_)));
}

auto operator<(const ServiceInstanceDeployment& lhs, const ServiceInstanceDeployment& rhs) noexcept -> bool
{
    bool bindingLess{false};

    const auto* const lhsShmBindingInfo = std::get_if<LolaServiceInstanceDeployment>(&lhs.bindingInfo_);
    const auto* const rhsShmBindingInfo = std::get_if<LolaServiceInstanceDeployment>(&rhs.bindingInfo_);
    if ((lhsShmBindingInfo != nullptr) && (rhsShmBindingInfo != nullptr))
    {
        bindingLess = lhsShmBindingInfo->instance_id_ < rhsShmBindingInfo->instance_id_;
    }
    else
    {
        const auto* const lhsSomeIpBindingInfo = std::get_if<SomeIpServiceInstanceDeployment>(&lhs.bindingInfo_);
        const auto* const rhsSomeIpBindingInfo = std::get_if<SomeIpServiceInstanceDeployment>(&rhs.bindingInfo_);
        if ((lhsSomeIpBindingInfo != nullptr) && (rhsSomeIpBindingInfo != nullptr))
        {
            bindingLess = lhsSomeIpBindingInfo->instance_id_ < rhsSomeIpBindingInfo->instance_id_;
        }
    }
    return (((lhs.asilLevel_ < rhs.asilLevel_) && bindingLess));
}

auto areCompatible(const ServiceInstanceDeployment& lhs, const ServiceInstanceDeployment& rhs) -> bool
{
    bool bindingCompatible{false};
    const auto* const lhsShmBindingInfo = std::get_if<LolaServiceInstanceDeployment>(&lhs.bindingInfo_);
    const auto* const rhsShmBindingInfo = std::get_if<LolaServiceInstanceDeployment>(&rhs.bindingInfo_);
    if ((lhsShmBindingInfo != nullptr) && (rhsShmBindingInfo != nullptr))
    {
        bindingCompatible = areCompatible(*lhsShmBindingInfo, *rhsShmBindingInfo);
    }
    else
    {
        const auto* const lhsSomeIpBindingInfo = std::get_if<SomeIpServiceInstanceDeployment>(&lhs.bindingInfo_);
        const auto* const rhsSomeIpBindingInfo = std::get_if<SomeIpServiceInstanceDeployment>(&rhs.bindingInfo_);
        if ((lhsSomeIpBindingInfo != nullptr) && (rhsSomeIpBindingInfo != nullptr))
        {
            bindingCompatible = areCompatible(*lhsSomeIpBindingInfo, *rhsSomeIpBindingInfo);
        }
    }
    return areCompatible(lhs.asilLevel_, rhs.asilLevel_) && bindingCompatible;
}

// coverity[autosar_cpp14_a15_5_3_violation] False positive, none of the fucntions throw.
ServiceInstanceDeployment::ServiceInstanceDeployment(const score::json::Object& json_object) noexcept
    : ServiceInstanceDeployment(
          ServiceIdentifierType{GetValueFromJson<json::Object>(json_object, kServiceKey)},
          GetBindingInfoFromJson(json_object),
          GetQualityTypeFromJson(json_object, kAsilLevelKey),
          InstanceSpecifier::Create(GetValueFromJson<std::string>(json_object, kInstanceSpecifierKey)).value())
{
    const auto serialization_version = GetValueFromJson<std::uint32_t>(json_object, kSerializationVersionKey);
    if (serialization_version != serializationVersion)
    {
        std::terminate();
    }
}

// Note 1:
// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall not be called
//                                                                   implicitly"
// coverity reports that GetAsStringView might throw since it calls std::visit, which itself
// might throw std::bad_variant_access.
// std::visit will only throw std::bad_variant_access if the variant is empty by exception. Since we do not use
// exceptions, a variant can never be in this state, i.e. std::visit can never throw std::bad_variant_access.
//
// coverity[autosar_cpp14_a15_5_3_violation] Note 1
score::json::Object ServiceInstanceDeployment::Serialize() const noexcept
{
    score::json::Object json_object{};
    json_object[kBindingInfoIndexKey] = score::json::Any{bindingInfo_.index()};

    auto visitor = score::cpp::overload(
        [&json_object](const LolaServiceInstanceDeployment& deployment) {
            json_object[kBindingInfoKey] = deployment.Serialize();
        },
        [&json_object](const SomeIpServiceInstanceDeployment& deployment) {
            json_object[kBindingInfoKey] = deployment.Serialize();
        },
        [](const score::cpp::blank&) noexcept {});
    std::visit(visitor, bindingInfo_);

    json_object[kAsilLevelKey] = json::Any{ToString(asilLevel_)};
    json_object[kServiceKey] = service_.Serialize();

    const auto instance_specifier_view = instance_specifier_.ToString();
    json_object[kInstanceSpecifierKey] =
        json::Any{std::string{instance_specifier_view.data(), instance_specifier_view.size()}};
    json_object[kSerializationVersionKey] = json::Any{serializationVersion};

    return json_object;
}

// coverity[autosar_cpp14_a15_5_3_violation] see Note 1
BindingType ServiceInstanceDeployment::GetBindingType() const noexcept
{
    // FP: only one statement in this line
    // coverity[autosar_cpp14_a7_1_7_violation]
    auto visitor = score::cpp::overload(
        [](const LolaServiceInstanceDeployment&) noexcept {
            return BindingType::kLoLa;
        },
        // FP: only one statement in this line
        // coverity[autosar_cpp14_a7_1_7_violation]
        [](const SomeIpServiceInstanceDeployment&) noexcept {
            return BindingType::kSomeIp;
        },
        // FP: only one statement in this line
        // coverity[autosar_cpp14_a7_1_7_violation]
        [](const score::cpp::blank&) noexcept {
            return BindingType::kFake;
        });
    return std::visit(visitor, bindingInfo_);
}

}  // namespace score::mw::com::impl
