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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_PROXY_METHOD_BINDING_FACTORY_H
#define SCORE_MW_COM_IMPL_PLUMBING_PROXY_METHOD_BINDING_FACTORY_H

#include "score/mw/com/impl/methods/proxy_method_binding.h"
#include "score/mw/com/impl/plumbing/i_proxy_method_binding_factory.h"
#include "score/mw/com/impl/plumbing/proxy_method_binding_factory_impl.h"
#include "score/mw/com/impl/proxy_base.h"

#include <score/overload.hpp>

#include <memory>
#include <string_view>

namespace score::mw::com::impl
{

template <typename Signature>
class ProxyMethodBindingFactory
{
    static_assert(sizeof(Signature) == 0,
                  "ProxyMethodBindingFactory can only be instantiated with a function signature, e.g., "
                  "ProxyMethodBindingFactory<void(int, double)>. "
                  "You tried to use an unsupported signature type.");
};
/// \brief Class that dispatches to either a real ProxyMethodBindingFactoryImpl or a mocked version
/// ProxyMethodBindingFactoryMock, if a mock is injected.
template <typename ReturnType, typename... ArgTypes>
class ProxyMethodBindingFactory<ReturnType(ArgTypes...)> final
{
  public:
    /// \brief See documentation in IProxyMethodBindingFactory.
    static std::unique_ptr<ProxyMethodBinding> Create(HandleType parent_handle,
                                                      ProxyBinding* parent_binding,
                                                      const std::string_view method_name) noexcept
    {
        return instance().Create(parent_handle, parent_binding, method_name);
    }

    /// \brief Inject a mock IProxyMethodBindingFactory. If a mock is injected, then all calls on
    /// ProxyMethodBindingFactory will be dispatched to the mock.
    static void InjectMockBinding(IProxyMethodBindingFactory* const mock) noexcept
    {
        mock_ = mock;
    }

  private:
    static IProxyMethodBindingFactory& instance() noexcept;
    static IProxyMethodBindingFactory* mock_;
};

template <typename ReturnType, typename... ArgTypes>
auto ProxyMethodBindingFactory<ReturnType(ArgTypes...)>::instance() noexcept -> IProxyMethodBindingFactory&
{
    if (mock_ != nullptr)
    {
        return *mock_;
    }

    // Suppress "AUTOSAR C++14 A3-2-2", The rule states: "Static and thread-local objects shall be constant-initialized"
    // It cannot be made const since we will need to call non-const methods from a static instance.
    // coverity[autosar_cpp14_a3_3_2_violation]
    static ProxyMethodBindingFactoryImpl<ReturnType(ArgTypes...)> instance{};
    return instance;
}

template <typename ReturnType, typename... ArgTypes>
IProxyMethodBindingFactory* ProxyMethodBindingFactory<ReturnType(ArgTypes...)>::mock_{nullptr};
}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_PROXY_METHOD_BINDING_FACTORY_H
