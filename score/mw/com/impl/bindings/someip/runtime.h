/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_RUNTIME_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_RUNTIME_H

#include "score/mw/com/impl/binding_type.h"
#include "score/mw/com/impl/bindings/someip/service_discovery_client.h"
#include "score/mw/com/impl/i_binding_runtime.h"

namespace score::mw::com::impl::someip
{

/// \brief Binding runtime of the SOME/IP binding.
///
/// \details Mirrors lola::Runtime in role. It is minimal because the SOME/IP binding needs neither a message passing
///          service (its ITransport takes that role) nor a tracing runtime (send tracing is not supported yet). Its
///          purpose is to provide the ServiceDiscoveryClient, so that offering a SOME/IP service instance resolves a
///          binding runtime instead of hitting the "Unsupported binding" assertion in impl::ServiceDiscovery.
class Runtime final : public IBindingRuntime
{
  public:
    Runtime() = default;
    ~Runtime() override = default;

    Runtime(const Runtime&) = delete;
    Runtime(Runtime&&) noexcept = delete;
    Runtime& operator=(const Runtime&) & = delete;
    Runtime& operator=(Runtime&&) & noexcept = delete;

    BindingType GetBindingType() const noexcept override
    {
        return BindingType::kSomeIp;
    }

    IServiceDiscoveryClient& GetServiceDiscoveryClient() & noexcept override
    {
        return service_discovery_client_;
    }

    /// \brief The SOME/IP binding does not support tracing yet.
    tracing::IBindingTracingRuntime* GetTracingRuntime() noexcept override
    {
        return nullptr;
    }

  private:
    ServiceDiscoveryClient service_discovery_client_{};
};

}  // namespace score::mw::com::impl::someip

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_RUNTIME_H
