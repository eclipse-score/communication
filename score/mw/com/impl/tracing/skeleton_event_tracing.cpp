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
#include "score/mw/com/impl/tracing/skeleton_event_tracing.h"

#include "score/mw/com/impl/bindings/lola/transaction_log_set.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/impl/service_element_type.h"
#include "score/mw/com/impl/tracing/common_event_tracing.h"
#include "score/mw/com/impl/tracing/configuration/proxy_event_trace_point_type.h"
#include "score/mw/com/impl/tracing/configuration/proxy_field_trace_point_type.h"
#include "score/mw/com/impl/tracing/configuration/service_element_instance_identifier_view.h"
#include "score/mw/com/impl/tracing/configuration/skeleton_event_trace_point_type.h"
#include "score/mw/com/impl/tracing/configuration/skeleton_field_trace_point_type.h"
#include "score/mw/com/impl/tracing/configuration/tracing_filter_config.h"
#include "score/mw/com/impl/tracing/skeleton_event_tracing_data.h"
#include "score/mw/com/impl/tracing/trace_error.h"

#include <score/assert.hpp>

#include <exception>

namespace score::mw::com::impl::tracing
{

namespace
{

class TracingData
{
  public:
    // Suppress "AUTOSAR C++14 M11-0-1" rule finding. This rule states: "Member data in non-POD class types shall be
    // private.". There is no need for too much overhead of having getter and setter for the private members, and
    // nothing violated by defining it as public.
    // coverity[autosar_cpp14_m11_0_1_violation]
    impl::tracing::ITracingRuntime::TracePointDataId trace_point_data_id{};
    // coverity[autosar_cpp14_m11_0_1_violation]
    const std::pair<const void*, std::size_t> shm_data_chunk{};
};

void UpdateTracingDataFromTraceResult(const Result<void> trace_result,
                                      SkeletonEventTracingData& skeleton_event_tracing_data,
                                      bool& skeleton_event_trace_point)
{
    if (!trace_result.has_value())
    {
        if (trace_result.error() == TraceErrorCode::TraceErrorDisableTracePointInstance)
        {
            skeleton_event_trace_point = false;
        }
        else if (trace_result.error() == TraceErrorCode::TraceErrorDisableAllTracePoints)
        {
            DisableAllTracePoints(skeleton_event_tracing_data);
        }
        else
        {
            ::score::mw::log::LogError("lola")
                << "Unexpected error received from trace call:" << trace_result.error() << ". Ignoring.";
        }
    }
}

template <ServiceElementType service_element_type>
std::uint8_t GetNumberOfTracingSlots(const InstanceIdentifier& instance_identifier,
                                     std::string_view service_element_name)
{
    static_assert(service_element_type != ServiceElementType::INVALID);

    const auto instance_identifier_view = InstanceIdentifierView(instance_identifier);
    const auto& service_instance_deployment = instance_identifier_view.GetServiceInstanceDeployment();
    const auto& lola_service_instance_deployment = [&service_instance_deployment]() {
        if (const auto* val = std::get_if<LolaServiceInstanceDeployment>(&service_instance_deployment.bindingInfo_))
        {
            return *val;
        }
        mw::log::LogFatal("lola")
            << "While getting number of tracing slots, a bad variant access was made. Provided service instance "
               "deployment, does not hold LolaServiceInstanceDeploymentType. Terminating.";
        std::terminate();
    }();

    const auto& service_instance_map = [&lola_service_instance_deployment]() constexpr {
        // Deviation from equivalent Rules A7-1-8 and  M6-4-1:
        // - A non-type specifier shall be placed before a type specifier in a declaration.
        // - An if ( condition ) construct shall be followed by a compound statement.
        // Justification:
        // - This is a false positive because "if constexpr" is a valid statement since C++17.
        // coverity[autosar_cpp14_a7_1_8_violation : FALSE]
        // coverity[autosar_cpp14_m6_4_1_violation : FALSE]
        if constexpr (service_element_type == ServiceElementType::EVENT)
        {
            return lola_service_instance_deployment.events_;
        }
        // coverity[autosar_cpp14_a7_1_8_violation : FALSE]
        // coverity[autosar_cpp14_m6_4_1_violation : FALSE]
        else if constexpr (service_element_type == ServiceElementType::FIELD)
        {
            return lola_service_instance_deployment.fields_;
        }
        // LCOV_EXCL_START: Defensive programming: This state will be unreachable since service_element_type must be an
        // EVENT or FIELD (we have a static_assert at the start of this function).
        else
        {
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(0);
        }
    }();

    const std::string service_element_name_str{service_element_name};
    const auto service_element_instance_deployment_it = service_instance_map.find(service_element_name_str);
    if (service_element_instance_deployment_it == service_instance_map.end())
    {
        score::mw::log::LogFatal() << "Lola: requested service element (" << service_element_name
                                   << ") does not exist.";
        std::terminate();
    }

    const auto& service_element_instance_deployment = service_element_instance_deployment_it->second;
    // coverity[autosar_cpp14_a7_1_8_violation : FALSE]
    // coverity[autosar_cpp14_m6_4_1_violation : FALSE]
    const auto slots_per_tracing_point = [&service_element_instance_deployment]() {
        if constexpr (service_element_type == ServiceElementType::EVENT)
        {
            return service_element_instance_deployment.GetNumberOfTracingSlots();
        }
        else
        {
            return service_element_instance_deployment.lola_event_instance_deployment_.GetNumberOfTracingSlots();
        }
    }();

    return slots_per_tracing_point;
}

// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall
// not be called implicitly.". std::visit Throws std::bad_variant_access if
// as-variant(vars_i).valueless_by_exception() is true for any variant vars_i in vars. The variant may only become
// valueless if an exception is thrown during different stages. Since we don't throw exceptions, it's not possible
// that the variant can return true from valueless_by_exception and therefore not possible that std::visit throws
// an exception.
// This suppression should be removed after fixing [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
TracingData ExtractBindingTracingData(const impl::SampleAllocateePtr<void>& sample_data_ptr,
                                      memory::DataTypeSizeInfo sample_type_size_info)
{
    const auto& binding_ptr_variant = SampleAllocateePtrView{sample_data_ptr}.GetUnderlyingVariant();
    auto visitor = score::cpp::overload(
        [&sample_type_size_info](const lola::SampleAllocateePtr& lola_ptr) -> TracingData {
            const lola::EventDataControlComposite<>& event_data_control_composite =
                lola::SampleAllocateePtrView{lola_ptr}.GetEventDataControlComposite();
            const auto referenced_slot = lola_ptr.GetReferencedSlot();
            const auto sample_timestamp = event_data_control_composite.GetEventSlotTimestamp(referenced_slot);
            static_assert(
                sizeof(lola::EventSlotStatus::EventTimeStamp) ==
                    sizeof(impl::tracing::ITracingRuntime::TracePointDataId),
                "Event timestamp is used for the trace point data id, therefore, the types should be the same.");

            const auto trace_point_data_id =
                static_cast<impl::tracing::ITracingRuntime::TracePointDataId>(sample_timestamp);

            return {trace_point_data_id, {lola_ptr.get(), sample_type_size_info.Size()}};
        },
        // Suppress "AUTOSAR C++14 A8-4-12" rule finding. This rule states: "A std::unique_ptr shall be passed to a
        // function as: (1) a copy to express the function assumes ownership (2) an lvalue reference to express that
        // the function replaces the managed object".
        // Here we can't use a raw pointer / reference since we're using score::cpp::overload, and the function is not
        // replaceing the managed object, so this should be a const reference.
        // coverity[autosar_cpp14_a8_4_12_violation]
        [&sample_type_size_info](const mock_binding::SampleAllocateePtr& ptr) -> TracingData {
            return {0U, {ptr.get(), sample_type_size_info.Size()}};
        },
        [](const score::cpp::blank&) -> TracingData {
            std::terminate();
        });
    return std::visit(visitor, binding_ptr_variant);
}

// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall
// not be called implicitly.". std::visit Throws std::bad_variant_access if
// as-variant(vars_i).valueless_by_exception() is true for any variant vars_i in vars. The variant may only become
// valueless if an exception is thrown during different stages. Since we don't throw exceptions, it's not possible
// that the variant can return true from valueless_by_exception and therefore not possible that std::visit throws
// an exception.
// This suppression should be removed after fixing [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
TypeErasedSamplePtr CreateTypeErasedSamplePtr(impl::SampleAllocateePtr<void>& sample_data_ptr)
{
    auto& binding_ptr_variant = SampleAllocateePtrMutableView{sample_data_ptr}.GetUnderlyingVariant();
    auto visitor = score::cpp::overload(
        [](lola::SampleAllocateePtr& lola_ptr) -> TypeErasedSamplePtr {
            lola::ConsumerEventDataControlLocalView<>& consumer_event_data_control_local =
                lola::SampleAllocateePtrMutableView{lola_ptr}.GetConsumerEventDataControlLocalView();

            const auto event_slot_index = lola_ptr.GetReferencedSlot();
            consumer_event_data_control_local.ReferenceSpecificEvent(event_slot_index);
            const auto* const managed_object =
                static_cast<const void*>(lola::SampleAllocateePtrView{lola_ptr}.GetManagedObject());

            lola::SamplePtr<void> sample_ptr{managed_object, consumer_event_data_control_local, event_slot_index};
            return impl::tracing::TypeErasedSamplePtr{std::move(sample_ptr)};
        },
        [](mock_binding::SampleAllocateePtr& ptr) -> TypeErasedSamplePtr {
            impl::tracing::TypeErasedSamplePtr type_erased_sample_ptr{
                mock_binding::SamplePtr<void>{ptr.get(), [](void*) noexcept {}}};
            return type_erased_sample_ptr;
        },
        // LCOV_EXCL_START (Defensive programming: CreateTypeErasedSamplePtr is always called after
        // ExtractBindingTracingData. If the SampleAllocateePtr contains a blank binding, then ExtractBindingTracingData
        // will terminate. Therefore, we will never reach this branch.
        [](score::cpp::blank&) -> TypeErasedSamplePtr {
            std::terminate();
        });
    // LCOV_EXCL_STOP

    return std::visit(visitor, binding_ptr_variant);
}

}  // namespace

// Suppress "AUTOSAR C++14 A3-1-1", The rule states: "It shall be possible to include any header file
// in multiple translation units without violating the One Definition Rule."
// This is false positive. Function is declared only once.
// coverity[autosar_cpp14_a3_1_1_violation]
SkeletonEventTracingData GenerateSkeletonTracingStructFromEventConfig(const InstanceIdentifier& instance_identifier,
                                                                      const BindingType binding_type,
                                                                      const std::string_view event_name)
{
    auto& runtime = Runtime::getInstance();
    auto* const tracing_runtime = runtime.GetTracingRuntime();
    // Suppress "AUTOSAR C++14 M8-5-2" rule finding. This rule declares: "Braces shall be used to indicate and match
    // the structure in the non-zero initialization of arrays and structures"
    // False positive: AUTOSAR C++14 M-8-5-2 refers to MISRA C++:2008 8-5-2 allows top level brace initialization.
    // We want to make sure that default initialization is always performed.
    // coverity[autosar_cpp14_m8_5_2_violation : FALSE]
    SkeletonEventTracingData skeleton_event_tracing_data{};
    const bool is_tracing_globally_enabled = ((tracing_runtime != nullptr) && (tracing_runtime->IsTracingEnabled()));

    // in case tracing is globally disabled, this will never switch back to enable. Thus, we work with default
    // initialized skeleton_event_tracing_data, which has all trace-points disabled. Only if is_tracing_globally_enabled
    // we specifically initialize skeleton_event_tracing_data.
    if (is_tracing_globally_enabled)
    {
        const auto* const tracing_config = runtime.GetTracingFilterConfig();
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(tracing_config != nullptr,
                                                    "tracing filter config must exist, when tracing runtime exists!");
        const auto service_element_instance_identifier_view =
            GetServiceElementInstanceIdentifierView(instance_identifier, event_name, ServiceElementType::EVENT);
        const auto instance_specifier_view = service_element_instance_identifier_view.instance_specifier;
        const auto service_type =
            service_element_instance_identifier_view.service_element_identifier_view.service_type_name;
        skeleton_event_tracing_data.service_element_instance_identifier_view = service_element_instance_identifier_view;

        skeleton_event_tracing_data.enable_send = tracing_config->IsTracePointEnabled(
            service_type, event_name, instance_specifier_view, SkeletonEventTracePointType::SEND);
        skeleton_event_tracing_data.enable_send_with_allocate = tracing_config->IsTracePointEnabled(
            service_type, event_name, instance_specifier_view, SkeletonEventTracePointType::SEND_WITH_ALLOCATE);

        // only register this service element at Runtime, in case TraceDoneCB relevant trace-point are enabled:
        const auto isTraceDoneCallbackNeeded =
            skeleton_event_tracing_data.enable_send || skeleton_event_tracing_data.enable_send_with_allocate;
        if (isTraceDoneCallbackNeeded)
        {
            auto number_of_tracing_slots =
                GetNumberOfTracingSlots<ServiceElementType::EVENT>(instance_identifier, event_name);
            const auto service_element_tracing_data =
                tracing_runtime->RegisterServiceElement(binding_type, number_of_tracing_slots);
            skeleton_event_tracing_data.service_element_tracing_data = service_element_tracing_data;
        }
    }
    return skeleton_event_tracing_data;
}

// Suppress "AUTOSAR C++14 A3-1-1", The rule states: "It shall be possible to include any header file
// in multiple translation units without violating the One Definition Rule."
// This is false positive. Function is declared only once.
// coverity[autosar_cpp14_a3_1_1_violation]
SkeletonEventTracingData GenerateSkeletonTracingStructFromFieldConfig(const InstanceIdentifier& instance_identifier,
                                                                      const BindingType binding_type,
                                                                      const std::string_view field_name)
{
    auto& runtime = Runtime::getInstance();
    const auto* const tracing_config = runtime.GetTracingFilterConfig();
    auto* const tracing_runtime = runtime.GetTracingRuntime();
    // Suppress "AUTOSAR C++14 M8-5-2" rule finding. This rule declares: "Braces shall be used to indicate and match
    // the structure in the non-zero initialization of arrays and structures"
    // False positive: AUTOSAR C++14 M-8-5-2 refers to MISRA C++:2008 8-5-2 allows top level brace initialization.
    // We want to make sure that default initialization is always performed.
    // coverity[autosar_cpp14_m8_5_2_violation : FALSE]
    SkeletonEventTracingData skeleton_event_tracing_data{};
    const bool is_tracing_globally_enabled = ((tracing_runtime != nullptr) && (tracing_runtime->IsTracingEnabled()));

    // in case tracing is globally disabled, this will never switch back to enable. Thus, we work with default
    // initialized skeleton_event_tracing_data, which has all trace-points disabled. Only if is_tracing_globally_enabled
    // we specifically initialize skeleton_event_tracing_data.
    if (is_tracing_globally_enabled)
    {
        const auto service_element_instance_identifier_view =
            GetServiceElementInstanceIdentifierView(instance_identifier, field_name, ServiceElementType::FIELD);
        const auto instance_specifier_view = service_element_instance_identifier_view.instance_specifier;
        const auto service_type =
            service_element_instance_identifier_view.service_element_identifier_view.service_type_name;
        skeleton_event_tracing_data.service_element_instance_identifier_view = service_element_instance_identifier_view;

        skeleton_event_tracing_data.enable_send = tracing_config->IsTracePointEnabled(
            service_type, field_name, instance_specifier_view, SkeletonFieldTracePointType::UPDATE);
        skeleton_event_tracing_data.enable_send_with_allocate = tracing_config->IsTracePointEnabled(
            service_type, field_name, instance_specifier_view, SkeletonFieldTracePointType::UPDATE_WITH_ALLOCATE);

        // only register this service element at Runtime, in case TraceDoneCB relevant trace-point are enabled:
        const auto isTraceDoneCallbackNeeded =
            skeleton_event_tracing_data.enable_send || skeleton_event_tracing_data.enable_send_with_allocate;
        if (isTraceDoneCallbackNeeded)
        {
            auto number_of_tracing_slots =
                GetNumberOfTracingSlots<ServiceElementType::FIELD>(instance_identifier, field_name);
            const auto service_element_tracing_data =
                tracing_runtime->RegisterServiceElement(binding_type, number_of_tracing_slots);
            skeleton_event_tracing_data.service_element_tracing_data = service_element_tracing_data;
        }
    }
    return skeleton_event_tracing_data;
}

auto CreateTracingSendCallback(SkeletonEventTracingData& skeleton_event_tracing_data,
                               memory::DataTypeSizeInfo skeleton_event_size_info,
                               const SkeletonEventBinding& skeleton_event_binding)
    -> std::optional<typename SkeletonEventBinding::SendTraceCallback>
{
    std::optional<typename SkeletonEventBinding::SendTraceCallback> tracing_handler{};
    if (skeleton_event_tracing_data.enable_send)
    {
        tracing_handler = [&skeleton_event_tracing_data, skeleton_event_size_info, &skeleton_event_binding](
                              impl::SampleAllocateePtr<void>& sample_data_ptr) mutable {
            TraceSend(skeleton_event_tracing_data, skeleton_event_size_info, skeleton_event_binding, sample_data_ptr);
        };
    }
    return tracing_handler;
}

auto CreateTracingSendWithAllocateCallback(SkeletonEventTracingData& skeleton_event_tracing_data,
                                           memory::DataTypeSizeInfo skeleton_event_size_info,
                                           const SkeletonEventBinding& skeleton_event_binding)
    -> std::optional<typename SkeletonEventBinding::SendTraceCallback>
{
    std::optional<typename SkeletonEventBinding::SendTraceCallback> tracing_handler{};
    if (skeleton_event_tracing_data.enable_send_with_allocate)
    {
        tracing_handler = [&skeleton_event_tracing_data, skeleton_event_size_info, &skeleton_event_binding](
                              impl::SampleAllocateePtr<void>& sample_data_ptr) mutable {
            TraceSendWithAllocate(
                skeleton_event_tracing_data, skeleton_event_size_info, skeleton_event_binding, sample_data_ptr);
        };
    }
    return tracing_handler;
}

void TraceSend(SkeletonEventTracingData& skeleton_event_tracing_data,
               memory::DataTypeSizeInfo skeleton_event_size_info,
               const SkeletonEventBinding& skeleton_event_binding_base,
               impl::SampleAllocateePtr<void>& sample_data_ptr)
{
    if (skeleton_event_tracing_data.enable_send)
    {
        const auto service_element_instance_identifier =
            skeleton_event_tracing_data.service_element_instance_identifier_view;
        const auto service_element_type =
            service_element_instance_identifier.service_element_identifier_view.service_element_type;
        tracing::TracingRuntime::TracePointType trace_point{};
        if (service_element_type == ServiceElementType::EVENT)
        {
            trace_point = tracing::SkeletonEventTracePointType::SEND;
        }
        else if (service_element_type == ServiceElementType::FIELD)
        {
            trace_point = tracing::SkeletonFieldTracePointType::UPDATE;
        }
        else
        {
            // Suppress "AUTOSAR C++14 M0-1-1", The rule states: "A project shall not contain unreachable code"
            // This is false positive, the enum has more fields than EVENT and FIELD so we might reach this branch.
            // coverity[autosar_cpp14_m0_1_1_violation : FALSE]
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(false, "Service element type must be EVENT or FIELD");
        }

        const auto tracing_data = ExtractBindingTracingData(sample_data_ptr, skeleton_event_size_info);
        auto type_erased_sample_ptr = CreateTypeErasedSamplePtr(sample_data_ptr);

        const auto binding_type = skeleton_event_binding_base.GetBindingType();
        const auto service_element_tracing_data = skeleton_event_tracing_data.service_element_tracing_data;
        const auto trace_result = TraceShmData(binding_type,
                                               service_element_tracing_data,
                                               service_element_instance_identifier,
                                               trace_point,
                                               tracing_data.trace_point_data_id,
                                               std::move(type_erased_sample_ptr),
                                               tracing_data.shm_data_chunk);
        UpdateTracingDataFromTraceResult(
            trace_result, skeleton_event_tracing_data, skeleton_event_tracing_data.enable_send);
    }
}

void TraceSendWithAllocate(SkeletonEventTracingData& skeleton_event_tracing_data,
                           memory::DataTypeSizeInfo skeleton_event_size_info,
                           const SkeletonEventBinding& skeleton_event_binding_base,
                           impl::SampleAllocateePtr<void>& sample_data_ptr)
{
    if (skeleton_event_tracing_data.enable_send_with_allocate)
    {
        const auto service_element_instance_identifier =
            skeleton_event_tracing_data.service_element_instance_identifier_view;
        const auto service_element_type =
            service_element_instance_identifier.service_element_identifier_view.service_element_type;
        tracing::TracingRuntime::TracePointType trace_point{};
        if (service_element_type == ServiceElementType::EVENT)
        {
            trace_point = tracing::SkeletonEventTracePointType::SEND_WITH_ALLOCATE;
        }
        else if (service_element_type == ServiceElementType::FIELD)
        {
            trace_point = tracing::SkeletonFieldTracePointType::UPDATE_WITH_ALLOCATE;
        }
        else
        {
            // Suppress "AUTOSAR C++14 M0-1-1", The rule states: "A project shall not contain unreachable code"
            // This is false positive, the enum has more fields than EVENT and FIELD so we might reach this branch.
            // coverity[autosar_cpp14_m0_1_1_violation : FALSE]
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(false, "Service element type must be EVENT or FIELD");
        }

        const auto tracing_data = ExtractBindingTracingData(sample_data_ptr, skeleton_event_size_info);
        auto type_erased_sample_ptr = CreateTypeErasedSamplePtr(sample_data_ptr);

        const auto binding_type = skeleton_event_binding_base.GetBindingType();
        const auto service_element_tracing_data = skeleton_event_tracing_data.service_element_tracing_data;
        const auto trace_result = TraceShmData(binding_type,
                                               service_element_tracing_data,
                                               service_element_instance_identifier,
                                               trace_point,
                                               tracing_data.trace_point_data_id,
                                               std::move(type_erased_sample_ptr),
                                               tracing_data.shm_data_chunk);

        UpdateTracingDataFromTraceResult(
            trace_result, skeleton_event_tracing_data, skeleton_event_tracing_data.enable_send_with_allocate);
    }
}

}  // namespace score::mw::com::impl::tracing
