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
#ifndef SCORE_MW_COM_IMPL_PROXYBINDING_H
#define SCORE_MW_COM_IMPL_PROXYBINDING_H

#include "score/mw/com/impl/proxy_event_binding_base.h"

#include "score/result/result.h"

#include <string_view>
#include <vector>

namespace score::mw::com::impl
{

/// \brief The ProxyBinding abstracts the interface that _every_ binding needs to provide.
///
/// It will be used by a concrete proxy to perform _any_ operation in a then binding specific manner.
class ProxyBinding
{
  public:
    ProxyBinding() = default;

    // A ProxyBinding is always held via a pointer in the binding independent impl::ProxyBase.
    // Therefore, the binding itself doesn't have to be moveable or copyable, as the pointer can simply be copied when
    // moving the impl::ProxyBase.
    ProxyBinding(const ProxyBinding&) = delete;
    ProxyBinding(ProxyBinding&&) noexcept = delete;
    ProxyBinding& operator=(const ProxyBinding&) & = delete;
    ProxyBinding& operator=(ProxyBinding&&) & noexcept = delete;

    virtual ~ProxyBinding() noexcept;

    /// Checks whether the event corresponding to event_name is provided
    ///
    /// Note. This function is currently only needed in a GenericProxy. However, we currently don't distinguish between
    /// a lola::Proxy and a lola::GenericProxy (the latter doesn't exists). This is because IsEventProvided() is the
    /// only function that is not the same for both classes so we avoid introducing multiple additional classes purely
    /// to remove IsEventProvided from lola::Proxy. Therefore, if we add a lola::GenericProxy in future, we should
    /// create a GenericProxyBinding class, which will contain the pure virtual IsEventProvided() function, and a
    /// ProxyBindingBase class which this class and GenericProxyBinding should both inherit from.
    ///
    /// \param event_name The event name to check.
    /// \return True if the event name exists, otherwise, false
    virtual bool IsEventProvided(const std::string_view event_name) const noexcept = 0;

    /// Registers a ProxyEvent binding with its parent proxy
    virtual void RegisterEventBinding(const std::string_view service_element_name,
                                      ProxyEventBindingBase& proxy_event_binding) noexcept = 0;

    /// Unregisters a ProxyEvent binding with its parent proxy
    virtual void UnregisterEventBinding(const std::string_view service_element_name) noexcept = 0;

    virtual ResultBlank SetupMethods(const std::vector<std::string_view>& enabled_method_names) = 0;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PROXYBINDING_H
