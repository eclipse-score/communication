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

#ifndef SCORE_MW_COM_IMPL_PROXY_EVENT_TRACING_H
#define SCORE_MW_COM_IMPL_PROXY_EVENT_TRACING_H

#include "score/mw/com/impl/event_receive_handler.h"
#include "score/mw/com/impl/generic_proxy_event_binding.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/proxy_event_binding.h"
#include "score/mw/com/impl/proxy_event_binding_base.h"
#include "score/mw/com/impl/tracing/proxy_event_tracing_data.h"

#include <score/callback.hpp>
#include <score/string_view.hpp>

#include <cstddef>

namespace score::mw::com::impl::tracing
{

ProxyEventTracingData GenerateProxyTracingStructFromEventConfig(const InstanceIdentifier& instance_identifier,
                                                                const score::cpp::string_view event_name) noexcept;
ProxyEventTracingData GenerateProxyTracingStructFromFieldConfig(const InstanceIdentifier& instance_identifier,
                                                                const score::cpp::string_view field_name) noexcept;

void TraceSubscribe(ProxyEventTracingData& proxy_event_tracing_data,
                    const ProxyEventBindingBase& proxy_event_binding_base,
                    const std::size_t max_sample_count) noexcept;
void TraceUnsubscribe(ProxyEventTracingData& proxy_event_tracing_data,
                      const ProxyEventBindingBase& proxy_event_binding_base) noexcept;
void TraceSetReceiveHandler(ProxyEventTracingData& proxy_event_tracing_data,
                            const ProxyEventBindingBase& proxy_event_binding_base) noexcept;
void TraceUnsetReceiveHandler(ProxyEventTracingData& proxy_event_tracing_data,
                              const ProxyEventBindingBase& proxy_event_binding_base) noexcept;
void TraceGetNewSamples(ProxyEventTracingData& proxy_event_tracing_data,
                        const ProxyEventBindingBase& proxy_event_binding_base) noexcept;
void TraceCallGetNewSamplesCallback(ProxyEventTracingData& proxy_event_tracing_data,
                                    const ProxyEventBindingBase& proxy_event_binding_base,
                                    ITracingRuntime::TracePointDataId trace_point_data_id) noexcept;
void TraceCallReceiveHandler(ProxyEventTracingData& proxy_event_tracing_data,
                             const ProxyEventBindingBase& proxy_event_binding_base) noexcept;

score::cpp::callback<void(void), 128U> CreateTracingReceiveHandler(ProxyEventTracingData& proxy_event_tracing_data,
                                                            const ProxyEventBindingBase& proxy_event_binding_base,
                                                            EventReceiveHandler handler) noexcept;

template <typename SampleType, typename ReceiverType>
auto CreateTracingGetNewSamplesCallback(ProxyEventTracingData& proxy_event_tracing_data,
                                        const ProxyEventBindingBase& proxy_event_binding_base,
                                        ReceiverType&& receiver) noexcept ->
    typename ProxyEventBinding<SampleType>::Callback
{
    if (proxy_event_tracing_data.enable_new_samples_callback)
    {
        typename ProxyEventBinding<SampleType>::Callback tracing_receiver =
            // Suppress "AUTOSAR C++14 A18-9-2", The rule states: "Forwarding values to other functions shall be done
            // via: (1) std::move if the value is an rvalue reference, (2) std::forward if the value is forwarding
            // reference. std::forward is already used here.
            // coverity[autosar_cpp14_a18_9_2_violation : FALSE]
            [&proxy_event_tracing_data, &proxy_event_binding_base, receiver = std::forward<ReceiverType>(receiver)](
                SamplePtr<SampleType> sample_ptr, ITracingRuntime::TracePointDataId trace_point_data_id) noexcept {
                TraceCallGetNewSamplesCallback(proxy_event_tracing_data, proxy_event_binding_base, trace_point_data_id);
                // Suppress "AUTOSAR C++14 A18-9-2", The rule states: "Forwarding values to other functions shall be
                // done via: (1) std::move if the value is an rvalue reference, (2) std::forward if the value is
                // forwarding reference. std::move is already used here.
                // coverity[autosar_cpp14_a18_9_2_violation : FALSE]
                receiver(std::move(sample_ptr));
            };
        return tracing_receiver;
    }
    else
    {
        typename ProxyEventBinding<SampleType>::Callback tracing_receiver =
            // Suppress "AUTOSAR C++14 A18-9-2", The rule states: "Forwarding values to other functions shall be done
            // via: (1) std::move if the value is an rvalue reference, (2) std::forward if the value is forwarding
            // reference. std::forward is already used here.
            // coverity[autosar_cpp14_a18_9_2_violation : FALSE]
            [receiver = std::forward<ReceiverType>(receiver)](SamplePtr<SampleType> sample_ptr,
                                                              ITracingRuntime::TracePointDataId) noexcept {
                // Suppress "AUTOSAR C++14 A18-9-2", The rule states: "Forwarding values to other functions shall be
                // done via: (1) std::move if the value is an rvalue reference, (2) std::forward if the value is
                // forwarding reference. std::move is already used here.
                // coverity[autosar_cpp14_a18_9_2_violation : FALSE]
                receiver(std::move(sample_ptr));
            };
        return tracing_receiver;
    }
}

template <typename ReceiverType>
typename GenericProxyEventBinding::Callback CreateTracingGenericGetNewSamplesCallback(
    ProxyEventTracingData& /* proxy_event_tracing_data */,
    ReceiverType&& receiver) noexcept
{
    // Suppress "AUTOSAR C++14 A18-9-2", The rule states: "Forwarding values to other functions shall be done
    // via: (1) std::move if the value is an rvalue reference, (2) std::forward if the value is forwarding
    // reference. std::forward is already used here.
    // coverity[autosar_cpp14_a18_9_2_violation : FALSE]
    typename GenericProxyEventBinding::Callback tracing_receiver = [receiver = std::forward<ReceiverType>(receiver)](
                                                                       SamplePtr<void> sample_ptr,
                                                                       ITracingRuntime::TracePointDataId) noexcept {
        // Suppress "AUTOSAR C++14 A18-9-2", The rule states: "Forwarding values to other functions shall be done
        // via: (1) std::move if the value is an rvalue reference, (2) std::forward if the value is forwarding
        // reference. std::move is already used here.
        // coverity[autosar_cpp14_a18_9_2_violation : FALSE]
        receiver(std::move(sample_ptr));
    };
    return tracing_receiver;
}

}  // namespace score::mw::com::impl::tracing

#endif  // SCORE_MW_COM_IMPL_PROXY_EVENT_TRACING_H
