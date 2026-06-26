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
#include "score/mw/com/impl/plumbing/proxy_event_binding_factory_impl.h"

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/generic_proxy_event.h"
#include "score/mw/com/impl/generic_proxy_event_binding.h"
#include "score/mw/com/impl/plumbing/lola_proxy_element_building_blocks.h"
#include "score/mw/com/impl/service_element_type.h"

#include "score/mw/log/logging.h"

namespace score::mw::com::impl
{

// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall
// not be called implicitly.". std::visit Throws std::bad_variant_access if
// as-variant(vars_i).valueless_by_exception() is true for any variant vars_i in vars. The variant may only become
// valueless if an exception is thrown during different stages. Since we don't throw exceptions, it's not possible
// that the variant can return true from valueless_by_exception and therefore not possible that std::visit throws
// an exception.
// This suppression should be removed after fixing [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
std::unique_ptr<GenericProxyEventBinding> GenericProxyEventBindingFactoryImpl::Create(
    ProxyBase& parent,
    const std::string_view event_name,
    const ServiceElementType service_element_type) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(service_element_type == ServiceElementType::EVENT ||
                                              service_element_type == ServiceElementType::FIELD);

    using ReturnType = std::unique_ptr<lola::GenericProxyEvent>;
    auto deployment_info_visitor = score::cpp::overload(
        [&parent, event_name, service_element_type](
            const LolaServiceTypeDeployment& lola_type_deployment) -> ReturnType {
            auto* const lola_proxy = dynamic_cast<lola::Proxy*>(&ProxyBaseView{parent}.GetBinding());
            if (lola_proxy == nullptr)
            {
                score::mw::log::LogError("lola") << "Generic proxy event binding could not be created for" << event_name
                                                 << "because the parent proxy binding is not a lola binding.";
                return nullptr;
            }

            const auto element_fq_id =
                GetElementFqId(parent.GetHandle(), lola_type_deployment, std::string{event_name}, service_element_type);
            return std::make_unique<lola::GenericProxyEvent>(*lola_proxy, element_fq_id, event_name);
        },
        [](const score::cpp::blank&) noexcept -> ReturnType {
            return nullptr;
        });

    const HandleType& handle = parent.GetHandle();
    const auto& type_deployment = handle.GetServiceTypeDeployment();
    return std::visit(deployment_info_visitor, type_deployment.binding_info_);
}

}  // namespace score::mw::com::impl
