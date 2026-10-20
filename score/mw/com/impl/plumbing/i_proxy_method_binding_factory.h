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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_I_PROXY_METHOD_BINDING_FACTORY_H
#define SCORE_MW_COM_IMPL_PLUMBING_I_PROXY_METHOD_BINDING_FACTORY_H

#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/method_type.h"
#include "score/mw/com/impl/methods/proxy_method_binding.h"
#include "score/mw/com/impl/proxy_binding.h"

#include "score/memory/data_type_size_info.h"
#include "score/result/result.h"

#include <memory>
#include <optional>
#include <string_view>

namespace score::mw::com::impl
{

/// \brief Interface for a factory class that dispatches calls to the appropriate binding based on binding
/// information in the deployment configuration.
class IProxyMethodBindingFactory
{
  public:
    IProxyMethodBindingFactory() noexcept = default;

    virtual ~IProxyMethodBindingFactory() noexcept = default;

    IProxyMethodBindingFactory(IProxyMethodBindingFactory&&) = delete;
    IProxyMethodBindingFactory& operator=(IProxyMethodBindingFactory&&) = delete;
    IProxyMethodBindingFactory(const IProxyMethodBindingFactory&) = delete;
    IProxyMethodBindingFactory& operator=(const IProxyMethodBindingFactory&) = delete;

    /// Creates instances of the binding specific implementations for a proxy method with a particular data type.
    /// \param parent_handle The handle containing the binding information.
    /// \param parent_binding Reference to the parent binding which can be cast to the actual binding type like
    /// lola::Proxy.
    /// \param method_name The binding unspecific name of the method inside the proxy denoted by handle.
    /// \param in_arg_type_info The size/alignment info of the method's in-args (type-erased), or std::nullopt if the
    /// method has no in-args. The caller (which knows the concrete argument types at compile-time) is responsible
    /// for computing this, e.g. via TypeErasedMethodSignatureInfo.
    /// \param return_type_info The size/alignment info of the method's return type (type-erased), or std::nullopt if
    /// the method has a void return type. The caller (which knows the concrete return type at compile-time) is
    /// responsible for computing this, e.g. via TypeErasedMethodSignatureInfo.
    /// \return An instance of ProxyMethodBinding or an error in case binding creation fails.
    virtual auto Create(HandleType parent_handle,
                        ProxyBinding& parent_binding,
                        const std::string_view method_name,
                        MethodType method_type,
                        std::optional<memory::DataTypeSizeInfo> in_arg_type_info,
                        std::optional<memory::DataTypeSizeInfo> return_type_info)
        -> Result<std::unique_ptr<ProxyMethodBinding>> = 0;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_I_PROXY_METHOD_BINDING_FACTORY_H
