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
#include "score/mw/com/impl/tracing/proxy_event_tracing.h"

#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/impl/tracing/common_event_tracing.h"
#include "score/mw/com/impl/tracing/configuration/proxy_event_trace_point_type.h"
#include "score/mw/com/impl/tracing/configuration/proxy_field_trace_point_type.h"
#include "score/mw/com/impl/tracing/configuration/service_element_instance_identifier_view.h"
#include "score/mw/com/impl/tracing/proxy_event_tracing_data.h"
#include "score/mw/com/impl/tracing/trace_error.h"

#include "score/mw/log/logging.h"

namespace score::mw::com::impl::tracing
{

namespace
{

void UpdateTracingDataFromTraceResult(ResultBlank trace_result,
                                      ProxyEventTracingData& proxy_event_tracing_data,
                                      bool& proxy_event_trace_point) noexcept
{
    if (!trace_result.has_value())
    {
        if (trace_result.error() == TraceErrorCode::TraceErrorDisableTracePointInstance)
        {
            proxy_event_trace_point = false;
        }
        else if (trace_result.error() == TraceErrorCode::TraceErrorDisableAllTracePoints)
        {
            DisableAllTracePoints(proxy_event_tracing_data);
        }
        else
        {
            ::score::mw::log::LogError("lola")
                << "Unexpected error received from trace call:" << trace_result.error() << ". Ignoring.";
        }
    }
}

}  // namespace

// Suppress "AUTOSAR C++14 A3-1-1", The rule states: "It shall be possible to include any header file
// in multiple translation units without violating the One Definition Rule."
// This is false positive. Function is declared only once.
// coverity[autosar_cpp14_a3_1_1_violation]
ProxyEventTracingData GenerateProxyTracingStructFromEventConfig(const InstanceIdentifier& instance_identifier,
                                                                const std::string_view event_name) noexcept
{
    const auto* const tracing_config = Runtime::getInstance().GetTracingFilterConfig();
    ProxyEventTracingData proxy_event_tracing_data{};
    if (tracing_config != nullptr)
    {
        const auto service_element_instance_identifier_view =
            GetServiceElementInstanceIdentifierView(instance_identifier, event_name, ServiceElementType::EVENT);
        const auto instance_specifier_view = service_element_instance_identifier_view.instance_specifier;
        const auto service_type =
            service_element_instance_identifier_view.service_element_identifier_view.service_type_name;

        proxy_event_tracing_data.service_element_instance_identifier_view = service_element_instance_identifier_view;

        proxy_event_tracing_data.enable_subscribe = tracing_config->IsTracePointEnabled(
            service_type, event_name, instance_specifier_view, ProxyEventTracePointType::SUBSCRIBE);
        proxy_event_tracing_data.enable_unsubscribe = tracing_config->IsTracePointEnabled(
            service_type, event_name, instance_specifier_view, ProxyEventTracePointType::UNSUBSCRIBE);
        proxy_event_tracing_data.enable_subscription_state_changed = tracing_config->IsTracePointEnabled(
            service_type, event_name, instance_specifier_view, ProxyEventTracePointType::SUBSCRIBE_STATE_CHANGE);
        proxy_event_tracing_data.enable_set_subcription_state_change_handler =
            tracing_config->IsTracePointEnabled(service_type,
                                                event_name,
                                                instance_specifier_view,
                                                ProxyEventTracePointType::SET_SUBSCRIPTION_STATE_CHANGE_HANDLER);
        proxy_event_tracing_data.enable_unset_subscription_state_change_handler =
            tracing_config->IsTracePointEnabled(service_type,
                                                event_name,
                                                instance_specifier_view,
                                                ProxyEventTracePointType::UNSET_SUBSCRIPTION_STATE_CHANGE_HANDLER);
        proxy_event_tracing_data.enable_call_subscription_state_change_handler =
            tracing_config->IsTracePointEnabled(service_type,
                                                event_name,
                                                instance_specifier_view,
                                                ProxyEventTracePointType::SUBSCRIPTION_STATE_CHANGE_HANDLER_CALLBACK);
        proxy_event_tracing_data.enable_set_receive_handler = tracing_config->IsTracePointEnabled(
            service_type, event_name, instance_specifier_view, ProxyEventTracePointType::SET_RECEIVE_HANDLER);
        proxy_event_tracing_data.enable_unset_receive_handler = tracing_config->IsTracePointEnabled(
            service_type, event_name, instance_specifier_view, ProxyEventTracePointType::UNSET_RECEIVE_HANDLER);
        proxy_event_tracing_data.enable_call_receive_handler = tracing_config->IsTracePointEnabled(
            service_type, event_name, instance_specifier_view, ProxyEventTracePointType::RECEIVE_HANDLER_CALLBACK);
        proxy_event_tracing_data.enable_get_new_samples = tracing_config->IsTracePointEnabled(
            service_type, event_name, instance_specifier_view, ProxyEventTracePointType::GET_NEW_SAMPLES);
        proxy_event_tracing_data.enable_new_samples_callback = tracing_config->IsTracePointEnabled(
            service_type, event_name, instance_specifier_view, ProxyEventTracePointType::GET_NEW_SAMPLES_CALLBACK);
    }
    return proxy_event_tracing_data;
}

// Suppress "AUTOSAR C++14 A3-1-1", The rule states: "It shall be possible to include any header file
// in multiple translation units without violating the One Definition Rule."
// This is false positive. Function is declared only once.
// coverity[autosar_cpp14_a3_1_1_violation]
ProxyEventTracingData GenerateProxyTracingStructFromFieldConfig(const InstanceIdentifier& instance_identifier,
                                                                const std::string_view field_name) noexcept
{
    const auto* const tracing_config = Runtime::getInstance().GetTracingFilterConfig();
    ProxyEventTracingData proxy_event_tracing_data{};
    if (tracing_config != nullptr)
    {
        const auto service_element_instance_identifier_view =
            GetServiceElementInstanceIdentifierView(instance_identifier, field_name, ServiceElementType::FIELD);
        const auto instance_specifier_view = service_element_instance_identifier_view.instance_specifier;
        const auto service_type =
            service_element_instance_identifier_view.service_element_identifier_view.service_type_name;

        proxy_event_tracing_data.service_element_instance_identifier_view = service_element_instance_identifier_view;

        proxy_event_tracing_data.enable_subscribe = tracing_config->IsTracePointEnabled(
            service_type, field_name, instance_specifier_view, ProxyFieldTracePointType::SUBSCRIBE);
        proxy_event_tracing_data.enable_unsubscribe = tracing_config->IsTracePointEnabled(
            service_type, field_name, instance_specifier_view, ProxyFieldTracePointType::UNSUBSCRIBE);
        proxy_event_tracing_data.enable_subscription_state_changed = tracing_config->IsTracePointEnabled(
            service_type, field_name, instance_specifier_view, ProxyFieldTracePointType::SUBSCRIBE_STATE_CHANGE);
        proxy_event_tracing_data.enable_set_subcription_state_change_handler =
            tracing_config->IsTracePointEnabled(service_type,
                                                field_name,
                                                instance_specifier_view,
                                                ProxyFieldTracePointType::SET_SUBSCRIPTION_STATE_CHANGE_HANDLER);
        proxy_event_tracing_data.enable_unset_subscription_state_change_handler =
            tracing_config->IsTracePointEnabled(service_type,
                                                field_name,
                                                instance_specifier_view,
                                                ProxyFieldTracePointType::UNSET_SUBSCRIPTION_STATE_CHANGE_HANDLER);
        proxy_event_tracing_data.enable_call_subscription_state_change_handler =
            tracing_config->IsTracePointEnabled(service_type,
                                                field_name,
                                                instance_specifier_view,
                                                ProxyFieldTracePointType::SUBSCRIPTION_STATE_CHANGE_HANDLER_CALLBACK);
        proxy_event_tracing_data.enable_set_receive_handler = tracing_config->IsTracePointEnabled(
            service_type, field_name, instance_specifier_view, ProxyFieldTracePointType::SET_RECEIVE_HANDLER);
        proxy_event_tracing_data.enable_unset_receive_handler = tracing_config->IsTracePointEnabled(
            service_type, field_name, instance_specifier_view, ProxyFieldTracePointType::UNSET_RECEIVE_HANDLER);
        proxy_event_tracing_data.enable_call_receive_handler = tracing_config->IsTracePointEnabled(
            service_type, field_name, instance_specifier_view, ProxyFieldTracePointType::RECEIVE_HANDLER_CALLBACK);
        proxy_event_tracing_data.enable_get_new_samples = tracing_config->IsTracePointEnabled(
            service_type, field_name, instance_specifier_view, ProxyFieldTracePointType::GET_NEW_SAMPLES);
        proxy_event_tracing_data.enable_new_samples_callback = tracing_config->IsTracePointEnabled(
            service_type, field_name, instance_specifier_view, ProxyFieldTracePointType::GET_NEW_SAMPLES_CALLBACK);
    }
    return proxy_event_tracing_data;
}

void TraceSubscribe(ProxyEventTracingData& proxy_event_tracing_data,
                    const ProxyEventBindingBase& proxy_event_binding_base,
                    const std::size_t max_sample_count) noexcept
{
    if (proxy_event_tracing_data.enable_subscribe)
    {
        const auto service_element_instance_identifier =
            proxy_event_tracing_data.service_element_instance_identifier_view;
        const auto service_element_type =
            service_element_instance_identifier.service_element_identifier_view.service_element_type;

        TracingRuntime::TracePointType trace_point{};
        if (service_element_type == ServiceElementType::EVENT)
        {
            trace_point = ProxyEventTracePointType::SUBSCRIBE;
        }
        else if (service_element_type == ServiceElementType::FIELD)
        {
            trace_point = ProxyFieldTracePointType::SUBSCRIBE;
        }
        else
        {
            // Suppress "AUTOSAR C++14 M0-1-1", The rule states: "A project shall not contain unreachable code"
            // This is false positive, the enum has more fields than EVENT and FIELD so we might reach this branch.
            // coverity[autosar_cpp14_m0_1_1_violation : FALSE]
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(false, "Service element type must be EVENT or FIELD");
        }

        const auto binding_type = proxy_event_binding_base.GetBindingType();
        const auto trace_result = TraceData(
            service_element_instance_identifier, trace_point, binding_type, ConvertToFatPointer(max_sample_count));
        UpdateTracingDataFromTraceResult(
            trace_result, proxy_event_tracing_data, proxy_event_tracing_data.enable_subscribe);
    }
}

void TraceUnsubscribe(ProxyEventTracingData& proxy_event_tracing_data,
                      const ProxyEventBindingBase& proxy_event_binding_base) noexcept
{
    if (proxy_event_tracing_data.enable_unsubscribe)
    {
        const auto service_element_instance_identifier =
            proxy_event_tracing_data.service_element_instance_identifier_view;
        const auto service_element_type =
            service_element_instance_identifier.service_element_identifier_view.service_element_type;

        TracingRuntime::TracePointType trace_point{};
        if (service_element_type == ServiceElementType::EVENT)
        {
            trace_point = ProxyEventTracePointType::UNSUBSCRIBE;
        }
        else if (service_element_type == ServiceElementType::FIELD)
        {
            trace_point = ProxyFieldTracePointType::UNSUBSCRIBE;
        }
        else
        {
            // Suppress "AUTOSAR C++14 M0-1-1", The rule states: "A project shall not contain unreachable code"
            // This is false positive, the enum has more fields than EVENT and FIELD so we might reach this branch.
            // coverity[autosar_cpp14_m0_1_1_violation : FALSE]
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(false, "Service element type must be EVENT or FIELD");
        }

        const auto binding_type = proxy_event_binding_base.GetBindingType();
        const auto trace_result =
            TraceData(proxy_event_tracing_data.service_element_instance_identifier_view, trace_point, binding_type);
        UpdateTracingDataFromTraceResult(
            trace_result, proxy_event_tracing_data, proxy_event_tracing_data.enable_unsubscribe);
    }
}

void TraceSetReceiveHandler(ProxyEventTracingData& proxy_event_tracing_data,
                            const ProxyEventBindingBase& proxy_event_binding_base) noexcept
{
    if (proxy_event_tracing_data.enable_set_receive_handler)
    {
        const auto service_element_instance_identifier =
            proxy_event_tracing_data.service_element_instance_identifier_view;
        const auto service_element_type =
            service_element_instance_identifier.service_element_identifier_view.service_element_type;

        TracingRuntime::TracePointType trace_point{};
        if (service_element_type == ServiceElementType::EVENT)
        {
            trace_point = ProxyEventTracePointType::SET_RECEIVE_HANDLER;
        }
        else if (service_element_type == ServiceElementType::FIELD)
        {
            trace_point = ProxyFieldTracePointType::SET_RECEIVE_HANDLER;
        }
        else
        {
            // Suppress "AUTOSAR C++14 M0-1-1", The rule states: "A project shall not contain unreachable code"
            // This is false positive, the enum has more fields than EVENT and FIELD so we might reach this branch.
            // coverity[autosar_cpp14_m0_1_1_violation : FALSE]
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(false, "Service element type must be EVENT or FIELD");
        }

        const auto binding_type = proxy_event_binding_base.GetBindingType();
        const auto trace_result =
            TraceData(proxy_event_tracing_data.service_element_instance_identifier_view, trace_point, binding_type);
        UpdateTracingDataFromTraceResult(
            trace_result, proxy_event_tracing_data, proxy_event_tracing_data.enable_set_receive_handler);
    }
}

void TraceUnsetReceiveHandler(ProxyEventTracingData& proxy_event_tracing_data,
                              const ProxyEventBindingBase& proxy_event_binding_base) noexcept
{
    if (proxy_event_tracing_data.enable_unset_receive_handler)
    {
        const auto service_element_instance_identifier =
            proxy_event_tracing_data.service_element_instance_identifier_view;
        const auto service_element_type =
            service_element_instance_identifier.service_element_identifier_view.service_element_type;

        TracingRuntime::TracePointType trace_point{};
        if (service_element_type == ServiceElementType::EVENT)
        {
            trace_point = ProxyEventTracePointType::UNSET_RECEIVE_HANDLER;
        }
        else if (service_element_type == ServiceElementType::FIELD)
        {
            trace_point = ProxyFieldTracePointType::UNSET_RECEIVE_HANDLER;
        }
        else
        {
            // Suppress "AUTOSAR C++14 M0-1-1", The rule states: "A project shall not contain unreachable code"
            // This is false positive, the enum has more fields than EVENT and FIELD so we might reach this branch.
            // coverity[autosar_cpp14_m0_1_1_violation : FALSE]
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(false, "Service element type must be EVENT or FIELD");
        }

        const auto binding_type = proxy_event_binding_base.GetBindingType();
        const auto trace_result =
            TraceData(proxy_event_tracing_data.service_element_instance_identifier_view, trace_point, binding_type);
        UpdateTracingDataFromTraceResult(
            trace_result, proxy_event_tracing_data, proxy_event_tracing_data.enable_unset_receive_handler);
    }
}

void TraceGetNewSamples(ProxyEventTracingData& proxy_event_tracing_data,
                        const ProxyEventBindingBase& proxy_event_binding_base) noexcept
{
    if (proxy_event_tracing_data.enable_get_new_samples)
    {
        const auto service_element_instance_identifier =
            proxy_event_tracing_data.service_element_instance_identifier_view;
        const auto service_element_type =
            service_element_instance_identifier.service_element_identifier_view.service_element_type;

        TracingRuntime::TracePointType trace_point{};
        if (service_element_type == ServiceElementType::EVENT)
        {
            trace_point = ProxyEventTracePointType::GET_NEW_SAMPLES;
        }
        else if (service_element_type == ServiceElementType::FIELD)
        {
            trace_point = ProxyFieldTracePointType::GET_NEW_SAMPLES;
        }
        else
        {
            // Suppress "AUTOSAR C++14 M0-1-1", The rule states: "A project shall not contain unreachable code"
            // This is false positive, the enum has more fields than EVENT and FIELD so we might reach this branch.
            // coverity[autosar_cpp14_m0_1_1_violation : FALSE]
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(false, "Service element type must be EVENT or FIELD");
        }

        const auto binding_type = proxy_event_binding_base.GetBindingType();
        const auto trace_result =
            TraceData(proxy_event_tracing_data.service_element_instance_identifier_view, trace_point, binding_type);
        UpdateTracingDataFromTraceResult(
            trace_result, proxy_event_tracing_data, proxy_event_tracing_data.enable_get_new_samples);
    }
}

void TraceCallGetNewSamplesCallback(ProxyEventTracingData& proxy_event_tracing_data,
                                    const ProxyEventBindingBase& proxy_event_binding_base,
                                    ITracingRuntime::TracePointDataId trace_point_data_id) noexcept
{
    if (proxy_event_tracing_data.enable_new_samples_callback)
    {
        const auto service_element_instance_identifier =
            proxy_event_tracing_data.service_element_instance_identifier_view;
        const auto service_element_type =
            service_element_instance_identifier.service_element_identifier_view.service_element_type;

        TracingRuntime::TracePointType trace_point{};
        if (service_element_type == ServiceElementType::EVENT)
        {
            trace_point = ProxyEventTracePointType::GET_NEW_SAMPLES_CALLBACK;
        }
        else if (service_element_type == ServiceElementType::FIELD)
        {
            trace_point = ProxyFieldTracePointType::GET_NEW_SAMPLES_CALLBACK;
        }
        else
        {
            // Suppress "AUTOSAR C++14 M0-1-1", The rule states: "A project shall not contain unreachable code"
            // This is false positive, the enum has more fields than EVENT and FIELD so we might reach this branch.
            // coverity[autosar_cpp14_m0_1_1_violation : FALSE]
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(false, "Service element type must be EVENT or FIELD");
        }

        const auto binding_type = proxy_event_binding_base.GetBindingType();
        const auto trace_result = TraceData(proxy_event_tracing_data.service_element_instance_identifier_view,
                                            trace_point,
                                            binding_type,
                                            {nullptr, 0U},
                                            trace_point_data_id);
        UpdateTracingDataFromTraceResult(
            trace_result, proxy_event_tracing_data, proxy_event_tracing_data.enable_new_samples_callback);
    }
}

void TraceCallReceiveHandler(ProxyEventTracingData& proxy_event_tracing_data,
                             const ProxyEventBindingBase& proxy_event_binding_base) noexcept
{
    if (proxy_event_tracing_data.enable_call_receive_handler)
    {
        const auto service_element_instance_identifier =
            proxy_event_tracing_data.service_element_instance_identifier_view;
        const auto service_element_type =
            service_element_instance_identifier.service_element_identifier_view.service_element_type;

        TracingRuntime::TracePointType trace_point{};
        if (service_element_type == ServiceElementType::EVENT)
        {
            trace_point = ProxyEventTracePointType::RECEIVE_HANDLER_CALLBACK;
        }
        else if (service_element_type == ServiceElementType::FIELD)
        {
            trace_point = ProxyFieldTracePointType::RECEIVE_HANDLER_CALLBACK;
        }
        else
        {
            // Suppress "AUTOSAR C++14 M0-1-1", The rule states: "A project shall not contain unreachable code"
            // This is false positive, the enum has more fields than EVENT and FIELD so we might reach this branch.
            // coverity[autosar_cpp14_m0_1_1_violation : FALSE]
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(false, "Service element type must be EVENT or FIELD");
        }

        const auto binding_type = proxy_event_binding_base.GetBindingType();
        const auto trace_result =
            TraceData(proxy_event_tracing_data.service_element_instance_identifier_view, trace_point, binding_type);
        UpdateTracingDataFromTraceResult(
            trace_result, proxy_event_tracing_data, proxy_event_tracing_data.enable_call_receive_handler);
    }
}

score::cpp::callback<void(void), 128U> CreateTracingReceiveHandler(ProxyEventTracingData& proxy_event_tracing_data,
                                                            const ProxyEventBindingBase& proxy_event_binding_base,
                                                            EventReceiveHandler handler) noexcept
{
    if (proxy_event_tracing_data.enable_call_receive_handler)
    {
        score::cpp::callback<void(void), 128U> tracing_receive_handler =
            [&proxy_event_tracing_data, &proxy_event_binding_base, handler = std::move(handler)]() noexcept {
                TraceCallReceiveHandler(proxy_event_tracing_data, proxy_event_binding_base);
                handler();
            };
        return tracing_receive_handler;
    }
    return handler;
}

}  // namespace score::mw::com::impl::tracing
