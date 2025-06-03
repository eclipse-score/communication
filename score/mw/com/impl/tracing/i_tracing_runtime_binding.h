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
#ifndef SCORE_MW_COM_IMPL_TRACING_I_TRACING_RUNTIME_BINDING_H
#define SCORE_MW_COM_IMPL_TRACING_I_TRACING_RUNTIME_BINDING_H

#include "score/mw/com/impl/tracing/configuration/service_element_instance_identifier_view.h"
#include "score/mw/com/impl/tracing/service_element_tracing_data.h"

#include "score/analysis/tracing/library/generic_trace_api/generic_trace_api.h"
#include "score/memory/shared/i_shared_memory_resource.h"
#include "score/mw/com/impl/tracing/type_erased_sample_ptr.h"

#include <score/callback.hpp>
#include <score/optional.hpp>

#include <cstdint>

namespace score::mw::com::impl::tracing
{

class ITracingRuntimeBinding
{
  public:
    using TraceContextId = analysis::tracing::TraceContextId;
    using TracedShmDataCallback = score::cpp::callback<void()>;
    using TracingSlotSizeType = ServiceElementTracingData::TracingSlotSizeType;
    using SamplePointerIndex = ServiceElementTracingData::SamplePointerIndex;

    ITracingRuntimeBinding() noexcept = default;

    virtual ~ITracingRuntimeBinding() noexcept = default;

    /// \brief Registers LoLa service element that will call impl::Runtime::Trace with a ShmDataChunkList with the
    ///        TracingRuntime. Before this service element can be traced, the ServiceElementTracingData has to be
    ///        used to obtain the next free tracing slot (if available), and its associated
    ///        trace_context_id. This trace_context_id can be used by TraceDoneCallback to free the SamplePtr.
    /// \return A struct containing index of the range start position in type_erased_sample_ptrs_, which is
    ///         associated to this service element. And the size of this range. From this struct a trace_context_id
    ///         can be constructed by EmplaceTypeErasedSamplePtr. It should also be used to create the TraceContextId
    ///         which will be passed to an impl::TracingRuntime::Trace() call which will then be used to identify the
    ///         service element in this class.
    ///
    /// This must be called by every LoLa service element that will call impl::Runtime::Trace with a ShmDataChunkList.
    virtual ServiceElementTracingData RegisterServiceElement(
        TracingSlotSizeType number_of_ipc_tracing_slots) noexcept = 0;

    /// \brief Each binding specific tracing runtime represents a distinct client from the perspective of
    ///        GenericTraceAPI. So it registers itself with the GenericTraceAPI, which gets triggered via this method.
    /// \return true in case registering with GenericTraceAPI was successful, false else.
    virtual bool RegisterWithGenericTraceApi() noexcept = 0;

    /// \brief Return trace client id this binding specific tracing runtime got assigned in
    ///        RegisterWithGenericTraceApi()
    /// \return trace client id
    virtual analysis::tracing::TraceClientId GetTraceClientId() const noexcept = 0;

    /// \brief Set data loss flag for the specific binding.
    /// \param new_value
    virtual void SetDataLossFlag(const bool new_value) noexcept = 0;

    /// \brief Read data loss for the specific binding.
    /// \return
    virtual bool GetDataLossFlag() const noexcept = 0;

    /// \brief Register the shm-object, which has been successfully registered at GenericTraceAPI under
    ///        shm_object_handle with binding specific tracing runtime, which relates to/owns this shm-object.
    /// \param service_element_instance_identifier_view
    /// \param shm_object_handle
    /// \param shm_memory_start_address
    virtual void RegisterShmObject(
        const impl::tracing::ServiceElementInstanceIdentifierView& service_element_instance_identifier_view,
        const analysis::tracing::ShmObjectHandle shm_object_handle,
        void* const shm_memory_start_address) noexcept = 0;
    virtual void UnregisterShmObject(const impl::tracing::ServiceElementInstanceIdentifierView&
                                         service_element_instance_identifier_view) noexcept = 0;

    virtual score::cpp::optional<analysis::tracing::ShmObjectHandle> GetShmObjectHandle(
        const impl::tracing::ServiceElementInstanceIdentifierView& service_element_instance_identifier_view)
        const noexcept = 0;
    virtual score::cpp::optional<void*> GetShmRegionStartAddress(
        const impl::tracing::ServiceElementInstanceIdentifierView& service_element_instance_identifier_view)
        const noexcept = 0;

    virtual void CacheFileDescriptorForReregisteringShmObject(
        const impl::tracing::ServiceElementInstanceIdentifierView& service_element_instance_identifier_view,
        const memory::shared::ISharedMemoryResource::FileDescriptor shm_file_descriptor,
        void* const shm_memory_start_address) noexcept = 0;
    virtual score::cpp::optional<std::pair<memory::shared::ISharedMemoryResource::FileDescriptor, void*>>
    GetCachedFileDescriptorForReregisteringShmObject(const impl::tracing::ServiceElementInstanceIdentifierView&
                                                         service_element_instance_identifier_view) const noexcept = 0;
    virtual void ClearCachedFileDescriptorForReregisteringShmObject(
        const impl::tracing::ServiceElementInstanceIdentifierView&
            service_element_instance_identifier_view) noexcept = 0;

    virtual analysis::tracing::ServiceInstanceElement ConvertToTracingServiceInstanceElement(
        const impl::tracing::ServiceElementInstanceIdentifierView service_element_instance_identifier_view) const = 0;

    virtual std::optional<TraceContextId> EmplaceTypeErasedSamplePtr(
        TypeErasedSamplePtr type_erased_sample_ptr,
        const ServiceElementTracingData service_element_tracing_data) noexcept = 0;
    virtual void ClearTypeErasedSamplePtr(const TraceContextId trace_context_id) noexcept = 0;
    virtual void ClearTypeErasedSamplePtrs(
        const impl::tracing::ServiceElementTracingData& service_element_tracing_data) noexcept = 0;

  protected:
    ITracingRuntimeBinding(ITracingRuntimeBinding&&) noexcept = default;
    ITracingRuntimeBinding& operator=(ITracingRuntimeBinding&&) noexcept = default;
    ITracingRuntimeBinding(const ITracingRuntimeBinding&) noexcept = default;
    ITracingRuntimeBinding& operator=(const ITracingRuntimeBinding&) noexcept = default;
};

}  // namespace score::mw::com::impl::tracing

#endif  // SCORE_MW_COM_IMPL_TRACING_I_TRACING_RUNTIME_BINDING_H
