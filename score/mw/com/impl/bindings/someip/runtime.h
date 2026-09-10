/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_RUNTIME_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_RUNTIME_H

#include "score/mw/com/impl/i_binding_runtime.h"

namespace score::mw::com::impl::someip
{

class Runtime final : public IBindingRuntime
{
  public:
    BindingType GetBindingType() const noexcept override { return BindingType::kSomeIp; }
    IServiceDiscoveryClient& GetServiceDiscoveryClient() & noexcept override;
    tracing::IBindingTracingRuntime* GetTracingRuntime() noexcept override { return nullptr; }
};

}  // namespace score::mw::com::impl::someip

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_RUNTIME_H
