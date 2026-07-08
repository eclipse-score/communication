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

#include "score/mw/log/logging.h"

namespace score::mw::com::impl
{
score::Result<void> ProxyMethod<void()>::operator()()
{
    if (binding_ == nullptr)
    {
        score::mw::log::LogError("lola") << "ProxyMethod::operator(): Binding is not initialized for method "
                                         << method_name_;
        return Unexpected(ComErrc::kMethodBindingDisabled);
    }
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
