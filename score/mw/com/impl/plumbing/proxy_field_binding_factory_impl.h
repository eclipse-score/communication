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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_PROXY_FIELD_BINDING_FACTORY_IMPL_H
#define SCORE_MW_COM_IMPL_PLUMBING_PROXY_FIELD_BINDING_FACTORY_IMPL_H

#include "score/mw/com/impl/plumbing/i_proxy_field_binding_factory.h"
#include "score/mw/com/impl/proxy_binding.h"
#include "score/mw/com/impl/proxy_event_binding.h"

#include "score/memory/data_type_size_info.h"

#include <memory>
#include <string_view>

namespace score::mw::com::impl
{

/// \brief Factory class that dispatches calls to the appropriate binding based on binding information in the
/// deployment configuration.
class ProxyFieldBindingFactoryImpl final : public IProxyFieldBindingFactory
{
  public:
    /// Creates instances of the event binding of a proxy field.
    /// \param handle The handle containing the binding information.
    /// \param field_name The binding unspecific name of the event inside the proxy denoted by handle.
    /// \param data_type_size_info Size/alignment info of the field's data type.
    /// \return An instance of ProxyEventBinding or an error in case binding creation fails.
    Result<std::unique_ptr<ProxyEventBinding>> CreateEventBinding(
        HandleType parent_handle,
        ProxyBinding& parent_binding,
        const std::string_view field_name,
        memory::DataTypeSizeInfo data_type_size_info) noexcept override;

    Result<std::unique_ptr<ProxyMethodBinding>> CreateGetMethodBinding(
        HandleType parent_handle,
        ProxyBinding& parent_binding,
        const std::string_view field_name,
        memory::DataTypeSizeInfo return_type_info) noexcept override;

    Result<std::unique_ptr<ProxyMethodBinding>> CreateSetMethodBinding(
        HandleType parent_handle,
        ProxyBinding& parent_binding,
        const std::string_view field_name,
        memory::DataTypeSizeInfo in_arg_type_info,
        memory::DataTypeSizeInfo return_type_info) noexcept override;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_PROXY_FIELD_BINDING_FACTORY_IMPL_H
