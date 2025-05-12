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
#include "score/mw/com/impl/tracing/proxy_event_tracing_data.h"

namespace score::mw::com::impl::tracing
{

void DisableAllTracePoints(ProxyEventTracingData& proxy_event_tracing_data) noexcept
{
    proxy_event_tracing_data.enable_subscribe = false;
    proxy_event_tracing_data.enable_unsubscribe = false;
    proxy_event_tracing_data.enable_subscription_state_changed = false;
    proxy_event_tracing_data.enable_set_subcription_state_change_handler = false;
    proxy_event_tracing_data.enable_unset_subscription_state_change_handler = false;
    proxy_event_tracing_data.enable_call_subscription_state_change_handler = false;
    proxy_event_tracing_data.enable_set_receive_handler = false;
    proxy_event_tracing_data.enable_unset_receive_handler = false;
    proxy_event_tracing_data.enable_call_receive_handler = false;
    proxy_event_tracing_data.enable_get_new_samples = false;
    proxy_event_tracing_data.enable_new_samples_callback = false;
}

}  // namespace score::mw::com::impl::tracing
