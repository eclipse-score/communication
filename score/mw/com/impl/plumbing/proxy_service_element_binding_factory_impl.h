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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_PROXY_SERVICE_ELEMENT_BINDING_FACTORY_IMPL_H
#define SCORE_MW_COM_IMPL_PLUMBING_PROXY_SERVICE_ELEMENT_BINDING_FACTORY_IMPL_H

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/proxy.h"
#include "score/mw/com/impl/configuration/binding_service_type_deployment.h"
#include "score/mw/com/impl/configuration/someip_service_instance_deployment.h"
#include "score/mw/com/impl/proxy_base.h"

#include "score/memory/any_string_view.h"
#include "score/mw/log/logging.h"

#include <score/assert.hpp>
#include <score/blank.hpp>
#include <score/overload.hpp>
#include <score/string_view.hpp>

#include <chrono>
#include <exception>
#include <memory>
#include <thread>
#include <variant>

namespace score::mw::com::impl
{

template <typename ProxyServiceElementBinding, typename ProxyServiceElement, ServiceElementType element_type>
// "AUTOSAR C++14 A15-5-3" triggered by std::bad_variant_access.
// Additionally the variant might be valueless_by_exception, which would also cause a std::bad_variant_access, this
// can only happen if any of the variants throw exception during construction. Since we do not throw exceptions,
// this can not happen, and therefore it is not possible for std::visit to throw an exception.
//
// The return type of the template function does not depend on the type of parameters.
//
//
// "AUTOSAR C++14 A8-2-1" This is not a  safetey issue. The rationall only outlines code style improvements, which would
// not be present here.
//
// coverity[autosar_cpp14_a15_5_3_violation]
// coverity[autosar_cpp14_a8_2_1_violation]
std::unique_ptr<ProxyServiceElementBinding> CreateProxyServiceElement(
    ProxyBase& parent,
    const score::cpp::string_view service_element_name) noexcept
{
    using ReturnType = std::unique_ptr<ProxyServiceElementBinding>;

    const HandleType& handle = parent.GetHandle();
    const auto& type_deployment = handle.GetServiceTypeDeployment();

    auto deployment_info_visitor = score::cpp::overload(
        [&parent, &handle, service_element_name](const LolaServiceTypeDeployment& lola_type_deployment) -> ReturnType {
            auto* const lola_parent = dynamic_cast<lola::Proxy*>(ProxyBaseView{parent}.GetBinding());
            if (lola_parent == nullptr)
            {
                score::mw::log::LogError("lola") << "Proxy service element could not be created because parent proxy "
                                                  "binding is a nullptr.";
                return nullptr;
            }

            const auto instance_id = handle.GetInstanceId();
            const auto* const lola_service_instance_id =
                std::get_if<LolaServiceInstanceId>(&(instance_id.binding_info_));
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(lola_service_instance_id != nullptr,
                                   "ServiceInstanceId does not contain lola binding.");

            const std::string service_element_name_string{service_element_name.data(), service_element_name.size()};
            const auto lola_service_element_id =
                GetServiceElementId<element_type>(lola_type_deployment, service_element_name_string);
            const lola::ElementFqId element_fq_id{lola_type_deployment.service_id_,
                                                  lola_service_element_id,
                                                  lola_service_instance_id->GetId(),
                                                  element_type};
            return std::make_unique<ProxyServiceElement>(*lola_parent, element_fq_id, service_element_name);
        },
        [](const score::cpp::blank&) noexcept -> ReturnType {
            return nullptr;
        },
        [](const SomeIpServiceInstanceDeployment&) noexcept -> ReturnType {
            return nullptr;
        });

    // coverity[autosar_cpp14_a15_5_3_violation]
    return std::visit(deployment_info_visitor, type_deployment.binding_info_);
}

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_PROXY_SERVICE_ELEMENT_BINDING_FACTORY_IMPL_H
