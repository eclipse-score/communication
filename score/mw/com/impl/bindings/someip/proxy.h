/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_PROXY_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_PROXY_H

#include "score/mw/com/impl/bindings/someip/service_instance_endpoint.h"
#include "score/mw/com/impl/proxy_binding.h"

#include <set>
#include <string>
#include <string_view>
#include <utility>

namespace score::mw::com::impl::someip
{

/// \brief Proxy side binding of the SOME/IP technical binding.
///
/// The instance is created by the binding independent ProxyBindingFactoryImpl once the deployment of the service
/// instance selects the SOME/IP binding. It holds the endpoint the proxy talks to and the names of the events that the
/// service interface provides.
class Proxy final : public ProxyBinding
{
  public:
    Proxy(ServiceInstanceEndpoint endpoint, std::set<std::string> provided_event_names) noexcept
        : ProxyBinding{}, endpoint_{std::move(endpoint)}, provided_event_names_{std::move(provided_event_names)}
    {
    }

    ~Proxy() noexcept override = default;

    const ServiceInstanceEndpoint& GetEndpoint() const noexcept
    {
        return endpoint_;
    }

    bool IsEventProvided(const std::string_view event_name) const override
    {
        //TODO: remove string alloc after change to std::set<std::string, std::less<>>
        return provided_event_names_.find(std::string{event_name}) != provided_event_names_.cend();
    }

    Result<void> SetupMethods(const std::size_t) override
    {
        return {};
    }

    void PrepareDeinitialize() override {}
    void FinalizeDeinitialize() override {}

  private:
    ServiceInstanceEndpoint endpoint_;
    std::set<std::string> provided_event_names_;
};

}  // namespace score::mw::com::impl::someip

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_PROXY_H
