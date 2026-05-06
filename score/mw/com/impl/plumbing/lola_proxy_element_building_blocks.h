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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_LOLA_PROXY_ELEMENT_BUILDING_BLOCKS_H
#define SCORE_MW_COM_IMPL_PLUMBING_LOLA_PROXY_ELEMENT_BUILDING_BLOCKS_H

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/proxy.h"
#include "score/mw/com/impl/configuration/lola_service_element_id.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/proxy_base.h"
#include "score/mw/com/impl/proxy_binding.h"
#include "score/mw/com/impl/service_element_type.h"

#include <optional>
#include <string_view>

namespace score::mw::com::impl
{

/// \brief Result of resolving a named service element against the lola deployment on the proxy side.
/// \details Carries everything a binding constructor needs that comes from deployment/handle lookup.
///          This is intentionally agnostic to what kind of binding is being built (event, field notifier,
///          method); the construction step lives with the caller.
struct LoLaProxyElementBuildingBlocks
{
    lola::Proxy& parent;
    LolaServiceInstanceId::InstanceId instance_id;
    lola::ElementFqId element_fq_id;
};

/// \brief Validate the parent is a lola proxy and resolve the element name against the lola deployment contained in the
/// given handle.
/// \return The lookup data on success, std::nullopt for non-lola bindings or a missing parent binding.
/// \note Fatally asserts on a malformed instance id or an element name that is not present in the
///       deployment, matching the previous behaviour of the per-factory inlined lookups.
std::optional<LoLaProxyElementBuildingBlocks> LookupLolaProxyElement(const HandleType& handle,
                                                                     ProxyBinding* parent_binding,
                                                                     std::string_view service_element_name,
                                                                     ServiceElementType element_type) noexcept;

/// \brief Convenience overload that pulls the handle and binding out of a ProxyBase reference.
inline std::optional<LoLaProxyElementBuildingBlocks> LookupLolaProxyElement(ProxyBase& parent,
                                                                            std::string_view service_element_name,
                                                                            ServiceElementType element_type) noexcept
{
    return LookupLolaProxyElement(
        parent.GetHandle(), ProxyBaseView{parent}.GetBinding(), service_element_name, element_type);
}

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_LOLA_PROXY_ELEMENT_BUILDING_BLOCKS_H
