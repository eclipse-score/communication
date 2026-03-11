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
#ifndef SCORE_MW_COM_IMPL_RUST_BRIDGE_MACROS_H
#define SCORE_MW_COM_IMPL_RUST_BRIDGE_MACROS_H

#include "score/mw/com/impl/rust/proxy_bridge.h"

#include "score/mw/com/impl/plumbing/sample_ptr.h"
#include "score/mw/com/impl/proxy_event.h"
#include "score/mw/com/impl/skeleton_event.h"
#include "score/mw/com/types.h"

#include <score/assert.hpp>

namespace score::mw::com::impl::rust
{

template <typename T>
inline score::Result<std::uint32_t> GetSamplesFromEvent(::score::mw::com::impl::ProxyEvent<T>& proxy_event,
                                                      const FatPtr& callback,
                                                      std::uint32_t max_num_samples) noexcept
{
    RustFnMutCallable<RustRefMutCallable, void, SamplePtr<T>> rust_callable{callback};
    auto samples = proxy_event.GetNewSamples(std::move(rust_callable), max_num_samples);
    if (samples.has_value())
    {
        return *samples;
    }
    else
    {
        return score::MakeUnexpected<std::uint32_t>(samples.error());
    }
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

template <typename S>
inline S* CreateSkeletonWrapper(const ::score::mw::com::InstanceSpecifier& instance_specifier)
{
    if (auto result = S::Create(instance_specifier); result.has_value())
    {
        return new S{std::move(result).value()};
    }
    else
    {
        // Todo convert error and return that instead of a nullptr
        return nullptr;
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
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(proxy != nullptr, "Attempt to delete nullptr proxywrapper!");                        \
        delete proxy;                                                                                               \
    }                                                                                                               \
                                                                                                                    \
    uid##MwComSkeletonType* mw_com_gen_SkeletonWrapperClass_##uid##_create(                                         \
        const ::score::mw::com::InstanceSpecifier& instance_specifier)                                                \
    {                                                                                                               \
        return ::score::mw::com::impl::rust::CreateSkeletonWrapper<uid##MwComSkeletonType>(instance_specifier);       \
    }                                                                                                               \
                                                                                                                    \
    void mw_com_gen_SkeletonWrapperClass_##uid##_delete(uid##MwComSkeletonType* skeleton)                           \
    {                                                                                                               \
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(skeleton != nullptr, "Attempt to delete nullptr skeleton!");                         \
        delete skeleton;                                                                                            \
    }                                                                                                               \
                                                                                                                    \
    bool mw_com_gen_SkeletonWrapperClass_##uid##_offer(uid##MwComSkeletonType* skeleton)                            \
    {                                                                                                               \
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(skeleton != nullptr, "Attempt to use nullptr skeleton!");                            \
        return skeleton->OfferService().has_value();                                                                \
    }                                                                                                               \
                                                                                                                    \
    void mw_com_gen_SkeletonWrapperClass_##uid##_stop_offer(uid##MwComSkeletonType* skeleton)                       \
    {                                                                                                               \
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(skeleton != nullptr, "Attempt to use nullptr skeleton!");                            \
        skeleton->StopOfferService();                                                                               \
    }

#define EXPORT_MW_COM_EVENT(uid, event_type, event_name)                                                         \
    ::score::mw::com::impl::ProxyEvent<event_type>* mw_com_gen_ProxyWrapperClass_##uid##_##event_name##_get(       \
        uid##MwComProxyType* proxy) noexcept                                                                     \
    {                                                                                                            \
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(proxy != nullptr, "Attempt to use nullptr proxy!");                               \
        return &proxy->event_name;                                                                               \
    }                                                                                                            \
                                                                                                                 \
    ::score::mw::com::impl::SkeletonEvent<event_type>* mw_com_gen_SkeletonWrapperClass_##uid##_##event_name##_get( \
        uid##MwComSkeletonType* skeleton) noexcept                                                               \
    {                                                                                                            \
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(skeleton != nullptr, "Attempt to use nullptr skeleton!");                         \
        return &skeleton->event_name;                                                                            \
    }

#define END_EXPORT_MW_COM_INTERFACE() }  // extern "C"

#define EXPORT_MW_COM_TYPE(uid, type)                                                                                \
    extern "C" {                                                                                                     \
    void mw_com_impl_call_dyn_ref_fnmut_sample_##uid(const ::score::mw::com::impl::rust::FatPtr* boxed_fnmut,          \
                                                     ::score::mw::com::impl::SamplePtr<type>* ptr);                    \
    }                                                                                                                \
    template <>                                                                                                      \
    class score::mw::com::impl::rust::RustRefMutCallable<void, ::score::mw::com::impl::SamplePtr<type>>                  \
    {                                                                                                                \
      public:                                                                                                        \
        static void invoke(::score::mw::com::impl::rust::FatPtr ptr_,                                                  \
                           ::score::mw::com::impl::SamplePtr<type> sample) noexcept                                    \
        {                                                                                                            \
            alignas(                                                                                                 \
                ::score::mw::com::impl::SamplePtr<type>) char storage[sizeof(::score::mw::com::impl::SamplePtr<type>)];  \
            auto* placement_sample = new (storage)::score::mw::com::impl::SamplePtr<type>(std::move(sample));          \
            mw_com_impl_call_dyn_ref_fnmut_sample_##uid(&ptr_, placement_sample);                                    \
        }                                                                                                            \
                                                                                                                     \
        static void dispose(::score::mw::com::impl::rust::FatPtr) noexcept {}                                          \
    };                                                                                                               \
    extern "C" {                                                                                                     \
    std::uint32_t mw_com_gen_ProxyEvent_##uid##_get_new_samples(::score::mw::com::impl::ProxyEvent<type>& proxy_event, \
                                                                const ::score::mw::com::impl::rust::FatPtr& callback,  \
                                                                std::uint32_t max_num_samples) noexcept              \
    {                                                                                                                \
        auto result = ::score::mw::com::impl::rust::GetSamplesFromEvent(proxy_event, callback, max_num_samples);       \
        return result.value_or(0);                                                                                   \
    }                                                                                                                \
                                                                                                                     \
    std::uint32_t mw_com_gen_##uid##_get_size() noexcept                                                             \
    {                                                                                                                \
        return sizeof(type);                                                                                         \
    }                                                                                                                \
                                                                                                                     \
    const type* mw_com_gen_SamplePtr_##uid##_get(const ::score::mw::com::impl::SamplePtr<type>* sample_ptr) noexcept   \
    {                                                                                                                \
        if (sample_ptr != nullptr)                                                                                   \
        {                                                                                                            \
            return sample_ptr->Get();                                                                                \
        }                                                                                                            \
        else                                                                                                         \
        {                                                                                                            \
            return nullptr;                                                                                          \
        }                                                                                                            \
    }                                                                                                                \
                                                                                                                     \
    void mw_com_gen_SamplePtr_##uid##_delete(::score::mw::com::impl::SamplePtr<type>* sample_ptr)                      \
    {                                                                                                                \
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(sample_ptr != nullptr, "Attempt to delete null sample ptr!");                         \
        sample_ptr->~SamplePtr<type>();                                                                              \
    }                                                                                                                \
                                                                                                                     \
    bool mw_com_gen_SkeletonEvent_##uid##_send(::score::mw::com::impl::SkeletonEvent<type>* event, type* data)         \
    {                                                                                                                \
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(event != nullptr, "Attempt to use nullptr event!");                                   \
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(data != nullptr, "Attempt to send nullptr data");                                     \
        return event->Send(std::move(*data)).has_value();                                                            \
    }                                                                                                                \
    }

#endif  // SCORE_MW_COM_IMPL_RUST_BRIDGE_MACROS_H
