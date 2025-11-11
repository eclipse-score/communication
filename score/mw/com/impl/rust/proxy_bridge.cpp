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
#include "score/mw/com/impl/plumbing/sample_ptr.h"
#include "score/mw/com/impl/proxy_event.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/runtime.h"
#include "score/mw/com/types.h"

#include <score/assert.hpp>
#include <cstdint>

struct FatPtr
{
    const void* vtbl;
    void* data;
};

extern "C" {

void mw_com_impl_call_dyn_fnmut(const FatPtr* boxed_fnmut) noexcept;
void mw_com_impl_delete_boxed_fnmut(const FatPtr* boxed_fnmut) noexcept;
}

template <typename R, typename... Args>
class RustFnMutCallable;

template <>
class RustFnMutCallable<void> final
{
  public:
    /// Constructs a new callable from a Rust fat ptr that was created from a Box<dyn FnMut()>.
    ///
    /// Any other type will lead to UB as the used Rust callback functions will assume this and transmute into that type
    // to call the callback on Rust side.
    explicit RustFnMutCallable(const FatPtr& dyn_fnmut) noexcept : ptr_{dyn_fnmut}
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(dyn_fnmut.data != nullptr,
                                                    "Failed creating a RustFnMutCallable due to an invalid pointer");
    }

    RustFnMutCallable(const RustFnMutCallable&) = delete;
    RustFnMutCallable(RustFnMutCallable&& other) noexcept : ptr_{other.ptr_}
    {
        other.ptr_.data = nullptr;
    }

    RustFnMutCallable& operator=(const RustFnMutCallable&) = delete;
    RustFnMutCallable& operator=(RustFnMutCallable&& other) noexcept
    {
        std::swap(ptr_, other.ptr_);
        return *this;
    }

    ~RustFnMutCallable() noexcept
    {
        if (ptr_.data != nullptr)
        {
            mw_com_impl_delete_boxed_fnmut(&ptr_);
        }
    }

    void operator()() noexcept
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(ptr_.data != nullptr,
                                                    "Tried to call a Rust FnMut callable without a valid pointer");
        mw_com_impl_call_dyn_fnmut(&ptr_);
    }

  private:
    FatPtr ptr_;
};

extern "C" {

::score::mw::com::InstanceSpecifier* mw_com_impl_instance_specifier_create(
    const char* const instance_specifier,
    const std::uint32_t instance_specifier_length) noexcept
{
    if (auto result = ::score::mw::com::InstanceSpecifier::Create(
            std::string_view{instance_specifier, instance_specifier_length});
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

void mw_com_impl_proxy_event_set_receive_handler(::score::mw::com::impl::ProxyEventBase* proxy_event,
                                                 const FatPtr* const boxed_handler)
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
        boxed_handler != nullptr, "Call to mw_com_impl_proxy_event_set_receive_handler with a nullptr for the handler");
    proxy_event->SetReceiveHandler(RustFnMutCallable<void>{*boxed_handler});
}

void mw_com_impl_proxy_event_unset_receive_handler(::score::mw::com::impl::ProxyEventBase* proxy_event)
{
    proxy_event->UnsetReceiveHandler();
}
}
