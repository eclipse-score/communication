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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_PROXY_FIELD_BINDING_FACTORY_IMPL_H
#define SCORE_MW_COM_IMPL_PLUMBING_PROXY_FIELD_BINDING_FACTORY_IMPL_H

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/proxy_event.h"
#include "score/mw/com/impl/method_type.h"
#include "score/mw/com/impl/plumbing/i_proxy_field_binding_factory.h"
#include "score/mw/com/impl/plumbing/lola_proxy_element_building_blocks.h"
#include "score/mw/com/impl/plumbing/proxy_method_binding_factory.h"
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
class ProxyFieldBindingFactoryImpl final : public IProxyFieldBindingFactory<SampleType>
{
  public:
    /// Creates instances of the event binding of a proxy field with a particular data type.
    /// \tparam SampleType Type of the data that is exchanges
    /// \param handle The handle containing the binding information.
    /// \param field_name The binding unspecific name of the event inside the proxy denoted by handle.
    /// \return An instance of ProxyEventBinding or nullptr in case of an error.
    std::unique_ptr<ProxyEventBinding<SampleType>> CreateEventBinding(
        ProxyBase& parent,
        const std::string_view field_name) noexcept override;

    std::unique_ptr<ProxyMethodBinding> CreateGetMethodBinding(ProxyBase& parent,
                                                               const std::string_view field_name) noexcept override;

    std::unique_ptr<ProxyMethodBinding> CreateSetMethodBinding(ProxyBase& parent,
                                                               const std::string_view field_name) noexcept override;
};

template <typename SampleType>
// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall
// not be called implicitly.". std::visit Throws std::bad_variant_access if
// as-variant(vars_i).valueless_by_exception() is true for any variant vars_i in vars. The variant may only become
// valueless if an exception is thrown during different stages. Since we don't throw exceptions, it's not possible
// that the variant can return true from valueless_by_exception and therefore not possible that std::visit throws
// an exception.
// This suppression should be removed after fixing [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
inline std::unique_ptr<ProxyEventBinding<SampleType>> ProxyFieldBindingFactoryImpl<SampleType>::CreateEventBinding(
    ProxyBase& parent,
    const std::string_view field_name) noexcept
{
    const auto lookup = LookupLolaProxyElement(parent, field_name, ServiceElementType::FIELD);
    if (!lookup.has_value())
    {
        score::mw::log::LogError("lola")
            << "ProxyField event binding could not be created for field" << field_name
            << "because the parent proxy binding is not a lola binding or the element could not be resolved.";
        return nullptr;
    }
    return std::make_unique<lola::ProxyEvent<SampleType>>(lookup->parent, lookup->element_fq_id, field_name);
}

template <typename SampleType>
inline std::unique_ptr<ProxyMethodBinding> ProxyFieldBindingFactoryImpl<SampleType>::CreateGetMethodBinding(
    ProxyBase& parent,
    const std::string_view field_name) noexcept
{
    return ProxyMethodBindingFactory<SampleType()>::Create(
        parent.GetHandle(), ProxyBaseView{parent}.GetBinding(), field_name, MethodType::kGet);
}

template <typename SampleType>
inline std::unique_ptr<ProxyMethodBinding> ProxyFieldBindingFactoryImpl<SampleType>::CreateSetMethodBinding(
    ProxyBase& parent,
    const std::string_view field_name) noexcept
{
    return ProxyMethodBindingFactory<SampleType(SampleType)>::Create(
        parent.GetHandle(), ProxyBaseView{parent}.GetBinding(), field_name, MethodType::kSet);
}

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_PROXY_FIELD_BINDING_FACTORY_IMPL_H
