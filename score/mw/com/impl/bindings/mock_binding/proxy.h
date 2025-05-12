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
    ~Proxy() = default;

    MOCK_METHOD(bool, IsEventProvided, (const score::cpp::string_view), (const, noexcept, override));
    MOCK_METHOD(void, RegisterEventBinding, (score::cpp::string_view, ProxyEventBindingBase&), (noexcept, override));
    MOCK_METHOD(void, UnregisterEventBinding, (score::cpp::string_view), (noexcept, override));
};

}  // namespace score::mw::com::impl::mock_binding

#endif  // SCORE_MW_COM_IMPL_BINDINGS_MOCK_BINDING_PROXY_H
