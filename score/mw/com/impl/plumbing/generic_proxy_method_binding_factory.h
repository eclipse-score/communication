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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_GENERIC_PROXY_METHOD_BINDING_FACTORY_H
#define SCORE_MW_COM_IMPL_PLUMBING_GENERIC_PROXY_METHOD_BINDING_FACTORY_H

#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/methods/proxy_method_binding.h"
#include "score/mw/com/impl/proxy_binding.h"

#include <memory>
#include <string_view>

namespace score::mw::com::impl
{

/// Non-templated factory for ProxyMethodBindings whose signature is only known at runtime.
/// Looks up the method's sizes in shared memory (published by the skeleton at offer time) and
/// constructs the corresponding binding, e.g. a lola::ProxyMethod, from that information.
class GenericProxyMethodBindingFactory final
{
  public:
    /// Returns nullptr if the parent binding is not a recognised binding, if the method is not in
    /// the deployment, or if the skeleton has not published sizes for it.
    static std::unique_ptr<ProxyMethodBinding> Create(HandleType parent_handle,
                                                      ProxyBinding* parent_binding,
                                                      std::string_view method_name) noexcept;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_GENERIC_PROXY_METHOD_BINDING_FACTORY_H
