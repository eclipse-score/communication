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
#include "datatype.h"

#include <cassert>

extern "C" {

::score::mw::com::IpcBridgeProxy* mw_com_gen_ProxyWrapperClass_mw_com_example_IpcBridge_create(
    const ::score::mw::com::IpcBridgeProxy::HandleType& handle)
{
    if (auto result = ::score::mw::com::IpcBridgeProxy::Create(handle); result.has_value())
    {
        return new ::score::mw::com::IpcBridgeProxy{std::move(result).value()};
    }
    else
    {
        // Todo convert error and return that instead of a nullptr
        return nullptr;
    }
}

void mw_com_gen_ProxyWrapperClass_mw_com_example_IpcBridge_delete(::score::mw::com::IpcBridgeProxy* proxy)
{
    delete proxy;
}

::score::mw::com::impl::ProxyEvent<::score::mw::com::MapApiLanesStamped>*
mw_com_gen_ProxyWrapperClass_mw_com_example_IpcBridge_map_api_lanes_stamped_get(
    ::score::mw::com::IpcBridgeProxy* proxy) noexcept
{
    return &proxy->map_api_lanes_stamped_;
}

bool mw_com_gen_ProxyEvent_MapApiLanesStamped_get_new_sample(
    ::score::mw::com::impl::ProxyEvent<::score::mw::com::MapApiLanesStamped>& proxy_event,
    ::score::mw::com::impl::SamplePtr<::score::mw::com::MapApiLanesStamped>* sample_ptr) noexcept
{
    bool received = false;
    const auto result = proxy_event.GetNewSamples(
        [sample_ptr, &received](::score::mw::com::impl::SamplePtr<::score::mw::com::MapApiLanesStamped> sample_ptr_in) {
            assert(!received);
            // Use placement new to not delete garbage as the sample ptr is an uninitialized out pointer
            new (sample_ptr)::score::mw::com::impl::SamplePtr<::score::mw::com::MapApiLanesStamped>{
                std::move(sample_ptr_in)};
            received = true;
        },
        1);

    // If received is set but result is bad, we don't want to return a pointer. Therefore, delete the
    // already-received sample and return false
    if (received && !result.has_value())
    {
        delete sample_ptr;
        return false;
    }
    else
    {
        return received;
    }
}

std::uint32_t mw_com_gen_MapApiLanesStamped_get_size() noexcept
{
    return sizeof(::score::mw::com::MapApiLanesStamped);
}

const ::score::mw::com::MapApiLanesStamped* mw_com_gen_SamplePtr_MapApiLanesStamped_get(
    const ::score::mw::com::impl::SamplePtr<::score::mw::com::MapApiLanesStamped>* sample_ptr) noexcept
{
    return sample_ptr->Get();
}

void mw_com_gen_SamplePtr_MapApiLanesStamped_delete(
    ::score::mw::com::impl::SamplePtr<::score::mw::com::MapApiLanesStamped>* sample_ptr)
{
    sample_ptr->~SamplePtr<::score::mw::com::MapApiLanesStamped>();
}
}
