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
#include "score/mw/com/impl/generic_proxy_event_binding.h"
#include "score/mw/com/impl/plumbing/lola_proxy_element_building_blocks.h"
#include "score/mw/com/impl/service_element_type.h"

#include "score/mw/log/logging.h"

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
std::unique_ptr<GenericProxyEventBinding> GenericProxyEventBindingFactoryImpl::Create(
    ProxyBase& parent,
    const std::string_view event_name) noexcept
{
    const auto lookup = LookupLolaProxyElement(parent, event_name, ServiceElementType::EVENT);
    if (!lookup.has_value())
    {
        score::mw::log::LogError("lola")
            << "GenericProxyEvent binding could not be created for event" << event_name
            << "because the parent proxy binding is not a lola binding or the element could not be resolved.";
        return nullptr;
    }
    return std::make_unique<lola::GenericProxyEvent>(lookup->parent, lookup->element_fq_id, event_name);
}

}  // namespace score::mw::com::impl
