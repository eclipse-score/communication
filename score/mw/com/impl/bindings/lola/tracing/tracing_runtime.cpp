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
#include "score/mw/com/impl/bindings/lola/tracing/tracing_runtime.h"
#include "score/analysis/tracing/common/types.h"
#include "score/language/safecpp/safe_math/safe_math.h"
#include "score/language/safecpp/scoped_function/scope.h"
#include "score/mw/com/impl/tracing/service_element_tracing_data.h"
#include "score/mw/log/logging.h"

#include <score/assert.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <mutex>
#include <utility>
#include <vector>

namespace score::mw::com::impl::lola::tracing
{

namespace
{

/// \brief Converts a detailed/element specific ServiceElementInstanceIdentifierView used by the binding independent
///        layer into a representation used by the lola binding.
/// \details In the context of shm-object identification, the binding independent layer expects/supports that a shm-
///          capable binding maintains shm-objects per service-element! I.e. a shm-object is identified by a full
///          fledged ServiceElementInstanceIdentifierView. But LoLa only maintains shm-objects on the granularity level
///          of service-instances (aggregating many service elements). So in the LoLa case
///          ServiceElementInstanceIdentifierView::service_element_identifier_view::service_element_name and
///          ServiceElementInstanceIdentifierView::service_element_identifier_view::service_element_type are just
///          an aggregated dummy value!
///          But whenever the upper/binding independent layer makes a lookup for a shm-object on the detailed
///          ServiceElementInstanceIdentifierView (with real/concrete service-element names and types), we have to
///          transform it into the simplified/aggregated ServiceElementInstanceIdentifierView, which LoLa uses ...
/// \param service_element_instance_identifier_view detailed/element specific ServiceElementInstanceIdentifierView used
///         by the binding independent layer
///
/// \return converted LoLa binding specific ServiceElementInstanceIdentifierView to identify a shm-object.
impl::tracing::ServiceElementInstanceIdentifierView ConvertServiceElementInstanceIdentifierViewForLoLaShmIdentification(
    const impl::tracing::ServiceElementInstanceIdentifierView& service_element_instance_identifier_view)
{
    impl::tracing::ServiceElementInstanceIdentifierView simplifiedIdentifier{service_element_instance_identifier_view};
    simplifiedIdentifier.service_element_identifier_view.service_element_name =
        TracingRuntime::kDummyElementNameForShmRegisterCallback;
    simplifiedIdentifier.service_element_identifier_view.service_element_type =
        TracingRuntime::kDummyElementTypeForShmRegisterCallback;
    return simplifiedIdentifier;
}

}  // namespace

TracingRuntime::TracingRuntime(const SamplePointerIndex number_of_needed_tracing_slots,
                               const Configuration& configuration) noexcept
    : impl::tracing::ITracingRuntimeBinding{},
      configuration_{configuration},
      trace_client_id_{},
      data_loss_flag_{false},
      type_erased_sample_ptrs_{number_of_needed_tracing_slots},
      next_available_position_for_new_service_element_range_start_{0U},
      shm_object_handle_map_{},
      failed_shm_object_registration_cache_{},
      receive_handler_scope_{}
{
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from std::unordered_map::at in case the container doesn't have the
// mapped element.
// The service elements is loaded into configuration during initialization.
// coverity[autosar_cpp14_a15_5_3_violation]
bool TracingRuntime::RegisterWithGenericTraceApi() noexcept
{
    const auto app_instance_identifier = configuration_.GetTracingConfiguration().GetApplicationInstanceID();
    std::string app_instance_identifier_string{app_instance_identifier.data(), app_instance_identifier.size()};
    const auto register_client_result = analysis::tracing::GenericTraceAPI::RegisterClient(
        analysis::tracing::BindingType::kLoLa, app_instance_identifier_string);
    if (!register_client_result.has_value())
    {
        score::mw::log::LogError("lola")
            << "Lola TracingRuntime: RegisterClient with the GenericTraceAPI failed with error:"
            << register_client_result.error();
        return false;
    }
    trace_client_id_ = *register_client_result;

    analysis::tracing::TraceDoneCallBackType trace_done_callback{
        receive_handler_scope_,
        // Suppress "AUTOSAR C++14 M3-2-3" rule finding. This rule states: "A type, object or function that is used in
        // multiple translation units shall be declared in one and only one file.".
        // This is false positive. Function is declared only once.
        // coverity[autosar_cpp14_m3_2_3_violation : FALSE]
        [this](analysis::tracing::TraceContextId trace_context_id) noexcept {
            if (!IsTracingSlotUsed(trace_context_id))
            {
                score::mw::log::LogWarn("lola")
                    << "Lola TracingRuntime: TraceDoneCB with TraceContextId" << trace_context_id
                    << "was not pending but has been called anyway. This is expected to occur if the trace done "
                       "callback is called after an event/field has been stop offered. Ignoring callback.";
                return;
            }
            ClearTypeErasedSamplePtr(trace_context_id);
        }};
    // Suppress "AUTOSAR C++14 A15-4-2" rule finding. This rule states: "I a function is declared to be
    // noexcept, noexcept(true) or noexcept(<true condition>), then it shall not exit with an exception"
    // we can't add noexcept to score::cpp::callback signature.
    // coverity[autosar_cpp14_a15_4_2_violation]
    const auto register_trace_done_callback_result =
        // Suppress "AUTOSAR C++14 A5-1-4", The rule states: "A lambda expression object shall not outlive
        // any of its reference-captured objects."
        // The scope of the trace_done_callback which captures 'this' is owned by the TracingRuntime class. Therefore,
        // the scope object will always be destroyed before 'this' is destroyed. The ScopedFunction will prevent the
        // lambda being called after the scope has been destroyed, so the lambda cannot outlive 'this'.
        // coverity[autosar_cpp14_a5_1_4_violation]
        analysis::tracing::GenericTraceAPI::RegisterTraceDoneCB(*trace_client_id_, std::move(trace_done_callback));
    if (!register_trace_done_callback_result.has_value())
    {
        score::mw::log::LogError("lola")
            << "Lola TracingRuntime: RegisterTraceDoneCB with the GenericTraceAPI failed with error: "
            << register_trace_done_callback_result.error();
        return false;
    }
    return true;
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from '.value()' in case it doesn't have value but as we check
// before with 'has_value()' so no way for throwing std::bad_optional_access which leds to std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
impl::tracing::ServiceElementTracingData TracingRuntime::RegisterServiceElement(
    TracingSlotSizeType number_of_ipc_tracing_slots) noexcept
{
    if (number_of_ipc_tracing_slots == 0U)
    {
        score::mw::log::LogFatal("lola")
            << "Value of number_of_ipc_tracing_slots is zero! Requesting zero ipc tracing slots for a trace enabled "
            << "service element makes no sense. Something has gone wrong! Terminating.";
        std::terminate();
    }

    const auto start_of_current_range = next_available_position_for_new_service_element_range_start_;
    const auto end_of_current_range =
        static_cast<std::size_t>(next_available_position_for_new_service_element_range_start_) +
        static_cast<std::size_t>(number_of_ipc_tracing_slots) - 1U;
    if (end_of_current_range >= type_erased_sample_ptrs_.size())
    {
        score::mw::log::LogFatal("lola")
            << "Could not register service element. Space needed to accommodate the requested tracing slots of this "
            << "service element, plus the number of all tracing slots of previously registered service elements "
            << "exceeds the size of the total requested number of tracing slots (" << type_erased_sample_ptrs_.size()
            << "). Terminating.";
        std::terminate();
    }

    const std::size_t start_of_next_range = end_of_current_range + 1U;
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
        start_of_next_range <= std::numeric_limits<SamplePointerIndex>::max(),
        "Previous check checks that end_of_current_range is less than SamplePointerIndex::max(). "
        "Therefore, end_of_current_range + 1U (i.e. start_of_next_range) must be less than or equal to "
        "SamplePointerIndex::max(). We add this check in case type_erased_sample_ptrs_ is "
        "erroneously initialised with a size larger than SamplePointerIndex::max().");

    next_available_position_for_new_service_element_range_start_ = static_cast<SamplePointerIndex>(start_of_next_range);

    return impl::tracing::ServiceElementTracingData{start_of_current_range, number_of_ipc_tracing_slots};
}

void TracingRuntime::RegisterShmObject(
    const impl::tracing::ServiceElementInstanceIdentifierView& service_element_instance_identifier_view,
    const analysis::tracing::ShmObjectHandle shm_object_handle,
    void* const shm_memory_start_address) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
        service_element_instance_identifier_view.service_element_identifier_view.service_element_type ==
            kDummyElementTypeForShmRegisterCallback,
        "Unexpected service_element_type in LoLa TracingRuntime::RegisterShmObject");
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
        service_element_instance_identifier_view.service_element_identifier_view.service_element_name ==
            kDummyElementNameForShmRegisterCallback,
        "Unexpected service_element_name in LoLa TracingRuntime::RegisterShmObject");
    const auto map_value = std::make_pair(shm_object_handle, shm_memory_start_address);
    const auto insert_result =
        shm_object_handle_map_.insert(std::make_pair(service_element_instance_identifier_view, map_value));
    if (!insert_result.second)
    {
        score::mw::log::LogFatal("lola") << "Could not insert shm object handle" << shm_object_handle
                                       << "into map. Terminating.";
        std::terminate();
    }
}

void TracingRuntime::UnregisterShmObject(
    const impl::tracing::ServiceElementInstanceIdentifierView& service_element_instance_identifier_view) noexcept
{
    const auto erase_result = shm_object_handle_map_.erase(service_element_instance_identifier_view);
    if (erase_result == 0U)
    {
        score::mw::log::LogWarn("lola") << "UnregisterShmObject called on non-existing shared memory object. Ignoring.";
    }
}

void TracingRuntime::CacheFileDescriptorForReregisteringShmObject(
    const impl::tracing::ServiceElementInstanceIdentifierView& service_element_instance_identifier_view,
    const memory::shared::ISharedMemoryResource::FileDescriptor shm_file_descriptor,
    void* const shm_memory_start_address) noexcept
{
    const auto map_value = std::make_pair(shm_file_descriptor, shm_memory_start_address);
    const auto insert_result = failed_shm_object_registration_cache_.insert(
        std::make_pair(service_element_instance_identifier_view, map_value));
    if (!insert_result.second)
    {
        score::mw::log::LogFatal("lola") << "Could not insert file descriptor" << shm_file_descriptor
                                       << "for shm object which failed registration into map. Terminating.";
        std::terminate();
    }
}

score::cpp::optional<std::pair<memory::shared::ISharedMemoryResource::FileDescriptor, void*>>
TracingRuntime::GetCachedFileDescriptorForReregisteringShmObject(
    const impl::tracing::ServiceElementInstanceIdentifierView& service_element_instance_identifier_view) const noexcept
{
    const auto find_result = failed_shm_object_registration_cache_.find(service_element_instance_identifier_view);
    if (find_result == failed_shm_object_registration_cache_.end())
    {
        return {};
    }
    return find_result->second;
}

void TracingRuntime::ClearCachedFileDescriptorForReregisteringShmObject(
    const impl::tracing::ServiceElementInstanceIdentifierView& service_element_instance_identifier_view) noexcept
{
    const auto erase_result = failed_shm_object_registration_cache_.erase(service_element_instance_identifier_view);
    if (erase_result == 0U)
    {
        score::mw::log::LogWarn("lola")
            << "ClearCachedFileDescriptorForReregisteringShmObject called on non-existing cached "
               "file descriptor. Ignoring.";
    }
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from std::unordered_map::at in case the container doesn't have the
// mapped element.
// The instance specifier is loaded into the configuration during the initialization.
// coverity[autosar_cpp14_a15_5_3_violation]
analysis::tracing::ServiceInstanceElement TracingRuntime::ConvertToTracingServiceInstanceElement(
    const impl::tracing::ServiceElementInstanceIdentifierView service_element_instance_identifier_view) const noexcept
{
    const auto& service_instance_deployments = configuration_.GetServiceInstances();
    const auto& service_type_deployments = configuration_.GetServiceTypes();

    // @todo: Replace the configuration unordered_maps with maps and use CompareId?
    const auto instance_specifier =
        InstanceSpecifier::Create(service_element_instance_identifier_view.instance_specifier).value();
    // Suppress "AUTOSAR C++14 A15-4-2" rule finding. This rule states: "I a function is declared to be
    // noexcept, noexcept(true) or noexcept(<true condition>), then it shall not exit with an exception"
    // The instance specifier is loaded into the configuration during the initialization, so the container
    // will always have an element with the specified key.
    // coverity[autosar_cpp14_a15_4_2_violation]
    const auto& service_instance_deployment = service_instance_deployments.at(instance_specifier);
    auto* lola_service_instance_deployment =
        std::get_if<LolaServiceInstanceDeployment>(&service_instance_deployment.bindingInfo_);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(lola_service_instance_deployment != nullptr);

    const auto& service_identifier = service_instance_deployment.service_;

    const auto service_type_deployment = service_type_deployments.at(service_identifier);
    const auto* const lola_service_type_deployment =
        std::get_if<LolaServiceTypeDeployment>(&service_type_deployment.binding_info_);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(lola_service_type_deployment != nullptr);

    using ServiceInstanceElement = analysis::tracing::ServiceInstanceElement;
    ServiceInstanceElement output_service_instance_element{};
    const auto service_element_type =
        service_element_instance_identifier_view.service_element_identifier_view.service_element_type;
    const auto service_element_name =
        service_element_instance_identifier_view.service_element_identifier_view.service_element_name;
    if (service_element_type == impl::tracing::ServiceElementType::EVENT)
    {
        const auto lola_event_id = lola_service_type_deployment->events_.at(
            std::string{service_element_name.data(), service_element_name.size()});
        output_service_instance_element.element_id_ = static_cast<ServiceInstanceElement::EventIdType>(lola_event_id);
    }
    else if (service_element_type == impl::tracing::ServiceElementType::FIELD)
    {
        const auto lola_field_id = lola_service_type_deployment->fields_.at(
            std::string{service_element_name.data(), service_element_name.size()});
        output_service_instance_element.element_id_ = static_cast<ServiceInstanceElement::FieldIdType>(lola_field_id);
    }
    else
    {
        score::mw::log::LogFatal("lola") << "Service element type: " << service_element_type
                                       << " is invalid. Terminating.";
        std::terminate();
    }

    output_service_instance_element.service_id_ =
        static_cast<ServiceInstanceElement::ServiceIdType>(lola_service_type_deployment->service_id_);

    if (!lola_service_instance_deployment->instance_id_.has_value())
    {
        score::mw::log::LogFatal("lola")
            << "Tracing should not be done on service element without configured instance ID. Terminating.";
        std::terminate();
    }
    output_service_instance_element.instance_id_ = static_cast<ServiceInstanceElement::InstanceIdType>(
        lola_service_instance_deployment->instance_id_.value().GetId());

    const auto version = ServiceIdentifierTypeView{service_identifier}.GetVersion();
    output_service_instance_element.major_version_ = ServiceVersionTypeView{version}.getMajor();
    output_service_instance_element.minor_version_ = ServiceVersionTypeView{version}.getMinor();
    return output_service_instance_element;
}

score::cpp::optional<analysis::tracing::ShmObjectHandle> TracingRuntime::GetShmObjectHandle(
    const impl::tracing::ServiceElementInstanceIdentifierView& service_element_instance_identifier_view) const noexcept
{
    impl::tracing::ServiceElementInstanceIdentifierView lolaBindingSpecificIdentifier =
        ConvertServiceElementInstanceIdentifierViewForLoLaShmIdentification(service_element_instance_identifier_view);

    const auto find_result = shm_object_handle_map_.find(lolaBindingSpecificIdentifier);
    if (find_result == shm_object_handle_map_.end())
    {
        return {};
    }
    return find_result->second.first;
}

score::cpp::optional<void*> TracingRuntime::GetShmRegionStartAddress(
    const impl::tracing::ServiceElementInstanceIdentifierView& service_element_instance_identifier_view) const noexcept
{
    impl::tracing::ServiceElementInstanceIdentifierView simplifiedIdentifier =
        ConvertServiceElementInstanceIdentifierViewForLoLaShmIdentification(service_element_instance_identifier_view);

    const auto find_result = shm_object_handle_map_.find(simplifiedIdentifier);
    if (find_result == shm_object_handle_map_.end())
    {
        return {};
    }
    return find_result->second.second;
}

bool TracingRuntime::IsTracingSlotUsed(const TraceContextId trace_context_id) noexcept
{
    auto& element = type_erased_sample_ptrs_.at(static_cast<std::size_t>(trace_context_id));
    std::lock_guard<std::mutex> lock(element.mutex);
    return element.sample_ptr.has_value();
}

auto TracingRuntime::GetTraceContextIdsForServiceElement(
    const impl::tracing::ServiceElementTracingData& service_element_tracing_data) noexcept
    -> std::vector<TraceContextId>
{
    const auto range_start = service_element_tracing_data.service_element_range_start;
    const auto range_size = service_element_tracing_data.number_of_service_element_tracing_slots;

    // LCOV_EXCL_START (We don't have the infrastructure to test the failure case of this static assert at compile time.
    // This check is anyway defensive programming to prevent accidental changes that could be made to the code in
    // the future but currently has no way of failing in production.
    static_assert(
        (std::numeric_limits<decltype(range_start)>::max() + std::numeric_limits<decltype(range_size)>::max() <=
         std::numeric_limits<TraceContextId>::max()),
        "If the maximum value of range_start plus the maximum value of range_size can exceed the maximum value a "
        "TraceContextId, then we could get an overflow.");
    // LCOV_EXCL_STOP

    std::vector<TraceContextId> trace_context_ids(range_size);
    for (std::size_t range_index = 0U; range_index < range_size; ++range_index)
    {
        trace_context_ids.push_back(static_cast<TraceContextId>(range_start + range_index));
    }
    return trace_context_ids;
}

auto TracingRuntime::GetTraceContextId(
    const impl::tracing::ServiceElementTracingData& service_element_tracing_data) noexcept
    -> std::optional<TraceContextId>
{
    const auto trace_context_ids = GetTraceContextIdsForServiceElement(service_element_tracing_data);
    for (const auto trace_context_id : trace_context_ids)
    {
        if (!IsTracingSlotUsed(trace_context_id))
        {
            return trace_context_id;
        }
    }

    score::mw::log::LogInfo("lola")
        << "Can not retrieve trace_context_id which is necessary to set type erased sample pointer. All slots"
        << "assigned to this service element are already tracing active. I.e. insufficient tracing slots were "
           "configured."
        << "This happened to the service element with "
        << service_element_tracing_data.number_of_service_element_tracing_slots << " configured slots. "
        << "The range of the service element starts at " << service_element_tracing_data.service_element_range_start
        << ".";
    return {};
}

[[nodiscard]] std::optional<impl::tracing::ITracingRuntimeBinding::TraceContextId>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from '.value()' in case it doesn't have value but as we check
// before with 'has_value()' so no way for throwing std::bad_optional_access which leds to std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
TracingRuntime::EmplaceTypeErasedSamplePtr(
    impl::tracing::TypeErasedSamplePtr type_erased_sample_ptr,
    const impl::tracing::ServiceElementTracingData service_element_tracing_data) noexcept
{
    if (service_element_tracing_data.service_element_range_start >=
        next_available_position_for_new_service_element_range_start_)
    {
        score::mw::log::LogFatal("lola")
            << "Cannot set type erased sample pointer as provided service element with range start at"
            << service_element_tracing_data.service_element_range_start << "was never registered. Terminating.";
        std::terminate();
    }

    const auto trace_context_id = GetTraceContextId(service_element_tracing_data);
    if (!trace_context_id.has_value())
    {
        return {};
    }
    auto& element = type_erased_sample_ptrs_.at(static_cast<std::size_t>(trace_context_id.value()));
    std::lock_guard<std::mutex> lock(element.mutex);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(!element.sample_ptr.has_value());
    element.sample_ptr = std::move(type_erased_sample_ptr);
    return trace_context_id;
}

void TracingRuntime::ClearTypeErasedSamplePtr(const TraceContextId trace_context_id) noexcept
{
    auto& [sample_ptr, mutex] = type_erased_sample_ptrs_.at(static_cast<std::size_t>(trace_context_id));
    std::lock_guard<std::mutex> lock(mutex);
    sample_ptr = {};
}

void TracingRuntime::ClearTypeErasedSamplePtrs(
    const impl::tracing::ServiceElementTracingData& service_element_tracing_data) noexcept
{
    const auto trace_context_ids = GetTraceContextIdsForServiceElement(service_element_tracing_data);
    for (const auto trace_context_id : trace_context_ids)
    {
        ClearTypeErasedSamplePtr(trace_context_id);
    }
}

}  // namespace score::mw::com::impl::lola::tracing
