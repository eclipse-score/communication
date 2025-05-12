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

#include <gtest/gtest.h>

namespace score::mw::com::impl::tracing
{
namespace
{

TEST(ProxyEventTracingDataTest, CallingDisableAllTracePointsWillSetAllTracePointsToFalse)
{
    // Given a ProxyEventTracingData object with all trace points set to true
    ProxyEventTracingData proxy_event_tracing_data{};
    proxy_event_tracing_data.enable_subscribe = true;
    proxy_event_tracing_data.enable_unsubscribe = true;
    proxy_event_tracing_data.enable_subscription_state_changed = true;
    proxy_event_tracing_data.enable_set_subcription_state_change_handler = true;
    proxy_event_tracing_data.enable_unset_subscription_state_change_handler = true;
    proxy_event_tracing_data.enable_call_subscription_state_change_handler = true;
    proxy_event_tracing_data.enable_set_receive_handler = true;
    proxy_event_tracing_data.enable_unset_receive_handler = true;
    proxy_event_tracing_data.enable_call_receive_handler = true;
    proxy_event_tracing_data.enable_get_new_samples = true;
    proxy_event_tracing_data.enable_new_samples_callback = true;

    // When calling DisableAllTracePoints
    DisableAllTracePoints(proxy_event_tracing_data);

    // Then all trace points will be set to false
    EXPECT_FALSE(proxy_event_tracing_data.enable_subscribe);
    EXPECT_FALSE(proxy_event_tracing_data.enable_unsubscribe);
    EXPECT_FALSE(proxy_event_tracing_data.enable_subscription_state_changed);
    EXPECT_FALSE(proxy_event_tracing_data.enable_set_subcription_state_change_handler);
    EXPECT_FALSE(proxy_event_tracing_data.enable_unset_subscription_state_change_handler);
    EXPECT_FALSE(proxy_event_tracing_data.enable_call_subscription_state_change_handler);
    EXPECT_FALSE(proxy_event_tracing_data.enable_set_receive_handler);
    EXPECT_FALSE(proxy_event_tracing_data.enable_unset_receive_handler);
    EXPECT_FALSE(proxy_event_tracing_data.enable_call_receive_handler);
    EXPECT_FALSE(proxy_event_tracing_data.enable_get_new_samples);
    EXPECT_FALSE(proxy_event_tracing_data.enable_new_samples_callback);
}

}  // namespace
}  // namespace score::mw::com::impl::tracing
