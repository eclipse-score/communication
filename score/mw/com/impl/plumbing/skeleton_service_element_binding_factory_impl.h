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
#include "score/mw/com/impl/configuration/binding_service_type_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/service_instance_deployment.h"
#include "score/mw/com/impl/configuration/someip_service_instance_deployment.h"
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

template <typename LolaServiceElementInstanceDeployment>
lola::SkeletonEventProperties GetSkeletonEventProperties(
    const LolaServiceElementInstanceDeployment& lola_service_element_instance_deployment)
{
    if (!lola_service_element_instance_deployment.GetNumberOfSampleSlots().has_value())
    {
        score::mw::log::LogFatal("lola")
            << "Could not create SkeletonEventProperties from ServiceElementInstanceDeployment. Number of sample slots "
               "was not specified in the configuration. Terminating.";
        std::terminate();
    }

    if (!lola_service_element_instance_deployment.max_subscribers_.has_value())
    {
        score::mw::log::LogFatal("lola")
            << "Could not create SkeletonEventProperties from ServiceElementInstanceDeployment. Max subscribers was "
               "not specified in the configuration. Terminating.";
        std::terminate();
    }
    return lola::SkeletonEventProperties{lola_service_element_instance_deployment.GetNumberOfSampleSlots().value(),
                                         lola_service_element_instance_deployment.max_subscribers_.value(),
                                         lola_service_element_instance_deployment.enforce_max_samples_};
}

}  // namespace detail

template <typename SkeletonServiceElementBinding, typename SkeletonServiceElement, ServiceElementType element_type>
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
                GetServiceInstanceDeploymentBinding<LolaServiceInstanceDeployment>(service_instance_deployment);

            const std::string service_element_name_string{service_element_name.data(), service_element_name.size()};
            const auto& lola_service_element_instance_deployment = GetServiceElementInstanceDeployment<element_type>(
                lola_service_instance_deployment, service_element_name_string);
            const auto skeleton_event_properties =
                detail::GetSkeletonEventProperties(lola_service_element_instance_deployment);

            const auto lola_service_element_id =
                GetServiceElementId<element_type>(lola_service_type_deployment, service_element_name_string);
            const lola::ElementFqId element_fq_id{lola_service_type_deployment.service_id_,
                                                  lola_service_element_id,
                                                  lola_service_instance_deployment.instance_id_.value().GetId(),
                                                  element_type};

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
