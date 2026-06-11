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
#include "score/mw/com/impl/plumbing/proxy_event_binding_factory_impl.h"

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/generic_proxy_event.h"
#include "score/mw/com/impl/bindings/lola/proxy_element_lookup.h"
#include "score/mw/com/impl/generic_proxy_event_binding.h"
#include "score/mw/com/impl/service_element_type.h"

#include "score/mw/log/logging.h"

namespace score::mw::com::impl
{

std::unique_ptr<GenericProxyEventBinding> GenericProxyEventBindingFactoryImpl::Create(
    ProxyBase& parent,
    const std::string_view event_name,
    const ServiceElementType element_type) noexcept
{
    auto* const lola_proxy = lola::GetLolaProxyBinding(ProxyBaseView{parent}.GetBinding());
    if (lola_proxy == nullptr)
    {
        score::mw::log::LogError("lola")
            << "GenericProxyEvent binding could not be created for event" << event_name
            << "because the parent proxy binding is not a lola binding or the element could not be resolved.";
        return nullptr;
    }
    const auto element_fq_id = lola::GetElementFqId(parent.GetHandle(), event_name, element_type);
    if (!element_fq_id.has_value())
    {
        score::mw::log::LogError("lola")
            << "GenericProxyEvent binding could not be created for event" << event_name
            << "because the parent proxy binding is not a lola binding or the element could not be resolved.";
        return nullptr;
    }
    return std::make_unique<lola::GenericProxyEvent>(*lola_proxy, *element_fq_id, event_name);
}

}  // namespace score::mw::com::impl
