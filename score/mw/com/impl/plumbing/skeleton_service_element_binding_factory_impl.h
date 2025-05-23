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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_SKELETON_SERVICE_ELEMENT_BINDING_FACTORY_IMPL_H
#define SCORE_MW_COM_IMPL_PLUMBING_SKELETON_SERVICE_ELEMENT_BINDING_FACTORY_IMPL_H

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/skeleton.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event_properties.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/someip_service_instance_deployment.h"
#include "score/mw/com/impl/plumbing/service_element_binding_resources.h"
#include "score/mw/com/impl/skeleton_base.h"

#include "score/memory/any_string_view.h"
#include "score/mw/log/logging.h"

#include <score/assert.hpp>
#include <score/blank.hpp>
#include <score/overload.hpp>
#include <score/string_view.hpp>

#include <chrono>
#include <exception>
#include <memory>
#include <string>
#include <thread>
#include <variant>

namespace score::mw::com::impl
{

namespace detail
{

template <lola::ElementType element_type>
// The return type of the template function does not depend on the type of parameters.
// coverity[autosar_cpp14_a8_2_1_violation: FALSE]
lola::SkeletonEventProperties GetSkeletonEventPropertiesFromConfig(
    const LolaServiceInstanceDeployment& lola_service_instance_deployment,
    const std::string_view service_element_name)
{
    const std::string service_element_name_string{service_element_name.data(), service_element_name.size()};
    // coverity[autosar_cpp14_a7_1_8_violation: FALSE]: this is a cpp-14 warning. if constexpr is cpp-17 syntax.
    // coverity[autosar_cpp14_m6_4_1_violation: FALSE]: "if constexpr" is a valid statement since C++17.
    if constexpr (element_type == lola::ElementType::EVENT)
    {
        return lola::SkeletonEventProperties{
            lola_service_instance_deployment.events_.at(service_element_name_string).GetNumberOfSampleSlots().value(),
            lola_service_instance_deployment.events_.at(service_element_name_string).max_subscribers_.value(),
            lola_service_instance_deployment.events_.at(service_element_name_string).enforce_max_samples_.value()};
    }
    // coverity[autosar_cpp14_a7_1_8_violation: FALSE]: this is a cpp-14 warning. if constexpr is cpp-17 syntax.
    // coverity[autosar_cpp14_m6_4_1_violation: FALSE]: "if constexpr" is a valid statement since C++17.
    if constexpr (element_type == lola::ElementType::FIELD)
    {
        return lola::SkeletonEventProperties{
            lola_service_instance_deployment.fields_.at(service_element_name_string).GetNumberOfSampleSlots().value(),
            lola_service_instance_deployment.fields_.at(service_element_name_string).max_subscribers_.value(),
            lola_service_instance_deployment.fields_.at(service_element_name_string).enforce_max_samples_.value()};
    }
    score::mw::log::LogFatal() << "Invalid service element type. Could not create SkeletonEventProperties. Terminating";
    std::terminate();
}

}  // namespace detail

template <typename SkeletonServiceElementBinding, typename SkeletonServiceElement, lola::ElementType element_type>
// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall
// not be called implicitly.". std::visit Throws std::bad_variant_access if
// as-variant(vars_i).valueless_by_exception() is true for any variant vars_i in vars. The variant may only become
// valueless if an exception is thrown during different stages. Since we don't throw exceptions, it's not possible
// that the variant can return true from valueless_by_exception and therefore not possible that std::visit throws
// an exception.
// This suppression should be removed after fixing [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto CreateSkeletonServiceElement(const InstanceIdentifier& identifier,
                                  SkeletonBase& parent,
                                  const score::cpp::string_view service_element_name) noexcept
    -> std::unique_ptr<SkeletonServiceElementBinding>
{
    const InstanceIdentifierView identifier_view{identifier};

    using ReturnType = std::unique_ptr<SkeletonServiceElementBinding>;
    auto visitor = score::cpp::overload(
        [identifier_view, &parent, &service_element_name](
            const LolaServiceTypeDeployment& lola_service_type_deployment) -> ReturnType {
            auto* const lola_parent = dynamic_cast<lola::Skeleton*>(SkeletonBaseView{parent}.GetBinding());
            if (lola_parent == nullptr)
            {
                score::mw::log::LogFatal("lola") << "Skeleton service element could not be created because parent "
                                                  "skeleton binding is a nullptr.";
                return nullptr;
            }

            const auto& service_instance_deployment = identifier_view.GetServiceInstanceDeployment();
            const auto& lola_service_instance_deployment =
                GetLolaServiceInstanceDeploymentFromServiceInstanceDeployment(service_instance_deployment);
            const auto element_fq_id =
                GetElementFqIdFromLolaConfig<element_type>(lola_service_type_deployment,
                                                           lola_service_instance_deployment.instance_id_.value(),
                                                           memory::AnyStringView{service_element_name});
            const auto skeleton_event_properties = detail::GetSkeletonEventPropertiesFromConfig<element_type>(
                lola_service_instance_deployment, memory::AnyStringView{service_element_name});
            return std::make_unique<SkeletonServiceElement>(
                *lola_parent, element_fq_id, service_element_name, skeleton_event_properties);
        },
        [](const SomeIpServiceInstanceDeployment&) noexcept -> ReturnType {
            return nullptr;
        },
        // Suppress "AUTOSAR C++14 A7-1-7" rule finding. This rule states: "Each
        // expression statement and identifier declaration shall be placed on a
        // separate line.". Following line statement is fine, this happens due to
        // clang formatting.
        // coverity[autosar_cpp14_a7_1_7_violation]
        [](const score::cpp::blank&) noexcept -> ReturnType {
            return nullptr;
        });

    return std::visit(visitor, identifier_view.GetServiceTypeDeployment().binding_info_);
}

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_SKELETON_SERVICE_ELEMENT_BINDING_FACTORY_IMPL_H
