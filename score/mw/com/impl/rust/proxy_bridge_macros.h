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
#ifndef SCORE_MW_COM_IMPL_RUST_PROXY_BRIDGE_GEN_HELPER_H
#define SCORE_MW_COM_IMPL_RUST_PROXY_BRIDGE_GEN_HELPER_H

#include "score/mw/com/impl/plumbing/sample_ptr.h"
#include "score/mw/com/impl/proxy_event.h"

#include <score/assert.hpp>
#include <optional>

namespace score::mw::com::impl::rust
{

template <typename T>
inline bool GetSampleFromEvent(::score::mw::com::impl::ProxyEvent<T>& proxy_event,
                               ::score::mw::com::impl::SamplePtr<T>* sample_ptr) noexcept
{
    std::optional<::score::mw::com::impl::SamplePtr<T>> sample_ptr_guard{};
    const auto result = proxy_event.GetNewSamples(
        [&sample_ptr_guard](::score::mw::com::impl::SamplePtr<T> sample_ptr_in) {
            SCORE_LANGUAGE_FUTURECPP_ASSERT_DBG_MESSAGE(!sample_ptr_guard, "Received multiple samples in one event");
            sample_ptr_guard = std::move(sample_ptr_in);
        },
        1);

    if (sample_ptr_guard.has_value() && result.has_value())
    {
        new (sample_ptr)::score::mw::com::impl::SamplePtr<T>{std::move(sample_ptr_guard).value()};
        return true;
    }
    return false;
}

template <typename P>
inline P* CreateProxyWrapper(const typename P::HandleType& handle)
{
    if (auto result = P::Create(handle))
    {
        return new P{std::move(result).value()};
    }
    else
    {
        return nullptr; /* TODO: Properly handle */
    }
}

}  // namespace score::mw::com::impl::rust

#define BEGIN_EXPORT_MW_COM_INTERFACE(uid, proxy_type, skeleton_type)                                               \
    extern "C" {                                                                                                    \
                                                                                                                    \
    using uid##MwComProxyType = proxy_type;                                                                         \
    using uid##MwComSkeletonType = skeleton_type;                                                                   \
                                                                                                                    \
    uid##MwComProxyType* mw_com_gen_ProxyWrapperClass_##uid##_create(const uid##MwComProxyType::HandleType& handle) \
    {                                                                                                               \
        return ::score::mw::com::impl::rust::CreateProxyWrapper<uid##MwComProxyType>(handle);                         \
    }                                                                                                               \
                                                                                                                    \
    void mw_com_gen_ProxyWrapperClass_##uid##_delete(uid##MwComProxyType* proxy)                                    \
    {                                                                                                               \
        delete proxy;                                                                                               \
    }

#define EXPORT_MW_COM_EVENT(uid, event_type, event_name)                                                   \
    ::score::mw::com::impl::ProxyEvent<event_type>* mw_com_gen_ProxyWrapperClass_##uid##_##event_name##_get( \
        uid##MwComProxyType* proxy) noexcept                                                               \
    {                                                                                                      \
        return &proxy->event_name;                                                                         \
    }

#define END_EXPORT_MW_COM_INTERFACE() }  // extern "C"

#define EXPORT_MW_COM_TYPE(uid, type)                                                                              \
    extern "C" {                                                                                                   \
    bool mw_com_gen_ProxyEvent_##uid##_get_new_sample(::score::mw::com::impl::ProxyEvent<type>& proxy_event,         \
                                                      ::score::mw::com::impl::SamplePtr<type>* sample_ptr) noexcept  \
    {                                                                                                              \
        return ::score::mw::com::impl::rust::GetSampleFromEvent(proxy_event, sample_ptr);                            \
    }                                                                                                              \
                                                                                                                   \
    std::uint32_t mw_com_gen_##uid##_get_size() noexcept                                                           \
    {                                                                                                              \
        return sizeof(type);                                                                                       \
    }                                                                                                              \
                                                                                                                   \
    const type* mw_com_gen_SamplePtr_##uid##_get(const ::score::mw::com::impl::SamplePtr<type>* sample_ptr) noexcept \
    {                                                                                                              \
        return sample_ptr->Get();                                                                                  \
    }                                                                                                              \
                                                                                                                   \
    void mw_com_gen_SamplePtr_##uid##_delete(::score::mw::com::impl::SamplePtr<type>* sample_ptr)                    \
    {                                                                                                              \
        sample_ptr->~SamplePtr<type>();                                                                            \
    }                                                                                                              \
    }

#endif  // SCORE_MW_COM_IMPL_RUST_PROXY_BRIDGE_GEN_HELPER_H
