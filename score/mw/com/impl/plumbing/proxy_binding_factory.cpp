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
#include "score/mw/com/impl/plumbing/proxy_binding_factory.h"

#include "score/mw/com/impl/plumbing/proxy_binding_factory_impl.h"

namespace score::mw::com::impl
{

IProxyBindingFactory* ProxyBindingFactory::mock_{nullptr};

IProxyBindingFactory& ProxyBindingFactory::instance() noexcept
{
    if (mock_ != nullptr)
    {
        return *mock_;
    }

    // Suppress "AUTOSAR C++14 A3-3-2", The rule states: "Static and thread-local objects shall be constant-initialized"
    // It cannot be made const since we will need to call non-const methods from a static instance.
    // coverity[autosar_cpp14_a3_3_2_violation]
    static ProxyBindingFactoryImpl instance{};
    return instance;
}

}  // namespace score::mw::com::impl
