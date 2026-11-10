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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_PROXY_FIELD_BINDING_FACTORY_H
#define SCORE_MW_COM_IMPL_PLUMBING_PROXY_FIELD_BINDING_FACTORY_H

#include "score/mw/com/impl/plumbing/i_proxy_field_binding_factory.h"
#include "score/mw/com/impl/proxy_binding.h"
#include "score/mw/com/impl/proxy_event_binding.h"

#include "score/memory/data_type_size_info.h"

#include <memory>
#include <string_view>
#include <utility>

namespace score::mw::com::impl
{

/// \brief Class that dispatches to either a real ProxyFieldBindingFactoryImpl or a mocked version
/// ProxyFieldBindingFactoryMock, if a mock is injected.
class ProxyFieldBindingFactory final
{
  public:
    /// \brief See documentation in IProxyFieldBindingFactory.
    static Result<std::unique_ptr<ProxyEventBinding>> CreateEventBinding(
        HandleType parent_handle,
        ProxyBinding& parent_binding,
        std::string_view field_name,
        memory::DataTypeSizeInfo data_type_size_info) noexcept
    {
        return instance().CreateEventBinding(std::move(parent_handle), parent_binding, field_name, data_type_size_info);
    }

    /// \brief See documentation in IProxyFieldBindingFactory.
    static Result<std::unique_ptr<ProxyMethodBinding>> CreateGetMethodBinding(
        HandleType parent_handle,
        ProxyBinding& parent_binding,
        std::string_view field_name,
        memory::DataTypeSizeInfo return_type_info) noexcept
    {
        return instance().CreateGetMethodBinding(
            std::move(parent_handle), parent_binding, field_name, return_type_info);
    }

    /// \brief See documentation in IProxyFieldBindingFactory.
    static Result<std::unique_ptr<ProxyMethodBinding>> CreateSetMethodBinding(
        HandleType parent_handle,
        ProxyBinding& parent_binding,
        std::string_view field_name,
        memory::DataTypeSizeInfo in_arg_type_info,
        memory::DataTypeSizeInfo return_type_info) noexcept
    {
        return instance().CreateSetMethodBinding(
            std::move(parent_handle), parent_binding, field_name, in_arg_type_info, return_type_info);
    }

    /// \brief Inject a mock IProxyFieldBindingFactory. If a mock is injected, then all calls on
    /// ProxyFieldBindingFactory will be dispatched to the mock.
    static void InjectMockBinding(IProxyFieldBindingFactory* const mock) noexcept
    {
        mock_ = mock;
    }

  private:
    static IProxyFieldBindingFactory& instance() noexcept;
    static IProxyFieldBindingFactory* mock_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_PROXY_FIELD_BINDING_FACTORY_H
