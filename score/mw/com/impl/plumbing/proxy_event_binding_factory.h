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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_PROXY_EVENT_BINDING_FACTORY_H
#define SCORE_MW_COM_IMPL_PLUMBING_PROXY_EVENT_BINDING_FACTORY_H

#include "score/mw/com/impl/generic_proxy_event_binding.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/plumbing/i_proxy_event_binding_factory.h"
#include "score/mw/com/impl/proxy_binding.h"
#include "score/mw/com/impl/proxy_event_binding.h"

#include <memory>
#include <string_view>
#include <utility>

namespace score::mw::com::impl
{

/// \brief Class that dispatches to either a real ProxyEventBindingFactoryImpl or a mocked version
/// ProxyEventBindingFactoryMock, if a mock is injected.
class ProxyEventBindingFactory final
{
  public:
    /// \brief See documentation in IProxyEventBindingFactory.
    static Result<std::unique_ptr<ProxyEventBinding>> Create(HandleType parent_handle,
                                                             ProxyBinding& parent_binding,
                                                             std::string_view event_name,
                                                             const ServiceElementType service_element_type)
    {
        return instance().Create(std::move(parent_handle), parent_binding, event_name, service_element_type);
    }

    /// \brief Inject a mock IProxyEventBindingFactory. If a mock is injected, then all calls on
    /// ProxyEventBindingFactory will be dispatched to the mock.
    static void InjectMockBinding(IProxyEventBindingFactory* const mock) noexcept
    {
        mock_ = mock;
    }

  private:
    static IProxyEventBindingFactory& instance() noexcept;
    static IProxyEventBindingFactory* mock_;
};

/// \brief Class that dispatches to either a real GenericProxyEventBindingFactoryImpl or a mocked version
/// GenericProxyEventBindingFactoryMock, if a mock is injected.
class GenericProxyEventBindingFactory final
{
  public:
    /// \brief See documentation in IGenericProxyEventBindingFactory.
    static Result<std::unique_ptr<GenericProxyEventBinding>> Create(HandleType parent_handle,
                                                                    ProxyBinding& parent_binding,
                                                                    std::string_view event_name,
                                                                    const ServiceElementType service_element_type)
    {
        return instance().Create(std::move(parent_handle), parent_binding, event_name, service_element_type);
    }

    /// \brief Inject a mock IGenericProxyEventBindingFactory. If a mock is injected, then all calls on
    /// GenericProxyEventBindingFactory will be dispatched to the mock.
    static void InjectMockBinding(IGenericProxyEventBindingFactory* const mock) noexcept
    {
        mock_ = mock;
    }

  private:
    static IGenericProxyEventBindingFactory& instance() noexcept;
    static IGenericProxyEventBindingFactory* mock_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_PROXY_EVENT_BINDING_FACTORY_H
