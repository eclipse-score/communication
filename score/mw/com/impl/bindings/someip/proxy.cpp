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
#include "score/mw/com/impl/bindings/someip/proxy.h"

namespace score::mw::com::impl::someip
{

std::unique_ptr<Proxy> Proxy::Create(const HandleType&)
{
    return std::make_unique<Proxy>();
}

bool Proxy::IsEventProvided(const std::string_view) const
{
    return false;
}

Result<void> Proxy::SetupMethods(const std::size_t)
{
    return {};
}

void Proxy::PrepareDeinitialize() {}

void Proxy::FinalizeDeinitialize() {}

}  // namespace score::mw::com::impl::someip