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

#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/methods/proxy_method_base.h"
#include "score/mw/com/impl/proxy_event_base.h"
#include "score/mw/com/impl/proxy_field_base.h"
#include "score/mw/com/impl/runtime.h"

#include <score/assert.hpp>

#include <cstdlib>
#include <memory>
#include <utility>

namespace score::mw::com::impl
{

ProxyBase::ProxyBase(std::unique_ptr<ProxyBinding> proxy_binding, HandleType handle)
    : proxy_binding_{std::move(proxy_binding)},
      handle_{std::move(handle)},
      are_service_element_bindings_valid_{true},
      events_{},
      fields_{},
      methods_{}
{
}

ProxyBase::ProxyBase(ProxyBase&& other) noexcept
    : proxy_binding_(std::move(other.proxy_binding_)),
      handle_(std::move(other.handle_)),
      are_service_element_bindings_valid_(other.are_service_element_bindings_valid_),
      events_(std::move(other.events_)),
      fields_(std::move(other.fields_)),
      methods_(std::move(other.methods_))
{
    // Since the address of this proxy has changed, we need update the address stored in each of the events, fields
    // and methods belonging to the proxy.
    for (auto& event : events_)
    {
        event.second.get().UpdateProxyReference(*this);
    }

    for (auto& field : fields_)
    {
        field.second.get().UpdateProxyReference(*this);
    }

    for (auto& method : methods_)
    {
        method.second.get().UpdateProxyReference(*this);
    }
}

ProxyBase& ProxyBase::operator=(ProxyBase&& other) noexcept
{
    if (this != &other)
    {
        proxy_binding_ = std::move(other.proxy_binding_);
        handle_ = std::move(other.handle_);
        are_service_element_bindings_valid_ = other.are_service_element_bindings_valid_;
        events_ = std::move(other.events_);
        fields_ = std::move(other.fields_);
        methods_ = std::move(other.methods_);

        // Since the address of this proxy has changed, we need update the address stored in each of the events, fields
        // and methods belonging to the proxy.
        for (auto& event : events_)
        {
            event.second.get().UpdateProxyReference(*this);
        }

        for (auto& field : fields_)
        {
            field.second.get().UpdateProxyReference(*this);
        }

        for (auto& method : methods_)
        {
            method.second.get().UpdateProxyReference(*this);
        }
    }
    return *this;
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
        return MakeUnexpected(ComErrc::kFindServiceHandlerFailure, start_find_service_result.error().UserMessage());
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
        return MakeUnexpected(ComErrc::kFindServiceHandlerFailure, start_find_service_result.error().UserMessage());
    }
    return start_find_service_result;
}

score::ResultBlank ProxyBase::StopFindService(const FindServiceHandle handle) noexcept
{
    const auto stop_find_service_result = Runtime::getInstance().GetServiceDiscovery().StopFindService(handle);
    if (!(stop_find_service_result.has_value()))
    {
        return MakeUnexpected(ComErrc::kInvalidHandle, stop_find_service_result.error().UserMessage());
    }
    return stop_find_service_result;
}

ProxyBaseView::ProxyBaseView(ProxyBase& proxy_base) noexcept : proxy_base_(proxy_base) {}

ProxyBinding* ProxyBaseView::GetBinding() noexcept
{
    return proxy_base_.proxy_binding_.get();
}

const HandleType& ProxyBaseView::GetAssociatedHandleType() const noexcept
{
    return proxy_base_.handle_;
}

void ProxyBaseView::MarkServiceElementBindingInvalid() noexcept
{
    proxy_base_.are_service_element_bindings_valid_ = false;
}

void ProxyBaseView::RegisterEvent(const std::string_view event_name, ProxyEventBase& event)
{
    const auto result = proxy_base_.events_.emplace(event_name, event);
    const bool was_event_inserted = result.second;
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(was_event_inserted, "Event cannot be registered as it already exists.");
}

void ProxyBaseView::RegisterField(const std::string_view field_name, ProxyFieldBase& field)
{
    const auto result = proxy_base_.fields_.emplace(field_name, field);
    const bool was_field_inserted = result.second;
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(was_field_inserted, "Field cannot be registered as it already exists.");
}

void ProxyBaseView::RegisterMethod(const std::string_view method_name, ProxyMethodBase& method)
{
    const auto result = proxy_base_.methods_.emplace(method_name, method);
    const bool was_method_inserted = result.second;
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(was_method_inserted, "Method cannot be registered as it already exists.");
}

void ProxyBaseView::UpdateEvent(const std::string_view event_name, ProxyEventBase& event)
{
    auto event_it = proxy_base_.events_.find(event_name);
    if (event_it == proxy_base_.events_.cend())
    {
        score::mw::log::LogFatal("lola") << "ProxyBaseView::UpdateEvent failed to update, because the requested event "
                                       << event_name << " doesn't exist!";

        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(false);
    }

    event_it->second = event;
}

void ProxyBaseView::UpdateField(const std::string_view field_name, ProxyFieldBase& field)
{
    auto field_it = proxy_base_.fields_.find(field_name);
    if (field_it == proxy_base_.fields_.cend())
    {
        score::mw::log::LogFatal("lola") << "ProxyBaseView::UpdateField failed to update, because the requested field "
                                       << field_name << " doesn't exist";
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(false);
    }

    field_it->second = field;
}

void ProxyBaseView::UpdateMethod(const std::string_view method_name, ProxyMethodBase& method)
{
    auto method_it = proxy_base_.methods_.find(method_name);
    if (method_it == proxy_base_.methods_.cend())
    {
        score::mw::log::LogFatal("lola") << "ProxyBaseView::UpdateMethod failed to update, because the requested method "
                                       << method_name << " doesn't exist";
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(false);
    }

    method_it->second = method;
}

bool ProxyBaseView::AreBindingsValid() const
{
    return proxy_base_.AreBindingsValid();
}

}  // namespace score::mw::com::impl
