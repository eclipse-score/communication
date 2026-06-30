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
#include "score/mw/com/impl/plumbing/proxy_binding_factory_impl.h"

#include "score/mw/com/impl/bindings/lola/proxy.h"
#include "score/mw/com/impl/plumbing/binding_factory_error.h"

#include <score/overload.hpp>

#include <memory>
#include <utility>
#include <variant>

namespace score::mw::com::impl
{

// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall
// not be called implicitly.". std::visit Throws std::bad_variant_access if
// as-variant(vars_i).valueless_by_exception() is true for any variant vars_i in vars. The variant may only become
// valueless if an exception is thrown during different stages. Since we don't throw exceptions, it's not possible
// that the variant can return true from valueless_by_exception and therefore not possible that std::visit throws
// an exception.
// This suppression should be removed after fixing [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
Result<std::unique_ptr<ProxyBinding>> ProxyBindingFactoryImpl::Create(const HandleType& handle) noexcept
{
    using ReturnType = Result<std::unique_ptr<ProxyBinding>>;
    auto visitor = score::cpp::overload(
        // Suppress "AUTOSAR C++14 A7-1-7" rule finding. This rule states: "Each
        // expression statement and identifier declaration shall be placed on a
        // separate line.". Following line statement is fine, this happens due to
        // clang formatting.
        // coverity[autosar_cpp14_a7_1_7_violation]
        [handle](const LolaServiceInstanceDeployment&) -> ReturnType {
            // TODO: Return Result<Proxy> from lola::Proxy::Create() and propagate errors to the caller.
            auto proxy_creation_result = lola::Proxy::Create(handle);
            if (proxy_creation_result == nullptr)
            {
                return MakeUnexpected(BindingFactoryErrorCode::kProxyCreationFailed);
            }
            return std::move(proxy_creation_result);
        },
        // coverity[autosar_cpp14_a7_1_7_violation]
        [](const score::cpp::blank&) noexcept -> ReturnType {
            return MakeUnexpected(BindingFactoryErrorCode::kUnsupportedBindingType);
        });
    return std::visit(visitor, handle.GetServiceInstanceDeployment().bindingInfo_);
}

}  // namespace score::mw::com::impl
