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
#include "score/mw/com/impl/methods/proxy_method_without_in_args_or_return.h"

namespace score::mw::com::impl
{
ProxyMethod<void()>::ProxyMethod(ProxyMethod&& other) noexcept : ProxyMethodBase(std::move(other))
{
    // Since the address of this method has changed, we need update the address stored in the parent proxy.
    ProxyBaseView proxy_base_view{proxy_base_.get()};
    proxy_base_view.UpdateMethod(method_name_, *this);
}

auto ProxyMethod<void()>::operator=(ProxyMethod&& other) noexcept -> ProxyMethod<void()>&
{
    if (this != &other)
    {
        ProxyMethodBase::operator=(std::move(other));

        // Since the address of this method has changed, we need update the address stored in the parent proxy.
        ProxyBaseView proxy_base_view{proxy_base_.get()};
        proxy_base_view.UpdateMethod(method_name_, *this);
    }
    return *this;
}

score::ResultBlank ProxyMethod<void()>::operator()()
{
    auto queue_position = detail::DetermineNextAvailableQueueSlot(is_return_type_ptr_active_);
    if (!queue_position.has_value())
    {
        return Unexpected(queue_position.error());
    }
    auto call_result = binding_->DoCall(queue_position.value());
    if (!call_result.has_value())
    {
        return Unexpected(call_result.error());
    }

    return {};
}

}  // namespace score::mw::com::impl
