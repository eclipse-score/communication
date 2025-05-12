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
#ifndef SCORE_MW_COM_IMPL_TRACING_PROXY_EVENT_TRACING_DATA_H
#define SCORE_MW_COM_IMPL_TRACING_PROXY_EVENT_TRACING_DATA_H

#include "score/mw/com/impl/tracing/configuration/service_element_instance_identifier_view.h"

namespace score::mw::com::impl::tracing
{

struct ProxyEventTracingData
{
    // Suppress "AUTOSAR C++14 M11-0-1" rule finding. This rule states: "Member data in non-POD class types shall be
    // private.". The is a false positive, this a POD class.
    // coverity[autosar_cpp14_m11_0_1_violation : FALSE]
    ServiceElementInstanceIdentifierView service_element_instance_identifier_view{};

    // coverity[autosar_cpp14_m11_0_1_violation : FALSE]
    bool enable_subscribe{false};
    // coverity[autosar_cpp14_m11_0_1_violation : FALSE]
    bool enable_unsubscribe{false};
    // coverity[autosar_cpp14_m11_0_1_violation : FALSE]
    bool enable_subscription_state_changed{false};
    // coverity[autosar_cpp14_m11_0_1_violation : FALSE]
    bool enable_set_subcription_state_change_handler{false};
    // coverity[autosar_cpp14_m11_0_1_violation : FALSE]
    bool enable_unset_subscription_state_change_handler{false};
    // coverity[autosar_cpp14_m11_0_1_violation : FALSE]
    bool enable_call_subscription_state_change_handler{false};
    // coverity[autosar_cpp14_m11_0_1_violation : FALSE]
    bool enable_set_receive_handler{false};
    // coverity[autosar_cpp14_m11_0_1_violation : FALSE]
    bool enable_unset_receive_handler{false};
    // coverity[autosar_cpp14_m11_0_1_violation : FALSE]
    bool enable_call_receive_handler{false};
    // coverity[autosar_cpp14_m11_0_1_violation : FALSE]
    bool enable_get_new_samples{false};
    // coverity[autosar_cpp14_m11_0_1_violation : FALSE]
    bool enable_new_samples_callback{false};
};

void DisableAllTracePoints(ProxyEventTracingData& proxy_event_tracing_data) noexcept;

}  // namespace score::mw::com::impl::tracing

#endif  // SCORE_MW_COM_IMPL_TRACING_PROXY_EVENT_TRACING_DATA_H
