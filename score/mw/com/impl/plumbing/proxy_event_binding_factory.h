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
#include "score/mw/com/impl/plumbing/i_proxy_event_binding_factory.h"
#include "score/mw/com/impl/plumbing/proxy_event_binding_factory_impl.h"
#include "score/mw/com/impl/proxy_base.h"
#include "score/mw/com/impl/proxy_event_binding.h"

#include <score/overload.hpp>

#include <memory>
#include <string_view>

namespace score::mw::com::impl
{

/// \brief Class that dispatches to either a real ProxyEventBindingFactoryImpl or a mocked version
/// ProxyEventBindingFactoryMock, if a mock is injected.
template <typename SampleType>
class ProxyEventBindingFactory final
{
  public:
    /// \brief See documentation in IProxyEventBindingFactory.
    static std::unique_ptr<ProxyEventBinding<SampleType>> Create(ProxyBase& parent, std::string_view event_name)
    {
        return instance().Create(parent, event_name);
    }

    /// \brief Inject a mock IProxyEventBindingFactory. If a mock is injected, then all calls on
    /// ProxyEventBindingFactory will be dispatched to the mock.
    static void InjectMockBinding(IProxyEventBindingFactory<SampleType>* const mock) noexcept
    {
        mock_ = mock;
    }

  private:
    static IProxyEventBindingFactory<SampleType>& instance() noexcept;
    static IProxyEventBindingFactory<SampleType>* mock_;
};

/// \brief Class that dispatches to either a real GenericProxyEventBindingFactoryImpl or a mocked version
/// GenericProxyEventBindingFactoryMock, if a mock is injected.
class GenericProxyEventBindingFactory final
{
  public:
    /// \brief See documentation in IGenericProxyEventBindingFactory.
    static std::unique_ptr<GenericProxyEventBinding> Create(ProxyBase& parent, std::string_view event_name)
    {
        return instance().Create(parent, event_name);
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

template <typename SampleType>
auto ProxyEventBindingFactory<SampleType>::instance() noexcept -> IProxyEventBindingFactory<SampleType>&
{
    if (mock_ != nullptr)
    {
        return *mock_;
    }

    // Suppress "AUTOSAR C++14 A3-2-2", The rule states: "Static and thread-local objects shall be constant-initialized"
    // It cannot be made const since we will need to call non-const methods from a static instance.
    // coverity[autosar_cpp14_a3_3_2_violation]
    static ProxyEventBindingFactoryImpl<SampleType> instance{};
    return instance;
}

// Suppress "AUTOSAR_C++14_A3-1-1" rule finding. This rule states:" It shall be possible to include any header file
// in multiple translation units without violating the One Definition Rule".
template <typename SampleType>
// coverity[autosar_cpp14_a3_1_1_violation] This is false-positive, "mock_" is defined once
IProxyEventBindingFactory<SampleType>* ProxyEventBindingFactory<SampleType>::mock_{nullptr};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_PROXY_EVENT_BINDING_FACTORY_H
