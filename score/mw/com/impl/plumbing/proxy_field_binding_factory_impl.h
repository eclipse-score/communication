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
#include "score/mw/com/impl/plumbing/proxy_event_binding_factory.h"
#include "score/mw/com/impl/plumbing/proxy_method_binding_factory.h"
#include "score/mw/com/impl/proxy_binding.h"
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
    /// \return An instance of ProxyEventBinding or an error in case binding creation fails.
    Result<std::unique_ptr<ProxyEventBinding<SampleType>>> CreateEventBinding(
        HandleType parent_handle,
        ProxyBinding* parent_binding,
        const std::string_view field_name) noexcept override;

    Result<std::unique_ptr<ProxyMethodBinding>> CreateGetMethodBinding(
        HandleType parent_handle,
        ProxyBinding* parent_binding,
        const std::string_view field_name) noexcept override;

    Result<std::unique_ptr<ProxyMethodBinding>> CreateSetMethodBinding(
        HandleType parent_handle,
        ProxyBinding* parent_binding,
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
inline Result<std::unique_ptr<ProxyEventBinding<SampleType>>>
ProxyFieldBindingFactoryImpl<SampleType>::CreateEventBinding(HandleType parent_handle,
                                                             ProxyBinding* parent_binding,
                                                             const std::string_view field_name) noexcept
{
    return ProxyEventBindingFactory<SampleType>::Create(
        std::move(parent_handle), parent_binding, field_name, ServiceElementType::FIELD);
}

template <typename SampleType>
inline Result<std::unique_ptr<ProxyMethodBinding>> ProxyFieldBindingFactoryImpl<SampleType>::CreateGetMethodBinding(
    HandleType parent_handle,
    ProxyBinding* parent_binding,
    const std::string_view field_name) noexcept
{
    return ProxyMethodBindingFactory<SampleType()>::Create(
        std::move(parent_handle), parent_binding, field_name, MethodType::kGet);
}

template <typename SampleType>
inline Result<std::unique_ptr<ProxyMethodBinding>> ProxyFieldBindingFactoryImpl<SampleType>::CreateSetMethodBinding(
    HandleType parent_handle,
    ProxyBinding* parent_binding,
    const std::string_view field_name) noexcept
{
    return ProxyMethodBindingFactory<SampleType(SampleType)>::Create(
        std::move(parent_handle), parent_binding, field_name, MethodType::kSet);
}

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_PROXY_FIELD_BINDING_FACTORY_IMPL_H
