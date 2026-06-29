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
#include "score/mw/com/impl/reference_to_moveable.h"
#include "score/mw/com/impl/runtime.h"

#include <score/assert.hpp>

#include <cstdlib>
#include <memory>
#include <tuple>
#include <utility>

namespace score::mw::com::impl
{

ProxyBase::ProxyBase(std::unique_ptr<ProxyBinding> proxy_binding, HandleType handle)
    : proxy_binding_{std::move(proxy_binding)}, handle_{std::move(handle)}, events_{}, fields_{}, methods_{}
{
}

const HandleType& ProxyBase::GetHandle() const& noexcept
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

score::Result<void> ProxyBase::StopFindService(const FindServiceHandle handle) noexcept
{
    const auto stop_find_service_result = Runtime::getInstance().GetServiceDiscovery().StopFindService(handle);
    if (!(stop_find_service_result.has_value()))
    {
        return MakeUnexpected(ComErrc::kInvalidHandle, stop_find_service_result.error().UserMessage());
    }
    return stop_find_service_result;
}

bool ProxyBase::AreBindingsValid() const noexcept
{
    const bool is_proxy_binding_valid = proxy_binding_ != nullptr;

    const bool are_event_bindings_valid = std::all_of(events_.begin(), events_.end(), [](const auto& element) {
        const auto binding_construction_result =
            ProxyEventBaseView{element.second.get().Get()}.GetBindingConstructionResult();
        if (!binding_construction_result.has_value())
        {
            score::mw::log::LogError("lola")
                << "Proxy event binding construction failed with error: " << binding_construction_result.error();
        }
        return binding_construction_result.has_value();
    });
    const bool are_field_bindings_valid = std::all_of(fields_.begin(), fields_.end(), [](const auto& element) {
        const auto event_binding_construction_result =
            ProxyFieldBaseView{element.second.get().Get()}.GetEventBindingConstructionResult();
        if (!event_binding_construction_result.has_value())
        {
            score::mw::log::LogError("lola") << "Proxy event binding construction for field " << element.first
                                             << " failed with error : " << event_binding_construction_result.error();
        }

        const auto setter_binding_construction_result =
            ProxyFieldBaseView{element.second.get().Get()}.GetSetterBindingConstructionResult();
        if (!setter_binding_construction_result.has_value())
        {
            score::mw::log::LogError("lola") << "Proxy setter method binding construction for field " << element.first
                                             << " failed with error : " << setter_binding_construction_result.error();
        }

        const auto getter_binding_construction_result =
            ProxyFieldBaseView{element.second.get().Get()}.GetGetterBindingConstructionResult();
        if (!getter_binding_construction_result.has_value())
        {
            score::mw::log::LogError("lola") << "Proxy getter method binding construction for field " << element.first
                                             << " failed with error : " << getter_binding_construction_result.error();
        }

        return event_binding_construction_result.has_value() && setter_binding_construction_result.has_value() &&
               getter_binding_construction_result.has_value();
    });
    const bool are_method_bindings_valid = std::all_of(methods_.begin(), methods_.end(), [](const auto& element) {
        const auto binding_construction_result =
            ProxyMethodBaseView{element.second.get().Get()}.GetBindingConstructionResult();
        if (!binding_construction_result.has_value())
        {
            score::mw::log::LogError("lola")
                << "Proxy method binding construction failed with error: " << binding_construction_result.error();
        }
        return binding_construction_result.has_value();
    });

    return is_proxy_binding_valid && are_event_bindings_valid && are_field_bindings_valid && are_method_bindings_valid;
}

Result<void> ProxyBase::SetupMethods()
{
    const auto result = proxy_binding_->SetupMethods();
    if (!result.has_value())
    {
        return MakeUnexpected<void>(result.error());
    }

    for (auto& method_key_value_pair : methods_)
    {
        auto& method = method_key_value_pair.second.get().Get();
        const auto method_init_result = method.InitializeInArgsAndReturnValues();
        if (!method_init_result.has_value())
        {
            if (method_init_result.error() == ComErrc::kMethodBindingDisabled)
            {
                score::mw::log::LogDebug("lola")
                    << "Proxy method " << method_key_value_pair.first
                    << " was disabled in the instance deployment configuration. No binding was created for it. "
                       "Skipping initialization of in-args and return values.";
            }
            else
            {
                score::mw::log::LogError("lola")
                    << "Proxy method " << method_key_value_pair.first
                    << " failed to initialize in-args and return values with error: " << method_init_result.error();
                return MakeUnexpected<void>(method_init_result.error());
            }
        }
    }
    return {};
}

void ProxyBase::Deinitialize()
{
    if (proxy_binding_ != nullptr)
    {
        proxy_binding_->PrepareDeinitialize();
    }
    for (auto& event : events_)
    {
        event.second.get().Get().Unsubscribe();
    }
    for (auto& field : fields_)
    {
        ProxyFieldBaseView{field.second.get().Get()}.Unsubscribe();
    }
    if (proxy_binding_ != nullptr)
    {
        proxy_binding_->FinalizeDeinitialize();
    }
}

ProxyBaseView::ProxyBaseView(ProxyBase& proxy_base) noexcept : proxy_base_(proxy_base) {}

ProxyBinding* ProxyBaseView::GetBinding() noexcept
{
    return proxy_base_.proxy_binding_.get();
}

const HandleType& ProxyBaseView::GetAssociatedHandleType() const& noexcept
{
    return proxy_base_.handle_;
}

void ProxyBaseView::RegisterEvent(const std::string_view event_name,
                                  ReferenceToMoveable<ProxyEventBase>::Reference& event)
{
    const auto result = proxy_base_.events_.emplace(event_name, std::ref(event));
    const bool was_event_inserted = result.second;
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(was_event_inserted, "Event cannot be registered as it already exists.");
}

void ProxyBaseView::RegisterField(const std::string_view field_name,
                                  ReferenceToMoveable<ProxyFieldBase>::Reference& field)
{
    const auto result = proxy_base_.fields_.emplace(field_name, std::ref(field));
    const bool was_field_inserted = result.second;
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(was_field_inserted, "Field cannot be registered as it already exists.");
}

void ProxyBaseView::RegisterMethod(const std::string_view method_name,
                                   ReferenceToMoveable<ProxyMethodBase>::Reference& method)
{
    const auto result = proxy_base_.methods_.emplace(method_name, std::ref(method));
    const bool was_method_inserted = result.second;
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(was_method_inserted, "Method cannot be registered as it already exists.");
}
bool ProxyBaseView::AreBindingsValid() const
{
    return proxy_base_.AreBindingsValid();
}

}  // namespace score::mw::com::impl
