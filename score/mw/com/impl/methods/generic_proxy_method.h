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
#ifndef SCORE_MW_COM_IMPL_METHODS_GENERIC_PROXY_METHOD_H
#define SCORE_MW_COM_IMPL_METHODS_GENERIC_PROXY_METHOD_H

#include "score/mw/com/impl/methods/proxy_method_base.h"
#include "score/mw/com/impl/methods/proxy_method_binding.h"
#include "score/mw/com/impl/proxy_base.h"

#include "score/result/result.h"

#include <score/span.hpp>

#include <cstddef>
#include <memory>
#include <string_view>

namespace score::mw::com::impl
{

/// Type-erased method for a generic proxy.
class GenericProxyMethod final : public ProxyMethodBase
{
  public:
    /// Test-only ctor: injeczt a ready-made binding. nullptr marks the parent proxy invalid.
    GenericProxyMethod(ProxyBase& parent,
                       std::unique_ptr<ProxyMethodBinding> binding,
                       std::string_view method_name) noexcept;

    /// Production ctor: obtains the binding from GenericProxyMethodBindingFactory.
    GenericProxyMethod(ProxyBase& parent, std::string_view method_name) noexcept;

    ~GenericProxyMethod() final = default;

    GenericProxyMethod(const GenericProxyMethod&) = delete;
    GenericProxyMethod& operator=(const GenericProxyMethod&) = delete;

    GenericProxyMethod(GenericProxyMethod&&) noexcept;
    GenericProxyMethod& operator=(GenericProxyMethod&&) noexcept;

    /// Byte buffer for the call's in-args at the given queue position. Must not be called for methods without in-args.
    score::Result<score::cpp::span<std::byte>> AllocateInArgs(std::size_t queue_position);

    /// Byte buffer for the call's return value at the given queue position. Must not be called for void returns.
    score::Result<score::cpp::span<std::byte>> AllocateReturnType(std::size_t queue_position);

    /// Invoke the method. Caller must have written in-args and allocated the return buffer first.
    score::ResultBlank Call(std::size_t queue_position);

    /// No typed objects to default-construct; the user fills the byte buffer directly.
    ResultBlank InitializeInArgsAndReturnValues() override
    {
        return {};
    }
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_METHODS_GENERIC_PROXY_METHOD_H
