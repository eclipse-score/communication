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
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/instance_identifier.h"

#include "score/mw/log/logging.h"

#include <score/overload.hpp>

#include <exception>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

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
std::unique_ptr<ProxyBinding> ProxyBindingFactoryImpl::Create(const HandleType& handle) noexcept
{
    using ReturnType = std::unique_ptr<ProxyBinding>;
    auto visitor = score::cpp::overload(
        // Suppress "AUTOSAR C++14 A7-1-7" rule finding. This rule states: "Each
        // expression statement and identifier declaration shall be placed on a
        // separate line.". Following line statement is fine, this happens due to
        // clang formatting.
        // coverity[autosar_cpp14_a7_1_7_violation]
        [handle](const LolaServiceInstanceDeployment&) -> ReturnType {
            return lola::Proxy::Create(handle);
        },
        // coverity[autosar_cpp14_a7_1_7_violation]
        [](const SomeIpServiceInstanceDeployment&) noexcept -> ReturnType {
            return nullptr;
        },
        // coverity[autosar_cpp14_a7_1_7_violation]
        [](const score::cpp::blank&) noexcept -> ReturnType {
            return nullptr;
        });
    return std::visit(visitor, handle.GetServiceInstanceDeployment().bindingInfo_);
}

}  // namespace score::mw::com::impl
