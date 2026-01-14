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
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/configuration/configuration_common_resources.h"

#include "score/mw/log/logging.h"

#include "score/json/json_writer.h"
#include "score/result/result.h"

#include <score/overload.hpp>

#include <exception>
#include <sstream>
#include <tuple>

namespace score::mw::com::impl
{

namespace
{

constexpr auto kServiceInstanceDeploymentKey = "serviceInstanceDeployment";
constexpr auto kServiceTypeDeploymentKey = "serviceTypeDeployment";

}  // namespace

Configuration* InstanceIdentifier::configuration_{nullptr};

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, std::terminate() is implicitly called from 'std::move(json_result).value()'
// in case the json_result doesn't have a value but as we check before with 'has_value()' so no way for throwing
// std::bad_optional_access which leds to std::terminate().
// This suppression should be removed after fixing [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
score::Result<InstanceIdentifier> InstanceIdentifier::Create(std::string&& serialized_format) noexcept
{
    if (configuration_ == nullptr)
    {
        ::score::mw::log::LogFatal("lola") << "InstanceIdentifier configuration pointer hasn't been set. Exiting";
        return MakeUnexpected(ComErrc::kInvalidConfiguration);
    }
    json::JsonParser json_parser{};
    // Suppress Rule A18-9-2 (required, implementation, automated)
    // Forwarding values to other functions shall be done via: (1) std::move if the value is an rvalue reference, (2)
    // std::forward if the value is forwarding reference.
    // Here serialized_format.data() and serialized_format.size() are no rvalue references and thus don´t need to be
    // moved.
    // coverity[autosar_cpp14_a18_9_2_violation : FALSE]
    auto json_result = json_parser.FromBuffer({serialized_format.data(), serialized_format.size()});
    if (!json_result.has_value())
    {
        ::score::mw::log::LogFatal("lola") << "InstanceIdentifier serialized string is invalid. Exiting";
        return MakeUnexpected(ComErrc::kInvalidInstanceIdentifierString);
    }
    const auto& json_object = std::move(json_result).value().As<json::Object>().value().get();
    return InstanceIdentifier{json_object, std::move(serialized_format)};
}

// Suppress "AUTOSAR C++14 A12-1-5" rule finding. This rule states:"Common class initialization for non-constant
// members shall be done by a delegating constructor.".
// Rationale: Delegation to a separate constructor complexifies the code significantly.
// This adds more risk than doing the initialization of the members in each constructor.
// coverity[autosar_cpp14_a12_1_5_violation : FALSE]
InstanceIdentifier::InstanceIdentifier(const json::Object& json_object, std::string&& serialized_string) noexcept
    : instance_deployment_{nullptr}, type_deployment_{nullptr}, serialized_string_{std::move(serialized_string)}
{
    const auto serialization_version = GetValueFromJson<std::uint32_t>(json_object, kSerializationVersionKey);
    if (serialization_version != serializationVersion)
    {
        ::score::mw::log::LogFatal("lola") << "InstanceIdentifier serialization versions don't match. "
                                         << serialization_version << "!=" << serializationVersion << ". Terminating.";
        std::terminate();
    }

    ServiceInstanceDeployment instance_deployment{
        GetValueFromJson<json::Object>(json_object, kServiceInstanceDeploymentKey)};
    ServiceTypeDeployment type_deployment{GetValueFromJson<json::Object>(json_object, kServiceTypeDeploymentKey)};

    auto service_identifier_type = instance_deployment.service_;
    const auto* const type_deployment_ptr =
        configuration_->AddServiceTypeDeployment(std::move(service_identifier_type), std::move(type_deployment));
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(type_deployment_ptr != nullptr,
                           "Could not insert service type deployment into configuration map");
    type_deployment_ = type_deployment_ptr;

    auto instance_specifier = instance_deployment.instance_specifier_;
    const auto* const service_instance_deployment_ptr =
        configuration_->AddServiceInstanceDeployments(std::move(instance_specifier), std::move(instance_deployment));
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(service_instance_deployment_ptr != nullptr,
                           "Could not insert instance deployment into configuration map");
    instance_deployment_ = service_instance_deployment_ptr;
}

// Suppress "AUTOSAR C++14 A12-1-5" rule finding. This rule states:"Common class initialization for non-constant
// members shall be done by a delegating constructor.".
// Rationale: Delegation to a separate constructor complexifies the code significantly.
// This adds more risk than doing the initialization of the members in each constructor.
// coverity[autosar_cpp14_a12_1_5_violation : FALSE]
InstanceIdentifier::InstanceIdentifier(const ServiceInstanceDeployment& deployment,
                                       const ServiceTypeDeployment& type_deployment) noexcept
    : instance_deployment_{&deployment},
      type_deployment_{&type_deployment},
      serialized_string_{ToStringImpl(Serialize())}
{
}

auto InstanceIdentifier::Serialize() const noexcept -> json::Object
{
    json::Object json_object{};
    json_object[kSerializationVersionKey] = score::json::Any{serializationVersion};

    json_object[kServiceInstanceDeploymentKey] = instance_deployment_->Serialize();
    json_object[kServiceTypeDeploymentKey] = type_deployment_->Serialize();

    return json_object;
}

auto InstanceIdentifier::ToString() const noexcept -> std::string_view
{
    return serialized_string_;
}

auto operator==(const InstanceIdentifier& lhs, const InstanceIdentifier& rhs) noexcept -> bool
{
    return (((lhs.instance_deployment_->service_ == rhs.instance_deployment_->service_) &&
             (*lhs.instance_deployment_ == *rhs.instance_deployment_)));
}

auto operator<(const InstanceIdentifier& lhs, const InstanceIdentifier& rhs) noexcept -> bool
{
    return std::tie(lhs.instance_deployment_->service_, *lhs.instance_deployment_) <
           std::tie(rhs.instance_deployment_->service_, *rhs.instance_deployment_);
}

InstanceIdentifierView::InstanceIdentifierView(const InstanceIdentifier& identifier) : identifier_{identifier} {}

// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall
// not be called implicitly.". std::visit Throws std::bad_variant_access if
// as-variant(vars_i).valueless_by_exception() is true for any variant vars_i in vars. The variant may only become
// valueless if an exception is thrown during different stages. Since we don't throw exceptions, it's not possible
// that the variant can return true from valueless_by_exception and therefore not possible that std::visit throws
// an exception.
// This suppression should be removed after fixing [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto InstanceIdentifierView::GetServiceInstanceId() const noexcept -> score::cpp::optional<ServiceInstanceId>
{
    auto visitor = score::cpp::overload(
        [](const LolaServiceInstanceDeployment& deployment) -> score::cpp::optional<ServiceInstanceId> {
            if (!deployment.instance_id_.has_value())
            {
                return {};
            }
            return ServiceInstanceId{*deployment.instance_id_};
        },
        [](const SomeIpServiceInstanceDeployment& deployment) -> score::cpp::optional<ServiceInstanceId> {
            if (!deployment.instance_id_.has_value())
            {
                return {};
            }
            return ServiceInstanceId{*deployment.instance_id_};
        },
        [](const score::cpp::blank&) noexcept -> score::cpp::optional<ServiceInstanceId> {
            return score::cpp::optional<ServiceInstanceId>{};
        });
    return std::visit(visitor, GetServiceInstanceDeployment().bindingInfo_);
}

auto InstanceIdentifierView::GetServiceInstanceDeployment() const noexcept -> const ServiceInstanceDeployment&
{
    return *identifier_.instance_deployment_;
}

auto InstanceIdentifierView::GetServiceTypeDeployment() const -> const ServiceTypeDeployment&
{
    return *identifier_.type_deployment_;
}

auto InstanceIdentifierView::isCompatibleWith(const InstanceIdentifier& rhs) const -> bool
{
    return areCompatible(*identifier_.instance_deployment_, *rhs.instance_deployment_);
}

auto InstanceIdentifierView::isCompatibleWith(const InstanceIdentifierView& rhs) const -> bool
{
    return areCompatible(*identifier_.instance_deployment_, *rhs.identifier_.instance_deployment_);
}

}  // namespace score::mw::com::impl

auto std::hash<score::mw::com::impl::InstanceIdentifier>::operator()(
    const score::mw::com::impl::InstanceIdentifier& instance_identifier) const noexcept -> std::size_t
{
    return std::hash<std::string_view>()(instance_identifier.ToString());
}
