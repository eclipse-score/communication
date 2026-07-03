/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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

// Proxy: service discovery and subscribe, no heap in the operational phase.
#include "common/sensor_interface.h"
#include "heap_check/heap_check.h"  // include in exactly ONE translation unit per binary

#include "score/mw/com/runtime.h"
#include "score/mw/com/runtime_configuration.h"
#include "score/mw/log/logging.h"

#include <score/assert.hpp>

#include <chrono>
#include <cstdint>
#include <thread>

namespace
{

// Poll FindService for up to 5 seconds (100 attempts * 50 ms) before giving up.
constexpr std::uint32_t kMaxFindAttempts{100U};
constexpr std::chrono::milliseconds kFindRetryDelay{50};
constexpr std::size_t kMaxSamples{16U};

}  // namespace

int main(int argc, char* argv[])
{
    score::mw::log::LogInfo("PxEs") << "Starting event_send_receive/proxy";

    // ---- INIT PHASE ----

    score::mw::com::runtime::RuntimeConfiguration runtime_config{};
    if (argc > 1)
    {
        runtime_config = score::mw::com::runtime::RuntimeConfiguration{score::filesystem::Path{argv[1]}};
    }
    score::mw::com::runtime::InitializeRuntime(runtime_config);

    score::Result<score::mw::com::InstanceSpecifier> specifier_result =
        score::mw::com::InstanceSpecifier::Create(std::string{"/sensor/event_send_receive/SensorInterface"});
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(specifier_result.has_value(), "InstanceSpecifier::Create failed");

    score::mw::com::ServiceHandleContainer<examples::SensorProxy::HandleType> handles{};
    for (std::uint32_t attempt = 0U; attempt < kMaxFindAttempts; ++attempt)
    {
        score::Result<score::mw::com::ServiceHandleContainer<examples::SensorProxy::HandleType>> handles_result =
            examples::SensorProxy::FindService(specifier_result.value());
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(handles_result.has_value(),
                                                    "FindService failed — check mw_com_config.json");
        handles = std::move(handles_result.value());
        if (!handles.empty())
        {
            break;
        }
        std::this_thread::sleep_for(kFindRetryDelay);
    }
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
        !handles.empty(), "Service not found after all attempts — is event_send_receive/skeleton running?");

    score::Result<examples::SensorProxy> proxy_result = examples::SensorProxy::Create(handles.front());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(proxy_result.has_value(), "SensorProxy::Create failed");
    examples::SensorProxy& proxy = proxy_result.value();

    score::Result<void> subscribe_result = proxy.reading.Subscribe(kMaxSamples);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(subscribe_result.has_value(), "reading.Subscribe failed");

    // ---- OPERATIONAL PHASE (see README.md for heap behavior of each API) ----
    score::mw::log::LogInfo("PxEs") << "Entering operational phase (heap forbidden)";
    heap_check::forbid_heap();
    score::mw::com::SubscriptionState state = proxy.reading.GetSubscriptionState();
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
        state == score::mw::com::SubscriptionState::kSubscribed,
        "Expected kSubscribed — is event_send_receive/skeleton running and offering its service?");

    proxy.reading.Unsubscribe();

    // ---- CLEANUP ----
    heap_check::allow_heap();
    score::mw::log::LogInfo("PxEs") << "Subscription verified and unsubscribed";

    score::mw::log::LogInfo("PxEs") << "Completed successfully";
    return 0;
}
