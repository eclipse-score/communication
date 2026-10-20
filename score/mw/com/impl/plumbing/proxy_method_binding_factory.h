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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_PROXY_METHOD_BINDING_FACTORY_H
#define SCORE_MW_COM_IMPL_PLUMBING_PROXY_METHOD_BINDING_FACTORY_H

#include "score/mw/com/impl/methods/proxy_method_binding.h"
#include "score/mw/com/impl/plumbing/i_proxy_method_binding_factory.h"
#include "score/mw/com/impl/proxy_base.h"

#include "score/memory/data_type_size_info.h"

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace score::mw::com::impl
{

/// \brief Class that dispatches to either a real ProxyMethodBindingFactoryImpl or a mocked version
/// ProxyMethodBindingFactoryMock, if a mock is injected.
class ProxyMethodBindingFactory final
{
  public:
    /// \brief See documentation in IProxyMethodBindingFactory.
    static Result<std::unique_ptr<ProxyMethodBinding>> Create(
        HandleType parent_handle,
        ProxyBinding& parent_binding,
        const std::string_view method_name,
        MethodType method_type,
        std::optional<memory::DataTypeSizeInfo> in_arg_type_info,
        std::optional<memory::DataTypeSizeInfo> return_type_info) noexcept
    {
        return instance().Create(
            std::move(parent_handle), parent_binding, method_name, method_type, in_arg_type_info, return_type_info);
    }

    /// \brief Inject a mock IProxyMethodBindingFactory. If a mock is injected, then all calls on
    /// ProxyMethodBindingFactory will be dispatched to the mock.
    static void InjectMockBinding(IProxyMethodBindingFactory* const mock) noexcept
    {
        mock_ = mock;
    }

  private:
    static IProxyMethodBindingFactory& instance() noexcept;
    static IProxyMethodBindingFactory* mock_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_PROXY_METHOD_BINDING_FACTORY_H
