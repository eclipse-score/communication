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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_PROXY_EVENT_BINDING_FACTORY_IMPL_H
#define SCORE_MW_COM_IMPL_PLUMBING_PROXY_EVENT_BINDING_FACTORY_IMPL_H

#include "score/mw/com/impl/bindings/lola/proxy.h"
#include "score/mw/com/impl/bindings/lola/proxy_event.h"
#include "score/mw/com/impl/generic_proxy_event_binding.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/plumbing/binding_factory_error.h"
#include "score/mw/com/impl/plumbing/i_proxy_event_binding_factory.h"
#include "score/mw/com/impl/plumbing/lola_proxy_element_building_blocks.h"
#include "score/mw/com/impl/proxy_binding.h"
#include "score/mw/com/impl/proxy_event_binding.h"
#include "score/mw/com/impl/service_element_type.h"

#include <memory>
#include <string_view>

namespace score::mw::com::impl
{

namespace detail
{
/// \brief Creates a lola::ProxyEvent for the given deployment/handle/binding combination.
///
/// This encapsulates the logic shared by ProxyEventBindingFactoryImpl::Create() and
/// GenericProxyEventBindingFactoryImpl::Create(), which only differ in the concrete `ReturnType` they need
/// (i.e. whether the created lola::ProxyEvent is returned as a std::unique_ptr<ProxyEventBinding> or a
/// std::unique_ptr<lola::ProxyEvent>).
/// \tparam ReturnType A score::Result<std::unique_ptr<T>> where lola::ProxyEvent is convertible to T.
/// \param parent_handle The handle containing the binding information.
/// \param parent_binding The parent proxy binding, which must be a lola::Proxy.
/// \param lola_type_deployment The lola specific service type deployment information.
/// \param event_or_field_name The binding unspecific name of the event/field inside the proxy denoted by handle.
/// \param service_element_type Whether the service element is an event or a field.
/// \return An instance of lola::ProxyEvent (wrapped in ReturnType) or an error in case binding creation fails.
template <typename ReturnType>
ReturnType CreateLolaProxyEvent(const HandleType& parent_handle,
                                ProxyBinding& parent_binding,
                                const LolaServiceTypeDeployment& lola_type_deployment,
                                const std::string_view event_or_field_name,
                                const ServiceElementType service_element_type)
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
    return std::make_unique<lola::ProxyEvent>(*lola_proxy, element_fq_id, event_or_field_name);
}
}  // namespace detail

/// \brief Factory class that dispatches calls to the appropriate binding based on binding information in the
/// deployment configuration.
template <typename SampleType>
class ProxyEventBindingFactoryImpl : public IProxyEventBindingFactory<SampleType>
{
  public:
    /// Creates instances of the binding specific implementations for a proxy event with a particular data type.
    /// \tparam SampleType Type of the data that is exchanges
    /// \param handle The handle containing the binding information.
    /// \param event_name The binding unspecific name of the event inside the proxy denoted by handle.
    /// \return An instance of ProxyEventBinding or an error in case binding creation fails.
    Result<std::unique_ptr<ProxyEventBinding>> Create(HandleType parent_handle,
                                                      ProxyBinding& parent_binding,
                                                      const std::string_view event_name,
                                                      const ServiceElementType service_element_type) noexcept override;
};

/// \brief Factory class that dispatches calls to the appropriate binding based on binding information in the
/// deployment configuration.
class GenericProxyEventBindingFactoryImpl : public IGenericProxyEventBindingFactory
{
  public:
    /// Creates instances of the binding specific implementations for a generic proxy event that has no data type.
    /// \param handle The handle containing the binding information.
    /// \param event_name The binding unspecific name of the event inside the proxy denoted by handle.
    /// \return An instance of ProxyEventBinding or an error in case binding creation fails.
    Result<std::unique_ptr<GenericProxyEventBinding>> Create(HandleType parent_handle,
                                                             ProxyBinding& parent_binding,
                                                             const std::string_view event_name,
                                                             const ServiceElementType service_element_type) override;
};

template <typename SampleType>
inline Result<std::unique_ptr<ProxyEventBinding>> ProxyEventBindingFactoryImpl<SampleType>::Create(
    HandleType parent_handle,
    ProxyBinding& parent_binding,
    const std::string_view event_or_field_name,
    const ServiceElementType service_element_type) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(service_element_type == ServiceElementType::EVENT ||
                                              service_element_type == ServiceElementType::FIELD);

    using ReturnType = Result<std::unique_ptr<ProxyEventBinding>>;
    auto deployment_info_visitor = score::cpp::overload(
        [&parent_handle, &parent_binding, event_or_field_name, service_element_type](
            const LolaServiceTypeDeployment& lola_type_deployment) -> ReturnType {
            return detail::CreateLolaProxyEvent<ReturnType>(
                parent_handle, parent_binding, lola_type_deployment, event_or_field_name, service_element_type);
        },
        [](const score::cpp::blank&) noexcept -> ReturnType {
            return MakeUnexpected(BindingFactoryErrorCode::kUnsupportedBindingType);
        });

    const auto& type_deployment = parent_handle.GetServiceTypeDeployment();
    return std::visit(deployment_info_visitor, type_deployment.binding_info_);
}

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_PROXY_EVENT_BINDING_FACTORY_IMPL_H
