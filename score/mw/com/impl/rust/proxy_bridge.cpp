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
#include "score/mw/com/impl/rust/proxy_bridge.h"

#include "score/mw/com/impl/plumbing/sample_ptr.h"
#include "score/mw/com/impl/proxy_event.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/runtime.h"
#include "score/mw/com/types.h"

#include <score/assert.hpp>
#include <cstdint>

namespace score::mw::com::impl::rust
{

extern "C" {

void mw_com_impl_call_dyn_fnmut(const FatPtr* boxed_fnmut) noexcept;
void mw_com_impl_delete_boxed_fnmut(const FatPtr* boxed_fnmut) noexcept;
}

template <>
class RustBoxedCallable<void>
{
  public:
    static void invoke(FatPtr ptr_) noexcept
    {
        mw_com_impl_call_dyn_fnmut(&ptr_);
    }

    static void dispose(FatPtr ptr_) noexcept
    {
        mw_com_impl_delete_boxed_fnmut(&ptr_);
    }
};

extern "C" {

::score::mw::com::InstanceSpecifier* mw_com_impl_instance_specifier_create(
    const char* const instance_specifier,
    const std::uint32_t instance_specifier_length) noexcept
{
    if (auto result =
            ::score::mw::com::InstanceSpecifier::Create(std::string{instance_specifier, instance_specifier_length});
        result.has_value())
    {
        return new ::score::mw::com::InstanceSpecifier{std::move(result).value()};
    }
    else
    {
        return nullptr;
    }
}

::score::mw::com::InstanceSpecifier* mw_com_impl_instance_specifier_clone(
    const ::score::mw::com::InstanceSpecifier& instance_specifier) noexcept
{
    return new ::score::mw::com::InstanceSpecifier{instance_specifier};
}

void mw_com_impl_instance_specifier_delete(::score::mw::com::InstanceSpecifier* instance_specifier) noexcept
{
    delete instance_specifier;
}

::score::mw::com::ServiceHandleContainer<::score::mw::com::impl::HandleType>* mw_com_impl_find_service(
    ::score::mw::com::InstanceSpecifier* instance_specifier) noexcept
{
    if (auto result = ::score::mw::com::impl::Runtime::getInstance().GetServiceDiscovery().FindService(
            std::move(*instance_specifier));
        result.has_value())
    {
        return new ::score::mw::com::ServiceHandleContainer<::score::mw::com::impl::HandleType>{
            std::move(result).value()};
    }
    else
    {
        return nullptr;
    }
}

void mw_com_impl_handle_container_delete(
    ::score::mw::com::ServiceHandleContainer<::score::mw::com::impl::HandleType>* container) noexcept
{
    delete container;
}

std::uint32_t mw_com_impl_handle_container_get_size(
    const ::score::mw::com::ServiceHandleContainer<::score::mw::com::impl::HandleType>* container)
{
    return static_cast<std::uint32_t>(container->size());
}

const ::score::mw::com::impl::HandleType* mw_com_impl_handle_container_get_handle_at(
    const ::score::mw::com::ServiceHandleContainer<::score::mw::com::impl::HandleType>* container,
    std::uint32_t pos) noexcept
{
    return &container->at(pos);
}

void mw_com_impl_initialize(const char* argv[], std::int32_t argc)
{
    ::score::mw::com::runtime::InitializeRuntime(argc, argv);
}

std::uint32_t mw_com_impl_sample_ptr_get_size() noexcept
{
    return sizeof(::score::mw::com::impl::SamplePtr<std::uint32_t>);
}

bool mw_com_impl_proxy_event_subscribe(::score::mw::com::impl::ProxyEventBase* proxy_event,
                                       std::uint32_t max_num_events)
{
    return proxy_event->Subscribe(max_num_events).has_value();
}

void mw_com_impl_proxy_event_unsubscribe(::score::mw::com::impl::ProxyEventBase* proxy_event)
{
    proxy_event->Unsubscribe();
}

bool mw_com_impl_proxy_event_set_receive_handler(::score::mw::com::impl::ProxyEventBase* proxy_event,
                                                 const FatPtr* const boxed_handler)
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
        boxed_handler != nullptr, "Call to mw_com_impl_proxy_event_set_receive_handler with a nullptr for the handler");
    const auto result = proxy_event->SetReceiveHandler(RustFnMutCallable<RustBoxedCallable>{*boxed_handler});
    if (!result.has_value())
    {
        return false;
    }
    return true;
}

bool mw_com_impl_proxy_event_unset_receive_handler(::score::mw::com::impl::ProxyEventBase* proxy_event)
{
    const auto result = proxy_event->UnsetReceiveHandler();
    if (!result.has_value())
    {
        return false;
    }
    return true;
}
}
}  // namespace score::mw::com::impl::rust
