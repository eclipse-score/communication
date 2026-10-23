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
#include "score/mw/com/impl/plumbing/proxy_field_binding_factory_impl.h"

#include "score/mw/com/impl/method_type.h"
#include "score/mw/com/impl/plumbing/proxy_event_binding_factory.h"
#include "score/mw/com/impl/plumbing/proxy_method_binding_factory.h"
#include "score/mw/com/impl/service_element_type.h"

#include <optional>
#include <utility>

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
Result<std::unique_ptr<ProxyEventBinding>> ProxyFieldBindingFactoryImpl::CreateEventBinding(
    HandleType parent_handle,
    ProxyBinding& parent_binding,
    const std::string_view field_name,
    memory::DataTypeSizeInfo data_type_size_info) noexcept
{
    return ProxyEventBindingFactory::Create(
        std::move(parent_handle), parent_binding, field_name, ServiceElementType::FIELD, data_type_size_info);
}

Result<std::unique_ptr<ProxyMethodBinding>> ProxyFieldBindingFactoryImpl::CreateGetMethodBinding(
    HandleType parent_handle,
    ProxyBinding& parent_binding,
    const std::string_view field_name,
    memory::DataTypeSizeInfo return_type_info) noexcept
{
    // A field's get-method never takes an argument (see GetMethodSignature), so there is no in-arg type info to pass.
    return ProxyMethodBindingFactory::Create(
        std::move(parent_handle), parent_binding, field_name, MethodType::kGet, std::nullopt, return_type_info);
}

Result<std::unique_ptr<ProxyMethodBinding>> ProxyFieldBindingFactoryImpl::CreateSetMethodBinding(
    HandleType parent_handle,
    ProxyBinding& parent_binding,
    const std::string_view field_name,
    memory::DataTypeSizeInfo in_arg_type_info,
    memory::DataTypeSizeInfo return_type_info) noexcept
{
    return ProxyMethodBindingFactory::Create(
        std::move(parent_handle), parent_binding, field_name, MethodType::kSet, in_arg_type_info, return_type_info);
}

}  // namespace score::mw::com::impl
