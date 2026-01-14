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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_MOCK_BINDING_PROXY_H
#define SCORE_MW_COM_IMPL_BINDINGS_MOCK_BINDING_PROXY_H

#include "score/mw/com/impl/proxy_binding.h"

#include <gmock/gmock.h>

namespace score::mw::com::impl::mock_binding
{

/// \brief Proxy binding implementation for all mock binding proxies.
class Proxy : public ProxyBinding
{
  public:
    Proxy() = default;
    ~Proxy() override = default;

    MOCK_METHOD(bool, IsEventProvided, (const std::string_view), (const, noexcept, override));
    MOCK_METHOD(void, RegisterEventBinding, (std::string_view, ProxyEventBindingBase&), (noexcept, override));
    MOCK_METHOD(void, UnregisterEventBinding, (std::string_view), (noexcept, override));
    MOCK_METHOD(ResultBlank, SetupMethods, (const std::vector<std::string_view>& enabled_method_names), (override));
};

class ProxyFacade : public ProxyBinding
{
  public:
    ProxyFacade(Proxy& proxy) : ProxyBinding{}, proxy_{proxy} {}
    ~ProxyFacade() override = default;

    bool IsEventProvided(const std::string_view event_name) const noexcept override
    {
        return proxy_.IsEventProvided(event_name);
    }

    void RegisterEventBinding(const std::string_view service_element_name,
                              ProxyEventBindingBase& proxy_event_binding) noexcept override
    {
        proxy_.RegisterEventBinding(service_element_name, proxy_event_binding);
    }

    void UnregisterEventBinding(const std::string_view service_element_name) noexcept override
    {
        proxy_.UnregisterEventBinding(service_element_name);
    }

    ResultBlank SetupMethods(const std::vector<std::string_view>& enabled_method_names) override
    {
        return proxy_.SetupMethods(enabled_method_names);
    }

  private:
    Proxy& proxy_;
};
}  // namespace score::mw::com::impl::mock_binding

#endif  // SCORE_MW_COM_IMPL_BINDINGS_MOCK_BINDING_PROXY_H
