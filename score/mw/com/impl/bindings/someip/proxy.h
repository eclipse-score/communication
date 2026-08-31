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

#include "score/mw/com/impl/proxy_binding.h"

namespace score::mw::com::impl::someip
{

class Proxy final : public ProxyBinding
{
  public:
    Proxy() = default;
    ~Proxy() noexcept override = default;

    bool IsEventProvided(const std::string_view) const override
    {
        return false;
    }

    Result<void> SetupMethods(const std::size_t) override
    {
        return {};
    }

    void PrepareDeinitialize() override {}
    void FinalizeDeinitialize() override {}
};

}  // namespace score::mw::com::impl::someip

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_PROXY_H
