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
#ifndef SCORE_MW_COM_IMPL_TRACING_I_TRACING_RUNTIME_H
#define SCORE_MW_COM_IMPL_TRACING_I_TRACING_RUNTIME_H

#include "score/memory/shared/i_shared_memory_resource.h"
#include "score/result/result.h"
#include "score/mw/com/impl/binding_type.h"
#include "score/mw/com/impl/tracing/configuration/proxy_event_trace_point_type.h"
#include "score/mw/com/impl/tracing/configuration/proxy_field_trace_point_type.h"
#include "score/mw/com/impl/tracing/configuration/service_element_instance_identifier_view.h"
#include "score/mw/com/impl/tracing/configuration/skeleton_event_trace_point_type.h"
#include "score/mw/com/impl/tracing/configuration/skeleton_field_trace_point_type.h"
#include "score/mw/com/impl/tracing/i_tracing_runtime_binding.h"
#include "score/mw/com/impl/tracing/service_element_tracing_data.h"
#include "score/mw/com/impl/tracing/type_erased_sample_ptr.h"

#include <cstdint>
#include <variant>

namespace score::mw::com::impl::tracing
{

class ITracingRuntime
{
  public:
    using TracePointType = std::variant<ProxyEventTracePointType,
                                        ProxyFieldTracePointType,
                                        SkeletonEventTracePointType,
                                        SkeletonFieldTracePointType>;
    using TracePointDataId = analysis::tracing::AraComProperties::TracePointDataId;

    ITracingRuntime() noexcept = default;

    virtual ~ITracingRuntime() noexcept = default;

    virtual void DisableTracing() noexcept = 0;

    virtual bool IsTracingEnabled() = 0;

    virtual ServiceElementTracingData RegisterServiceElement(const BindingType binding_type,
                                                             std::uint8_t number_of_ipc_tracing_slots) noexcept = 0;

    virtual void SetDataLossFlag(const BindingType binding_type) noexcept = 0;
    virtual void RegisterShmObject(const BindingType binding_type,
                                   const ServiceElementInstanceIdentifierView service_element_instance_identifier_view,
                                   const memory::shared::ISharedMemoryResource::FileDescriptor shm_object_fd,
                                   void* const shm_memory_start_address) noexcept = 0;
    virtual void UnregisterShmObject(
        BindingType binding_type,
        ServiceElementInstanceIdentifierView service_element_instance_identifier_view) noexcept = 0;

    virtual ResultBlank Trace(const BindingType binding_type,
                              const ServiceElementTracingData service_element_tracing_data,
                              const ServiceElementInstanceIdentifierView service_element_instance_identifier,
                              const TracePointType trace_point_type,
                              const TracePointDataId trace_point_data_id,
                              TypeErasedSamplePtr sample_ptr,
                              const void* const shm_data_ptr,
                              const std::size_t shm_data_size) noexcept = 0;

    virtual ResultBlank Trace(const BindingType binding_type,
                              const ServiceElementInstanceIdentifierView service_element_instance_identifier,
                              const TracePointType trace_point_type,
                              const score::cpp::optional<TracePointDataId> trace_point_data_id,
                              const void* const local_data_ptr,
                              const std::size_t local_data_size) noexcept = 0;

  protected:
    ITracingRuntime(ITracingRuntime&&) noexcept = default;
    ITracingRuntime& operator=(ITracingRuntime&&) noexcept = default;
    ITracingRuntime(const ITracingRuntime&) noexcept = default;
    ITracingRuntime& operator=(const ITracingRuntime&) noexcept = default;
};

}  // namespace score::mw::com::impl::tracing

#endif  // SCORE_MW_COM_IMPL_TRACING_I_TRACING_RUNTIME_H
