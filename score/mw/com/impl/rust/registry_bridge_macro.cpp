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

/// \file registry_bridge_macro.cpp
/// \brief C FFI wrapper implementations for generic event bridge APIs
/// \details This file provides extern "C" functions that wrap the C++ generic APIs.
/// These functions are called by Rust code through FFI and provide safe,
/// C-compatible interfaces to the C++ generic event system.
/// The functions bridge between:
/// - Rust side: String-based, safe wrapper APIs
/// - C++ side: Template-based, type-erased implementation

#include "score/mw/com/impl/rust/registry_bridge_macro.h"
#include "score/mw/com/impl/proxy_base.h"
#include "score/mw/com/impl/proxy_event.h"
#include "score/mw/com/impl/proxy_event_base.h"
#include "score/mw/com/impl/skeleton_base.h"
#include "score/mw/com/impl/skeleton_event.h"
#include "score/mw/com/impl/skeleton_event_base.h"
#include "score/mw/com/types.h"

#include <limits>
#include <string_view>

namespace score::mw::com::impl::rust
{

extern "C" {

/// \brief Get event pointer from proxy by event name
/// \details Retrieves an event from a proxy instance by string name.
/// The returned pointer should be cast to ProxyEvent<T>* where T is the event type.
/// \param proxy_ptr Opaque proxy pointer (actually ProxyType*)
/// \param interface_id UTF-8 string view of interface ID
/// \param event_id UTF-8 string view of event name
/// \return Pointer to ProxyEventBase if found, nullptr otherwise
ProxyEventBase* mw_com_get_event_from_proxy(ProxyBase* proxy_ptr, StringView interface_id, StringView event_id)
{
    if (proxy_ptr == nullptr || interface_id.data == nullptr || event_id.data == nullptr)
    {
        return nullptr;
    }
    auto event = static_cast<std::string_view>(event_id);
    auto id = static_cast<std::string_view>(interface_id);

    auto registry = GlobalRegistryMapping::FindMemberOperation(id, event);

    if (registry == nullptr)
    {
        return nullptr;
    }
    return registry->GetProxyEvent(proxy_ptr);
}

/// \brief Get event pointer from skeleton by event name
/// \details Retrieves an event from a skeleton instance by string name.
/// Similar to mw_com_get_event_from_proxy but for skeleton instances.
/// \param skeleton_ptr Opaque skeleton pointer (actually SkeletonType*)
/// \param interface_id UTF-8 string view of interface ID
/// \param event_id UTF-8 string view of event name
/// \return Pointer to SkeletonEventBase if found, nullptr otherwise
SkeletonEventBase* mw_com_get_event_from_skeleton(SkeletonBase* skeleton_ptr,
                                                  StringView interface_id,
                                                  StringView event_id)
{
    if (skeleton_ptr == nullptr || interface_id.data == nullptr || event_id.data == nullptr)
    {
        return nullptr;
    }
    auto event = static_cast<std::string_view>(event_id);
    auto id = static_cast<std::string_view>(interface_id);

    auto registry = GlobalRegistryMapping::FindMemberOperation(id, event);

    if (registry == nullptr)
    {
        return nullptr;
    }
    return registry->GetSkeletonEvent(skeleton_ptr);
}

/// \brief Send data via a skeleton event by name
/// \details Sends event data to all subscribed proxy instances.
/// \param event_ptr Opaque skeleton event pointer (SkeletonEvent<T>*)
/// \param event_type UTF-8 string view of event type name
/// \param data_ptr Pointer to event data (T*)
void mw_com_skeleton_send_event(SkeletonEventBase* event_ptr, StringView event_type, void* data_ptr)
{
    if (event_ptr == nullptr || event_type.data == nullptr || data_ptr == nullptr)
    {
        return;
    }

    auto name = static_cast<std::string_view>(event_type);

    auto registry = GlobalRegistryMapping::FindTypeInformation(name);

    if (registry == nullptr)
    {
        return;
    }
    return registry->SkeletonSendEvent(event_ptr, data_ptr);
}

/// \brief Subscribe to a proxy event to allocate sample buffers
/// \details Must be called before GetNewSamples to initialize the event's sample tracker.
/// \param event_ptr Opaque event pointer (ProxyEventBase*)
/// \param max_sample_count Maximum number of concurrent samples to allocate
/// \return true if subscription successful, false otherwise
bool mw_com_proxy_event_subscribe(ProxyEventBase* event_ptr, uint32_t max_sample_count)
{
    if (event_ptr == nullptr)
    {
        return false;
    }

    auto result = event_ptr->Subscribe(max_sample_count);

    if (!result.has_value())
    {
        return false;
    }

    return true;
}

/// \brief Create proxy instance dynamically
/// \details Creates a proxy for the given interface UID using the provided handle.
/// \param interface_id UTF-8 string view of interface UID (e.g., "mw_com_IpcBridge")
/// \param handle_ptr Opaque handle identifying the service instance
/// \return Pointer to ProxyBase instance, or nullptr on failure
ProxyBase* mw_com_create_proxy(StringView interface_id, const HandleType& handle_ptr)
{
    if (interface_id.data == nullptr)
    {
        return nullptr;
    }
    auto id = static_cast<std::string_view>(interface_id);
    auto registry = GlobalRegistryMapping::FindInterfaceRegistry(id);

    if (registry == nullptr)
    {
        return nullptr;
    }
    return registry->CreateProxy(handle_ptr);
}

/// \brief Create skeleton instance dynamically
/// \details Creates a skeleton for the given interface UID.
/// \param interface_id UTF-8 string view of interface UID
/// \param instance_spec Pointer to InstanceSpecifier identifying the service to offer
/// \return Pointer to SkeletonBase instance, or nullptr on failure
SkeletonBase* mw_com_create_skeleton(StringView interface_id, ::score::mw::com::InstanceSpecifier* instance_spec)
{
    if (interface_id.data == nullptr || instance_spec == nullptr)
    {
        return nullptr;
    }

    auto id = static_cast<std::string_view>(interface_id);
    auto registry = GlobalRegistryMapping::FindInterfaceRegistry(id);

    if (registry == nullptr)
    {
        return nullptr;
    }

    return registry->CreateSkeleton(*instance_spec);
}

/// \brief Offer service for skeleton instance
/// \details Starts offering the service on the provided skeleton instance.
/// \param skeleton_ptr Opaque skeleton pointer
/// \return true if service is offered successfully, false otherwise
bool mw_com_skeleton_offer_service(SkeletonBase* skeleton_ptr)
{
    if (skeleton_ptr == nullptr)
    {
        return false;
    }

    bool result = skeleton_ptr->OfferService().has_value();
    return result;
}

/// \brief Stop offering service for skeleton instance
/// \details Stops offering the service on the provided skeleton instance.
/// \param skeleton_ptr Opaque skeleton pointer
void mw_com_skeleton_stop_offer_service(SkeletonBase* skeleton_ptr)
{
    if (skeleton_ptr == nullptr)
    {
        return;
    }
    skeleton_ptr->StopOfferService();
}

/// \brief Destroy proxy instance
/// \details Deallocates a proxy created with mw_com_create_proxy.
/// \param proxy_ptr Opaque proxy pointer to destroy
void mw_com_destroy_proxy(ProxyBase* proxy_ptr)
{
    if (proxy_ptr == nullptr)
    {
        return;
    }

    delete proxy_ptr;
    ;
}

/// \brief Destroy skeleton instance
/// \details Deallocates a skeleton created with mw_com_create_skeleton.
/// \param skeleton_ptr Opaque skeleton pointer to destroy
void mw_com_destroy_skeleton(SkeletonBase* skeleton_ptr)
{
    if (skeleton_ptr == nullptr)
    {
        return;
    }
    delete skeleton_ptr;
}

/// \brief Get samples from proxy event of specific type
/// \details Retrieves new samples from a proxy event using the type operations registry.
/// \param event_ptr Opaque proxy event pointer (ProxyEventBase*)
/// \param event_type UTF-8 string view of event type name
/// \param callback Pointer to FatPtr callback for sample processing
/// \param max_samples Maximum number of samples to retrieve
/// \return Number of samples retrieved, or std::numeric_limits<std::uint32_t>::max() on error
std::uint32_t mw_com_type_registry_get_samples_from_event(ProxyEventBase* event_ptr,
                                                          StringView event_type,
                                                          const FatPtr* callback,
                                                          uint32_t max_samples)
{
    if (event_ptr == nullptr || event_type.data == nullptr || callback == nullptr)
    {
        return std::numeric_limits<std::uint32_t>::max();
    }

    auto id = static_cast<std::string_view>(event_type);
    auto registry = GlobalRegistryMapping::FindTypeInformation(id);
    if (registry == nullptr)
    {
        return std::numeric_limits<std::uint32_t>::max();
    }

    auto result = registry->GetSamplesFromEvent(event_ptr, max_samples, *callback);

    if (result.has_value() == false)
    {
        return std::numeric_limits<std::uint32_t>::max();
    }

    return result.value();
}

}  // extern "C"
}  // namespace score::mw::com::impl::rust
