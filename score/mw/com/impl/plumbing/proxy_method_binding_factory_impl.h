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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_PROXY_METHOD_BINDING_FACTORY_IMPL_H
#define SCORE_MW_COM_IMPL_PLUMBING_PROXY_METHOD_BINDING_FACTORY_IMPL_H

#include "score/mw/com/impl/configuration/lola_method_instance_deployment.h"
#include "score/mw/com/impl/configuration/service_instance_deployment.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/methods/proxy_method_binding.h"
#include "score/mw/com/impl/plumbing/i_proxy_method_binding_factory.h"
#include "score/mw/com/impl/proxy_binding.h"

#include "score/memory/data_type_size_info.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace score::mw::com::impl
{
namespace detail
{

LolaMethodInstanceDeployment::QueueSize GetQueueSize(const HandleType& parent_handle,
                                                     const std::string& method_name_str,
                                                     MethodType method_type);

bool IsMethodOrFieldEnabled(const LolaServiceInstanceDeployment& lola_service_instance_deployment,
                            const std::string& method_name_str,
                            MethodType method_type);

}  // namespace detail

/// \brief Factory class that dispatches calls to the appropriate binding based on binding information in the
/// deployment configuration.
class ProxyMethodBindingFactoryImpl : public IProxyMethodBindingFactory
{
  public:
    /// Creates instances of the binding specific implementations for a proxy method with a particular data type.
    /// \param handle The handle containing the binding information.
    /// \param method_name The binding unspecific name of the method inside the proxy denoted by handle.
    /// \param in_arg_type_info The size/alignment info of the method's in-args (type-erased), or std::nullopt if the
    /// method has no in-args.
    /// \param return_type_info The size/alignment info of the method's return type (type-erased), or std::nullopt if
    /// the method has a void return type.
    /// \return An instance of ProxyMethodBinding or an error in case binding creation fails.
    Result<std::unique_ptr<ProxyMethodBinding>> Create(
        HandleType parent_handle,
        ProxyBinding& parent_binding,
        const std::string_view method_name,
        MethodType method_type,
        std::optional<memory::DataTypeSizeInfo> in_arg_type_info,
        std::optional<memory::DataTypeSizeInfo> return_type_info) override;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_PROXY_METHOD_BINDING_FACTORY_IMPL_H
