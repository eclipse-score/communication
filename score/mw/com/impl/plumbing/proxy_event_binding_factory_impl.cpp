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

#include "score/mw/com/impl/bindings/lola/proxy.h"
#include "score/mw/com/impl/bindings/lola/proxy_event.h"
#include "score/mw/com/impl/generic_proxy_event_binding.h"
#include "score/mw/com/impl/plumbing/binding_factory_error.h"
#include "score/mw/com/impl/plumbing/lola_proxy_element_building_blocks.h"
#include "score/mw/com/impl/service_element_type.h"

#include "score/mw/log/logging.h"

#include <score/assert.hpp>
#include <score/overload.hpp>

namespace score::mw::com::impl::detail
{

template <typename ReturnType>
ReturnType CreateLolaProxyEvent(const HandleType& parent_handle,
                                ProxyBinding& parent_binding,
                                const LolaServiceTypeDeployment& lola_type_deployment,
                                const std::string_view event_or_field_name,
                                const ServiceElementType service_element_type,
                                const std::optional<memory::DataTypeSizeInfo> data_type_size_info)
{
    auto* const lola_proxy = dynamic_cast<lola::Proxy*>(&parent_binding);
    if (lola_proxy == nullptr)
    {
        score::mw::log::LogError("lola") << "Proxy event binding could not be created for" << event_or_field_name
                                         << "because the parent proxy binding is not a lola binding.";
        return MakeUnexpected(BindingFactoryErrorCode::kParentBindingIsNotLola);
    }

    const auto element_fq_id =
        GetElementFqId(parent_handle, lola_type_deployment, std::string{event_or_field_name}, service_element_type);

    // In case of GenericProxyEvent the caller doesn't know the event's data type (this is the reason
    // data_type_size_info is an optional). Then we are looking up the DataTypeSizeInfo from the EventMetaInfo that the
    // providing skeleton placed into shared-memory.
    const auto resolved_data_type_size_info = data_type_size_info.has_value()
                                                  ? data_type_size_info.value()
                                                  : lola_proxy->GetEventMetaInfo(element_fq_id).data_type_info_;
    return std::make_unique<lola::ProxyEvent>(
        *lola_proxy, element_fq_id, event_or_field_name, resolved_data_type_size_info);
}

}  // namespace score::mw::com::impl::detail

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
Result<std::unique_ptr<ProxyEventBinding>> ProxyEventBindingFactoryImpl::Create(
    HandleType parent_handle,
    ProxyBinding& parent_binding,
    const std::string_view event_or_field_name,
    const ServiceElementType service_element_type,
    const memory::DataTypeSizeInfo data_type_size_info) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(service_element_type == ServiceElementType::EVENT ||
                                              service_element_type == ServiceElementType::FIELD);

    using ReturnType = Result<std::unique_ptr<ProxyEventBinding>>;
    auto deployment_info_visitor = score::cpp::overload(
        [&parent_handle, &parent_binding, event_or_field_name, service_element_type, data_type_size_info](
            const LolaServiceTypeDeployment& lola_type_deployment) -> ReturnType {
            return detail::CreateLolaProxyEvent<ReturnType>(parent_handle,
                                                            parent_binding,
                                                            lola_type_deployment,
                                                            event_or_field_name,
                                                            service_element_type,
                                                            data_type_size_info);
        },
        // The SOME/IP binding does not support proxy events (yet). It is listed explicitly (instead of being served
        // by the score::cpp::blank arm) because std::visit requires an arm for every variant alternative.
        [](const SomeIpServiceTypeDeployment&) noexcept -> ReturnType {
            // TODO(someip-proxy-event): Create the SOME/IP proxy event binding.
            return MakeUnexpected(BindingFactoryErrorCode::kUnsupportedBindingType);
        },
        [](const score::cpp::blank&) noexcept -> ReturnType {
            return MakeUnexpected(BindingFactoryErrorCode::kUnsupportedBindingType);
        });

    const auto& type_deployment = parent_handle.GetServiceTypeDeployment();
    return std::visit(deployment_info_visitor, type_deployment.binding_info_);
}

// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
Result<std::unique_ptr<GenericProxyEventBinding>> GenericProxyEventBindingFactoryImpl::Create(
    HandleType parent_handle,
    ProxyBinding& parent_binding,
    const std::string_view event_name,
    const ServiceElementType service_element_type)
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD((service_element_type == ServiceElementType::EVENT) ||
                                              (service_element_type == ServiceElementType::FIELD));

    using ReturnType = Result<std::unique_ptr<lola::ProxyEvent>>;
    auto deployment_info_visitor = score::cpp::overload(
        [&parent_handle, &parent_binding, event_name, service_element_type](
            const LolaServiceTypeDeployment& lola_type_deployment) -> ReturnType {
            // A generic proxy event doesn't know its SampleType, so the DataTypeSizeInfo is looked up by
            // CreateLolaProxyEvent() from the EventMetaInfo stored in shared-memory.
            return detail::CreateLolaProxyEvent<ReturnType>(
                parent_handle, parent_binding, lola_type_deployment, event_name, service_element_type, std::nullopt);
        },
        // The SOME/IP binding does not support this service element (yet). It is listed explicitly (instead of
        // being served by the score::cpp::blank arm) because std::visit requires an arm for every variant
        // alternative.
        [](const SomeIpServiceTypeDeployment&) noexcept -> ReturnType {
            // TODO(someip-proxy-event): Create the SOME/IP proxy event binding.
            return MakeUnexpected(BindingFactoryErrorCode::kUnsupportedBindingType);
        },
        [](const score::cpp::blank&) noexcept -> ReturnType {
            return MakeUnexpected(BindingFactoryErrorCode::kUnsupportedBindingType);
        });

    const auto& type_deployment = parent_handle.GetServiceTypeDeployment();
    return std::visit(deployment_info_visitor, type_deployment.binding_info_);
}

}  // namespace score::mw::com::impl
