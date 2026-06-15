/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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
#include "score/mw/com/impl/bindings/lola/proxy_element_lookup.h"

#include "score/mw/com/impl/bindings/lola/proxy.h"
#include "score/mw/com/impl/configuration/binding_service_type_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"
#include "score/mw/log/logging.h"

#include <score/assert.hpp>
#include <score/blank.hpp>
#include <score/overload.hpp>

#include <string>
#include <variant>

namespace score::mw::com::impl::lola
{

Proxy* GetLolaProxyBinding(ProxyBinding* const parent_binding) noexcept
{
    return dynamic_cast<Proxy*>(parent_binding);
}

namespace
{

std::optional<ElementFqId> GetElementFqIdForLola(const HandleType& handle,
                                                 const LolaServiceTypeDeployment& lola_type_deployment,
                                                 const std::string_view name,
                                                 const ServiceElementType element_type) noexcept
{
    const auto instance_id = handle.GetInstanceId();
    const auto* const lola_instance_id = std::get_if<LolaServiceInstanceId>(&(instance_id.binding_info_));
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(lola_instance_id != nullptr,
                                                "ServiceInstanceId does not contain lola binding.");

    const std::string name_str{name};
    LolaServiceElementId element_id{};
    switch (element_type)
    {
        case ServiceElementType::EVENT:
            element_id = GetServiceElementId<ServiceElementType::EVENT>(lola_type_deployment, name_str);
            break;
        case ServiceElementType::FIELD:
            element_id = GetServiceElementId<ServiceElementType::FIELD>(lola_type_deployment, name_str);
            break;
        case ServiceElementType::METHOD:
            element_id = GetServiceElementId<ServiceElementType::METHOD>(lola_type_deployment, name_str);
            break;
        case ServiceElementType::INVALID:
        default:
            score::mw::log::LogFatal("lola") << "LookupLolaProxyElement called with invalid ServiceElementType.";
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
                false, "LookupLolaProxyElement called with invalid ServiceElementType.");
            return std::nullopt;
    }

    return ElementFqId{lola_type_deployment.service_id_, element_id, lola_instance_id->GetId(), element_type};
}

}  // namespace

// Suppress "AUTOSAR C++14 A15-5-3" rule finding. std::visit may throw std::bad_variant_access only
// if the variant is valueless_by_exception, which cannot happen here because we do not throw
// exceptions during construction of the alternatives.
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
std::optional<ElementFqId> GetElementFqId(const HandleType& handle,
                                          const std::string_view service_element_name,
                                          const ServiceElementType element_type) noexcept
{
    const auto& type_deployment = handle.GetServiceTypeDeployment();

    auto visitor = score::cpp::overload(
        [&handle, service_element_name, element_type](const LolaServiceTypeDeployment& lola_deployment) {
            return GetElementFqIdForLola(handle, lola_deployment, service_element_name, element_type);
        },
        [](const score::cpp::blank&) noexcept -> std::optional<ElementFqId> {
            return std::nullopt;
        });

    return std::visit(visitor, type_deployment.binding_info_);
}

}  // namespace score::mw::com::impl::lola
