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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_PROXY_ELEMENT_LOOKUP_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_PROXY_ELEMENT_LOOKUP_H

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/proxy_binding.h"
#include "score/mw/com/impl/service_element_type.h"

#include <optional>
#include <string_view>

namespace score::mw::com::impl::lola
{

class Proxy;

/// \brief Returns the parent binding cast to a lola::Proxy.
/// \return The lola proxy, or nullptr if the binding is null or not a lola binding.
Proxy* GetLolaProxyBinding(ProxyBinding* parent_binding) noexcept;

/// \brief Resolves a named service element against the lola deployment contained in the given handle.
/// \return The fully qualified element id, or std::nullopt for a non-lola (blank) deployment.
/// \note Fatally asserts on a malformed instance id, an invalid element type, or an element name that is not present
///       in the deployment.
std::optional<ElementFqId> GetElementFqId(const HandleType& handle,
                                          std::string_view service_element_name,
                                          ServiceElementType element_type) noexcept;

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_PROXY_ELEMENT_LOOKUP_H
