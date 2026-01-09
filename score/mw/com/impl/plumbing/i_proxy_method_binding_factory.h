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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_I_PROXY_METHOD_BINDING_FACTORY_H
#define SCORE_MW_COM_IMPL_PLUMBING_I_PROXY_METHOD_BINDING_FACTORY_H

#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/methods/proxy_method_binding.h"
#include "score/mw/com/impl/proxy_base.h"

#include <memory>
#include <string_view>

namespace score::mw::com::impl
{

/// \brief Interface for a factory class that dispatches calls to the appropriate binding based on binding
/// information in the deployment configuration.
class IProxyMethodBindingFactory
{
  public:
    IProxyMethodBindingFactory() noexcept = default;

    virtual ~IProxyMethodBindingFactory() noexcept = default;

    IProxyMethodBindingFactory(IProxyMethodBindingFactory&&) = delete;
    IProxyMethodBindingFactory& operator=(IProxyMethodBindingFactory&&) = delete;
    IProxyMethodBindingFactory(const IProxyMethodBindingFactory&) = delete;
    IProxyMethodBindingFactory& operator=(const IProxyMethodBindingFactory&) = delete;

    /// Creates instances of the binding specific implementations for a proxy method with a particular data type.
    /// \param parent_handle The handle containing the binding information.
    /// \param parent_binding Pointer to the parent binding which can be cast to the actual binding pointer like
    /// lola::Proxy.
    /// \param method_name The binding unspecific name of the method inside the proxy denoted by handle.
    /// \return An instance of ProxyMethodBinding or nullptr in case of an error.
    virtual auto Create(HandleType parent_handle,
                        ProxyBinding* parent_binding,
                        const std::string_view method_name) noexcept -> std::unique_ptr<ProxyMethodBinding> = 0;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_I_PROXY_METHOD_BINDING_FACTORY_H
