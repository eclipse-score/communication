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

#include "score/message_passing/client_factory.h"
#include "score/mw/com/impl/binding_type.h"
#include "score/mw/com/impl/bindings/someip/service_discovery_client.h"
#include "score/mw/com/impl/i_binding_runtime.h"

namespace score::mw::com::impl::someip
{

/// \brief Binding runtime of the SOME/IP binding.
class Runtime final : public IBindingRuntime
{
  public:
    Runtime() : service_discovery_client_{client_factory_} {}
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
        // TODO(someip-tracing): Implement the SOME/IP tracing runtime.
        return nullptr;
    }

  private:
    score::message_passing::ClientFactory client_factory_{};
    ServiceDiscoveryClient service_discovery_client_;
};

}  // namespace score::mw::com::impl::someip

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_RUNTIME_H
