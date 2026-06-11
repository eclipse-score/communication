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

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/proxy_element_lookup.h"
#include "score/mw/com/impl/bindings/lola/proxy_event.h"
#include "score/mw/com/impl/generic_proxy_event_binding.h"
#include "score/mw/com/impl/plumbing/i_proxy_event_binding_factory.h"
#include "score/mw/com/impl/proxy_base.h"
#include "score/mw/com/impl/proxy_event_binding.h"
#include "score/mw/com/impl/service_element_type.h"

#include "score/mw/log/logging.h"

#include <memory>
#include <string_view>

namespace score::mw::com::impl
{

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
    /// \return An instance of ProxyEventBinding or nullptr in case of an error.
    std::unique_ptr<ProxyEventBinding<SampleType>> Create(ProxyBase& parent,
                                                          const std::string_view event_name,
                                                          const ServiceElementType element_type) noexcept override;
};

/// \brief Factory class that dispatches calls to the appropriate binding based on binding information in the
/// deployment configuration.
class GenericProxyEventBindingFactoryImpl : public IGenericProxyEventBindingFactory
{
  public:
    /// Creates instances of the binding specific implementations for a generic proxy event that has no data type.
    /// \param handle The handle containing the binding information.
    /// \param event_name The binding unspecific name of the event inside the proxy denoted by handle.
    /// \return An instance of ProxyEventBinding or nullptr in case of an error.
    std::unique_ptr<GenericProxyEventBinding> Create(ProxyBase& parent,
                                                     const std::string_view event_name,
                                                     const ServiceElementType element_type) noexcept override;
};

template <typename SampleType>
inline std::unique_ptr<ProxyEventBinding<SampleType>> ProxyEventBindingFactoryImpl<SampleType>::Create(
    ProxyBase& parent,
    const std::string_view event_name,
    const ServiceElementType element_type) noexcept
{
    auto* const lola_proxy = lola::GetLolaProxyBinding(ProxyBaseView{parent}.GetBinding());
    if (lola_proxy == nullptr)
    {
        score::mw::log::LogError("lola")
            << "ProxyEvent binding could not be created for event" << event_name
            << "because the parent proxy binding is not a lola binding or the element could not be resolved.";
        return nullptr;
    }
    const auto element_fq_id = lola::GetElementFqId(parent.GetHandle(), event_name, element_type);
    if (!element_fq_id.has_value())
    {
        score::mw::log::LogError("lola")
            << "ProxyEvent binding could not be created for event" << event_name
            << "because the parent proxy binding is not a lola binding or the element could not be resolved.";
        return nullptr;
    }
    return std::make_unique<lola::ProxyEvent<SampleType>>(*lola_proxy, *element_fq_id, event_name);
}

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_PROXY_EVENT_BINDING_FACTORY_IMPL_H
