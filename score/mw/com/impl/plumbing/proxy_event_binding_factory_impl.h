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

#include "score/mw/com/impl/generic_proxy_event_binding.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/plumbing/binding_factory_error.h"
#include "score/mw/com/impl/plumbing/i_proxy_event_binding_factory.h"
#include "score/mw/com/impl/proxy_binding.h"
#include "score/mw/com/impl/proxy_event_binding.h"
#include "score/mw/com/impl/service_element_type.h"

#include <memory>
#include <string_view>

namespace score::mw::com::impl
{

/// \brief Factory class that dispatches calls to the appropriate binding based on binding information in the
/// deployment configuration.
class ProxyEventBindingFactoryImpl : public IProxyEventBindingFactory
{
  public:
    /// Creates instances of the binding specific implementations for a proxy event with a particular data type.
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

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_PROXY_EVENT_BINDING_FACTORY_IMPL_H
