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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_I_PROXY_BINDING_FACTORY_H
#define SCORE_MW_COM_IMPL_PLUMBING_I_PROXY_BINDING_FACTORY_H

#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/proxy_binding.h"

#include "score/result/result.h"

#include <memory>
#include <vector>

namespace score::mw::com::impl
{

/// \brief Interface for a factory class that dispatches calls to the appropriate binding based on binding information
/// in the deployment configuration.
class IProxyBindingFactory
{
  public:
    IProxyBindingFactory() noexcept = default;

    virtual ~IProxyBindingFactory() noexcept = default;

    IProxyBindingFactory(IProxyBindingFactory&&) = delete;
    IProxyBindingFactory& operator=(IProxyBindingFactory&&) = delete;
    IProxyBindingFactory(const IProxyBindingFactory&) = delete;
    IProxyBindingFactory& operator=(const IProxyBindingFactory&) = delete;

    /// \brief Try to create a proxy binding that adheres to the information given in the handle.
    ///
    /// \param handle A handle that describes the desired service instance location that this proxy instance shall
    ///               connect to.
    /// \return An instantiated ProxyBinding, or nullptr in case the instantiation failed for some reason.
    virtual std::unique_ptr<ProxyBinding> Create(const HandleType& handle) noexcept = 0;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_I_PROXY_BINDING_FACTORY_H
