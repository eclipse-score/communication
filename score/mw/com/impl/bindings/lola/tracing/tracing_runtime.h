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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_TRACING_TRACING_RUNTIME_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_TRACING_TRACING_RUNTIME_H

#include "score/mw/com/impl/configuration/configuration.h"
#include "score/mw/com/impl/tracing/configuration/service_element_instance_identifier_view.h"
#include "score/mw/com/impl/tracing/configuration/service_element_type.h"
#include "score/mw/com/impl/tracing/i_tracing_runtime_binding.h"
#include "score/mw/com/impl/tracing/service_element_tracing_data.h"

#include "score/language/safecpp/scoped_function/scope.h"
#include "score/containers/dynamic_array.h"
#include "score/memory/shared/i_shared_memory_resource.h"

#include <score/optional.hpp>
#include <score/string_view.hpp>

#include <mutex>
#include <unordered_map>
#include <utility>

namespace score::mw::com::impl::lola::tracing
{

class TracingRuntime : public impl::tracing::ITracingRuntimeBinding
{
    // friend to test class; it's used to check type_erased_sample_ptrs_
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class TracingRuntimeAttorney;

  public:
    constexpr static score::cpp::string_view kDummyElementNameForShmRegisterCallback{"DUMMY_ELEMENT_NAME"};
    // Suppress "AUTOSAR C++14 A0-1-1", The rule states: "A project shall not contain instances of non-volatile
    // variables being given values that are not subsequently used"
    // Variable is used in the implementation(.cpp) file.
    // coverity[autosar_cpp14_a0_1_1_violation : FALSE]
    constexpr static impl::tracing::ServiceElementType kDummyElementTypeForShmRegisterCallback{
        impl::tracing::ServiceElementType::EVENT};

    /// \brief Constructor
    /// \param number_of_needed_tracing_slots The maximum number of tracing slots that will be required.
    ///        Used to set the capacity of type_erased_sample_ptrs_.
    ///        Each service element that requires tracing, will register as many tracing slots as specified in the
    ///        configuration by the numberOfIpcTracingSlots variable. Then the number_of_needed_tracing_slots is the sum
    ///        of numberOfIpcTracingSlots for each service element. This registration is done via
    ///        RegisterServiceElement(number_of_ipc_tracing_slots) call.
    TracingRuntime(const SamplePointerIndex number_of_needed_tracing_slots,
                   const Configuration& configuration) noexcept;

    ~TracingRuntime() noexcept override = default;

    TracingRuntime(const TracingRuntime&) = delete;

    TracingRuntime(TracingRuntime&&) noexcept = delete;

    TracingRuntime& operator=(const TracingRuntime&) = delete;

    TracingRuntime& operator=(TracingRuntime&&) noexcept = delete;

    /// \brief This function registers a range of tracing_slots for the current service element. It returns the
    /// information on where the registered range starts and how long it is.
    /// This information can be passed to EmplaceTypeErasedSamplePtr which will find the next free slot in
    /// the  range, set a sample pointer at that location and return the trace_context_id (which is the index
    /// of the location).
    impl::tracing::ServiceElementTracingData RegisterServiceElement(
        TracingSlotSizeType number_of_ipc_tracing_slots) noexcept override;

    bool RegisterWithGenericTraceApi() noexcept override;

    analysis::tracing::TraceClientId GetTraceClientId() const noexcept override
    {
        return trace_client_id_.value();
    }

    void SetDataLossFlag(const bool new_value) noexcept override
    {
        data_loss_flag_ = new_value;
    }

    bool GetDataLossFlag() const noexcept override
    {
        return data_loss_flag_;
    }

    void RegisterShmObject(
        const impl::tracing::ServiceElementInstanceIdentifierView& service_element_instance_identifier_view,
        const analysis::tracing::ShmObjectHandle shm_object_handle,
        void* const shm_memory_start_address) noexcept override;

    void UnregisterShmObject(const impl::tracing::ServiceElementInstanceIdentifierView&
                                 service_element_instance_identifier_view) noexcept override;

    score::cpp::optional<analysis::tracing::ShmObjectHandle> GetShmObjectHandle(
        const impl::tracing::ServiceElementInstanceIdentifierView& service_element_instance_identifier_view)
        const noexcept override;

    score::cpp::optional<void*> GetShmRegionStartAddress(const impl::tracing::ServiceElementInstanceIdentifierView&
                                                      service_element_instance_identifier_view) const noexcept override;

    void CacheFileDescriptorForReregisteringShmObject(
        const impl::tracing::ServiceElementInstanceIdentifierView& service_element_instance_identifier_view,
        const memory::shared::ISharedMemoryResource::FileDescriptor shm_file_descriptor,
        void* const shm_memory_start_address) noexcept override;

    score::cpp::optional<std::pair<memory::shared::ISharedMemoryResource::FileDescriptor, void*>>
    GetCachedFileDescriptorForReregisteringShmObject(
        const impl::tracing::ServiceElementInstanceIdentifierView& service_element_instance_identifier_view)
        const noexcept override;

    void ClearCachedFileDescriptorForReregisteringShmObject(
        const impl::tracing::ServiceElementInstanceIdentifierView& service_element_instance_identifier_view) noexcept
        override;

    analysis::tracing::ServiceInstanceElement ConvertToTracingServiceInstanceElement(
        const impl::tracing::ServiceElementInstanceIdentifierView service_element_instance_identifier_view)
        const noexcept override;

    /// \brief EmplaceTypeErasedSamplePtr takes a ServiceElementTracingData and returns a TraceContextId, which allows
    /// to correctly retrieve the sample ptr again. Discarding this value will make it impossible to free the sample
    /// pointer correctly. If no slots are left for the service element then no TraceContextId can be returned, thus the
    /// function returns an empty optional.
    [[nodiscard]] std::optional<TraceContextId> EmplaceTypeErasedSamplePtr(
        impl::tracing::TypeErasedSamplePtr type_erased_sample_ptr,
        const impl::tracing::ServiceElementTracingData service_element_tracing_data) noexcept override;

    void ClearTypeErasedSamplePtr(const TraceContextId trace_context_id) noexcept override;

    bool IsTracingSlotUsed(const TraceContextId trace_context_id) noexcept;

  private:
    /// \brief Helper struct which contains an optional sample_ptr and a mutex which is used to protect access to the
    ///        sample_ptr. A struct is used instead of a std::pair to make the code more explicit when accessing the
    ///        elements.
    struct TypeErasedSamplePtrWithMutex
    {
        // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
        // be private.". We need these data elements to be organized into a coherent organized data structure.
        // coverity[autosar_cpp14_m11_0_1_violation]
        score::cpp::optional<impl::tracing::TypeErasedSamplePtr> sample_ptr;
        // coverity[autosar_cpp14_m11_0_1_violation]
        std::mutex mutex;
    };

    std::optional<TraceContextId> GetTraceContextId(
        const impl::tracing::ServiceElementTracingData& service_element_tracing_data) noexcept;

    const Configuration& configuration_;
    score::cpp::optional<analysis::tracing::TraceClientId> trace_client_id_;
    bool data_loss_flag_;

    /// \brief Array of type erased sample pointers containing one element per service element that registers itself via
    ///        RegisterServiceElement.
    ///
    /// Since the array is of fixed size, we can insert new elements and read other elements at the same time
    /// without synchronisation. However, operations on individual elements must be protected by a mutex.
    score::containers::DynamicArray<TypeErasedSamplePtrWithMutex> type_erased_sample_ptrs_;

    /// \brief Index in type_erased_sample_ptrs_. This is the index directly after the index, where the range of last
    /// service element that was registered via RegisterServiceElement, ends.
    SamplePointerIndex next_available_position_for_new_service_element_range_start_;

    std::unordered_map<impl::tracing::ServiceElementInstanceIdentifierView,
                       std::pair<analysis::tracing::ShmObjectHandle, void*>>
        shm_object_handle_map_;
    std::unordered_map<impl::tracing::ServiceElementInstanceIdentifierView,
                       std::pair<memory::shared::ISharedMemoryResource::FileDescriptor, void*>>
        failed_shm_object_registration_cache_;

    /// \brief Ensure that the associated scoped function called only as long as the scope is not expired.
    /// \details The scope is used for the callback registered with RegisterTraceDoneCB.
    safecpp::Scope<> receive_handler_scope_;
};

}  // namespace score::mw::com::impl::lola::tracing

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_TRACING_TRACING_RUNTIME_H
