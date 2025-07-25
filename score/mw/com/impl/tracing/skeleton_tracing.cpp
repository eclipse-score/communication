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
///
/// @file
/// @copyright Copyright (C) 2023, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
///
#include "score/mw/com/impl/tracing/skeleton_tracing.h"

#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/impl/skeleton_event_base.h"
#include "score/mw/com/impl/tracing/configuration/service_element_instance_identifier_view.h"
#include "score/mw/com/impl/tracing/tracing_runtime.h"

#include <map>

namespace score::mw::com::impl::tracing
{

namespace
{

bool IsTracingEnabledForInterfaceEvent(const SkeletonEventTracingData& skeleton_event_tracing) noexcept
{
    return (skeleton_event_tracing.enable_send || skeleton_event_tracing.enable_send_with_allocate);
}

bool IsTracingEnabledForAnyEvent(
    const std::map<score::cpp::string_view, std::reference_wrapper<SkeletonEventBase>>& events) noexcept
{
    for (const auto& event : events)
    {
        auto& skeleton_event_base = event.second.get();
        auto skeleton_event_base_view = SkeletonEventBaseView(skeleton_event_base);
        const auto& skeleton_event_tracing = skeleton_event_base_view.GetSkeletonEventTracing();
        const bool tracing_enabled = IsTracingEnabledForInterfaceEvent(skeleton_event_tracing);
        if (tracing_enabled)
        {
            return true;
        }
    }
    return false;
}

bool IsTracingEnabledForAnyField(
    const std::map<score::cpp::string_view, std::reference_wrapper<SkeletonFieldBase>>& fields) noexcept
{
    for (const auto& field : fields)
    {
        auto& skeleton_field_base = field.second.get();
        auto skeleton_field_base_view = SkeletonFieldBaseView(skeleton_field_base);
        auto skeleton_event_base_view = SkeletonEventBaseView(skeleton_field_base_view.GetEventBase());
        const auto& skeleton_event_tracing = skeleton_event_base_view.GetSkeletonEventTracing();
        const bool tracing_enabled = IsTracingEnabledForInterfaceEvent(skeleton_event_tracing);
        if (tracing_enabled)
        {
            return true;
        }
    }
    return false;
}

bool IsTracingEnabledForInstance(
    ITracingRuntime* const tracing_runtime,
    const std::map<score::cpp::string_view, std::reference_wrapper<SkeletonEventBase>>& events,
    const std::map<score::cpp::string_view, std::reference_wrapper<SkeletonFieldBase>>& fields) noexcept
{
    if (tracing_runtime == nullptr)
    {
        return false;
    }

    const bool tracing_enabled_for_any_event = IsTracingEnabledForAnyEvent(events);
    if (tracing_enabled_for_any_event)
    {
        return true;
    }

    return IsTracingEnabledForAnyField(fields);
}

}  // namespace

std::optional<SkeletonBinding::RegisterShmObjectTraceCallback> CreateRegisterShmObjectCallback(
    const InstanceIdentifier& instance_id,
    const std::map<score::cpp::string_view, std::reference_wrapper<SkeletonEventBase>>& events,
    const std::map<score::cpp::string_view, std::reference_wrapper<SkeletonFieldBase>>& fields,
    const SkeletonBinding& skeleton_binding) noexcept
{
    std::optional<SkeletonBinding::RegisterShmObjectTraceCallback> tracing_handler{};
    auto* const tracing_runtime = impl::Runtime::getInstance().GetTracingRuntime();
    if (IsTracingEnabledForInstance(tracing_runtime, events, fields))
    {
        tracing_handler = [&instance_id, &skeleton_binding, tracing_runtime](
                              score::cpp::string_view element_name,
                              ServiceElementType element_type,
                              memory::shared::ISharedMemoryResource::FileDescriptor shm_object_fd,
                              void* shm_memory_start_address) -> void {
            const auto instance_identifier_view = InstanceIdentifierView(instance_id);
            const auto service_std_string_view =
                instance_identifier_view.GetServiceInstanceDeployment().service_.ToString();
            const auto service_string_view =
                score::cpp::string_view{service_std_string_view.data(), service_std_string_view.size()};
            tracing::ServiceElementIdentifierView service_element_identifier{
                service_string_view, element_name, element_type};
            const auto instance_specifier_std_string_view =
                instance_identifier_view.GetServiceInstanceDeployment().instance_specifier_.ToString();
            const auto instance_specifier_string_view =
                score::cpp::string_view{instance_specifier_std_string_view.data(), instance_specifier_std_string_view.size()};
            tracing::ServiceElementInstanceIdentifierView service_element_instance_identifier{
                service_element_identifier, instance_specifier_string_view};
            tracing_runtime->RegisterShmObject(skeleton_binding.GetBindingType(),
                                               service_element_instance_identifier,
                                               shm_object_fd,
                                               shm_memory_start_address);
        };
    }
    return tracing_handler;
}

std::optional<SkeletonBinding::UnregisterShmObjectTraceCallback> CreateUnregisterShmObjectCallback(
    const InstanceIdentifier& instance_id,
    const std::map<score::cpp::string_view, std::reference_wrapper<SkeletonEventBase>>& events,
    const std::map<score::cpp::string_view, std::reference_wrapper<SkeletonFieldBase>>& fields,
    const SkeletonBinding& skeleton_binding) noexcept
{
    std::optional<SkeletonBinding::UnregisterShmObjectTraceCallback> tracing_handler{};
    auto* const tracing_runtime = impl::Runtime::getInstance().GetTracingRuntime();
    if (IsTracingEnabledForInstance(tracing_runtime, events, fields))
    {
        tracing_handler = [&instance_id, &skeleton_binding, tracing_runtime](score::cpp::string_view element_name,
                                                                             ServiceElementType element_type) -> void {
            const auto instance_identifier_view = InstanceIdentifierView(instance_id);
            const auto service_std_string_view =
                instance_identifier_view.GetServiceInstanceDeployment().service_.ToString();
            const auto service_string_view =
                score::cpp::string_view{service_std_string_view.data(), service_std_string_view.size()};
            tracing::ServiceElementIdentifierView service_element_identifier{
                service_string_view, element_name, element_type};
            const auto instance_specifier_std_string_view =
                instance_identifier_view.GetServiceInstanceDeployment().instance_specifier_.ToString();
            const auto instance_specifier_string_view =
                score::cpp::string_view{instance_specifier_std_string_view.data(), instance_specifier_std_string_view.size()};
            tracing::ServiceElementInstanceIdentifierView service_element_instance_identifier{
                service_element_identifier, instance_specifier_string_view};
            tracing_runtime->UnregisterShmObject(skeleton_binding.GetBindingType(),
                                                 service_element_instance_identifier);
        };
    }
    return tracing_handler;
}

}  // namespace score::mw::com::impl::tracing
