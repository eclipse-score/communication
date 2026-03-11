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
#include "score/mw/com/impl/generic_proxy_event.h"

#include "score/mw/com/impl/plumbing/proxy_event_binding_factory.h"

#include <score/assert.hpp>

namespace score::mw::com::impl
{

GenericProxyEvent::GenericProxyEvent(ProxyBase& base, const std::string_view event_name)
    : ProxyEventBase{base,
                     ProxyBaseView{base}.GetBinding(),
                     GenericProxyEventBindingFactory::Create(base, event_name),
                     event_name}
{
    ProxyBaseView proxy_base_view{base};
    if (!binding_base_)
    {
        proxy_base_view.MarkServiceElementBindingInvalid();
        return;
    }
}

GenericProxyEvent::GenericProxyEvent(ProxyBase& base,
                                     std::unique_ptr<GenericProxyEventBinding> proxy_binding,
                                     const std::string_view event_name)
    : ProxyEventBase{base, ProxyBaseView{base}.GetBinding(), std::move(proxy_binding), event_name}
{
    ProxyBaseView proxy_base_view{base};
    if (!binding_base_)
    {
        proxy_base_view.MarkServiceElementBindingInvalid();
        return;
    }
}

std::size_t GenericProxyEvent::GetSampleSize() const noexcept
{
    auto* const proxy_event_binding = dynamic_cast<GenericProxyEventBinding*>(binding_base_.get());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(proxy_event_binding != nullptr, "Downcast to GenericProxyEventBinding failed!");
    return proxy_event_binding->GetSampleSize();
}

bool GenericProxyEvent::HasSerializedFormat() const noexcept
{
    auto* const proxy_event_binding = dynamic_cast<GenericProxyEventBinding*>(binding_base_.get());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(proxy_event_binding != nullptr, "Downcast to GenericProxyEventBinding failed!");
    return proxy_event_binding->HasSerializedFormat();
}

}  // namespace score::mw::com::impl
