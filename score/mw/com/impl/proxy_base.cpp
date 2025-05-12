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
#include "score/mw/com/impl/proxy_base.h"

#include "score/mw/com/impl/plumbing/proxy_binding_factory.h"
#include "score/mw/com/impl/runtime.h"

#include <score/assert.hpp>

#include <cstdlib>
#include <utility>

namespace score::mw::com::impl
{

ProxyBase::ProxyBase(std::unique_ptr<ProxyBinding> proxy_binding, HandleType handle)
    : proxy_binding_{std::move(proxy_binding)}, handle_{std::move(handle)}, are_service_element_bindings_valid_{true}
{
}

const HandleType& ProxyBase::GetHandle() const noexcept
{
    return handle_;
}

auto ProxyBase::FindService(InstanceSpecifier specifier) noexcept -> Result<ServiceHandleContainer<HandleType>>
{
    const auto find_service_result = Runtime::getInstance().GetServiceDiscovery().FindService(std::move(specifier));
    if (!find_service_result.has_value())
    {
        return MakeUnexpected(ComErrc::kBindingFailure);
    }
    return find_service_result;
}

auto ProxyBase::FindService(InstanceIdentifier instance_identifier) noexcept
    -> Result<ServiceHandleContainer<HandleType>>
{
    const auto find_service_result =
        Runtime::getInstance().GetServiceDiscovery().FindService(std::move(instance_identifier));

    if (!find_service_result.has_value())
    {
        return MakeUnexpected(ComErrc::kBindingFailure);
    }
    return find_service_result;
}

auto ProxyBase::StartFindService(FindServiceHandler<HandleType> handler,
                                 InstanceIdentifier instance_identifier) noexcept -> Result<FindServiceHandle>
{
    const auto start_find_service_result = Runtime::getInstance().GetServiceDiscovery().StartFindService(
        std::move(handler), std::move(instance_identifier));
    if (!(start_find_service_result.has_value()))
    {
        return MakeUnexpected(ComErrc::kFindServiceHandlerFailure,
                              start_find_service_result.error().UserMessage().data());
    }
    return start_find_service_result;
}

auto ProxyBase::StartFindService(FindServiceHandler<HandleType> handler, InstanceSpecifier instance_specifier) noexcept
    -> Result<FindServiceHandle>
{
    const auto start_find_service_result = Runtime::getInstance().GetServiceDiscovery().StartFindService(
        std::move(handler), std::move(instance_specifier));
    if (!(start_find_service_result.has_value()))
    {
        return MakeUnexpected(ComErrc::kFindServiceHandlerFailure,
                              start_find_service_result.error().UserMessage().data());
    }
    return start_find_service_result;
}

score::ResultBlank ProxyBase::StopFindService(const FindServiceHandle handle) noexcept
{
    const auto stop_find_service_result = Runtime::getInstance().GetServiceDiscovery().StopFindService(handle);
    if (!(stop_find_service_result.has_value()))
    {
        return MakeUnexpected(ComErrc::kInvalidHandle, stop_find_service_result.error().UserMessage().data());
    }
    return stop_find_service_result;
}

ProxyBaseView::ProxyBaseView(ProxyBase& proxy_base) noexcept : proxy_base_(proxy_base) {}

ProxyBinding* ProxyBaseView::GetBinding() noexcept
{
    return proxy_base_.proxy_binding_.get();
}

}  // namespace score::mw::com::impl
