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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_PROXY_BINDING_FACTORY_H
#define SCORE_MW_COM_IMPL_PLUMBING_PROXY_BINDING_FACTORY_H

#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/plumbing/i_proxy_binding_factory.h"
#include "score/mw/com/impl/proxy_binding.h"

#include "score/result/result.h"

#include <score/optional.hpp>

#include <memory>
#include <vector>

namespace score::mw::com::impl
{

/// \brief Class that dispatches to either a real ProxyBindingFactoryImpl or a mocked version ProxyBindingFactoryMock,
/// if a mock is injected.
class ProxyBindingFactory final
{
  public:
    /// \brief See documentation in IProxyBindingFactory.
    static std::unique_ptr<ProxyBinding> Create(const HandleType& handle) noexcept
    {
        return instance().Create(handle);
    }

    /// \brief Inject a mock IProxyBindingFactory. If a mock is injected, then all calls on ProxyBindingFactory will be
    /// dispatched to the mock.
    static void InjectMockBinding(IProxyBindingFactory* const mock) noexcept
    {
        mock_ = mock;
    }

  private:
    static IProxyBindingFactory& instance() noexcept;
    static IProxyBindingFactory* mock_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_PROXY_BINDING_FACTORY_H
