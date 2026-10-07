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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_PROXY_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_PROXY_H

#include "score/mw/com/impl/proxy_binding.h"

#include <cstddef>
#include <memory>
#include <string_view>

namespace score::mw::com::impl
{

class HandleType;

namespace someip
{

/// \brief Proxy binding implementation for the SOME/IP binding.
class Proxy final : public ProxyBinding
{
  public:
    static std::unique_ptr<Proxy> Create(const HandleType& handle);

    ~Proxy() noexcept override = default;

    bool IsEventProvided(std::string_view event_name) const override;
    Result<void> SetupMethods(std::size_t additional_shm_size_bytes) override;
    void PrepareDeinitialize() override;
    void FinalizeDeinitialize() override;
};

}  // namespace someip
}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_PROXY_H