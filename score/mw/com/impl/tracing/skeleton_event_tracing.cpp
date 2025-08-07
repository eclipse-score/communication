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

namespace detail_skeleton_event_tracing
{

void UpdateTracingDataFromTraceResult(const ResultBlank trace_result,
                                      SkeletonEventTracingData& skeleton_event_tracing_data,
                                      bool& skeleton_event_trace_point) noexcept
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

}  // namespace detail_skeleton_event_tracing

namespace
{
template <ServiceElementType service_element_type>
std::uint8_t GetNumberOfTracingSlots(const InstanceIdentifier& instance_identifier,
                                     std::string_view service_element_name) noexcept
{
    const auto instance_identifier_view = InstanceIdentifierView(instance_identifier);
    const auto& service_instance_deployment = instance_identifier_view.GetServiceInstanceDeployment();
    const auto& lola_service_instance_deployment = [&service_instance_deployment]() {
        if (const auto* val = std::get_if<LolaServiceInstanceDeployment>(&service_instance_deployment.bindingInfo_))
        {
            return *val;
        }
        mw::log::LogFatal("lola")
            << "While getting number of tracing slots, a bed variant access was made. Provided service instance "
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
        if constexpr (service_element_type == ServiceElementType::FIELD)
        {
            return lola_service_instance_deployment.fields_;
        }
        // LCOV_EXCL_START: Defensive programming: This state will be unreachable since this is a private function and
        // the calls to it only ever calls it with a ServiceElementType of EVENT or FIELD.
        // Suppress "AUTOSAR C++14 M6-4-1" rule finding. This rule declares: "An if ( condition ) construct shall be
        // followed by a compound statement. The else keyword shall be followed by either a compound statement, or
        // another if statement". This is a false positive because "if constexpr" is a valid statement since C++17.
        // coverity[autosar_cpp14_m6_4_1_violation : FALSE]
        score::mw::log::LogFatal() << "Lola: invalid service element (" << service_element_type << ") provided.";
        std::terminate();
        // LCOV_EXCL_STOP
    }();

    const std::string service_element_name_str{service_element_name};
    const auto service_element_instance_deployment_it = service_instance_map.find(service_element_name_str);
    if (service_element_instance_deployment_it == service_instance_map.end())
    {
        score::mw::log::LogFatal() << "Lola: requested service element (" << service_element_name << ") does not exist.";
        std::terminate();
    }

    const auto& service_element_instance_deployment = service_element_instance_deployment_it->second;
    const auto slots_per_tracing_point = service_element_instance_deployment.GetNumberOfTracingSlots();

    return slots_per_tracing_point;
}

}  // namespace

// Suppress "AUTOSAR C++14 A3-1-1", The rule states: "It shall be possible to include any header file
// in multiple translation units without violating the One Definition Rule."
// This is false positive. Function is declared only once.
// coverity[autosar_cpp14_a3_1_1_violation]
SkeletonEventTracingData GenerateSkeletonTracingStructFromEventConfig(const InstanceIdentifier& instance_identifier,
                                                                      const BindingType binding_type,
                                                                      const score::cpp::string_view event_name) noexcept
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
    if ((tracing_config != nullptr) && (tracing_runtime != nullptr))
    {
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
            auto number_of_tracing_slots = GetNumberOfTracingSlots<ServiceElementType::EVENT>(
                instance_identifier, std::string_view{event_name.data(), event_name.size()});
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
                                                                      const score::cpp::string_view field_name) noexcept
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
    if ((tracing_config != nullptr) && (tracing_runtime != nullptr))
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
            auto number_of_tracing_slots = GetNumberOfTracingSlots<ServiceElementType::FIELD>(
                instance_identifier, std::string_view{field_name.data(), field_name.size()});
            const auto service_element_tracing_data =
                tracing_runtime->RegisterServiceElement(binding_type, number_of_tracing_slots);
            skeleton_event_tracing_data.service_element_tracing_data = service_element_tracing_data;
        }
    }
    return skeleton_event_tracing_data;
}

}  // namespace score::mw::com::impl::tracing
