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

// Async receive handler example for event_send_receive.
//
// This binary shows how to use SetReceiveHandler and waitWithAbort for event-driven
// reception. It is NOT heap-free end-to-end: SetReceiveHandler causes the skeleton
// background thread to allocate when it registers the notification handler. For a
// heap-free alternative, use event_send_receive/consumer (GetNewSamples polling).

#include "common/tire_pressure_service.h"

#include "score/concurrency/notification.h"
#include "score/mw/log/logging.h"
#include "score/stop_token.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <thread>

using score::mw::com::tutorial::TirePressureProxy;

namespace
{

// Poll FindService for up to 5 seconds (100 attempts * 50 ms) before giving up.
constexpr std::uint32_t kMaxFindAttempts{100U};
constexpr std::chrono::milliseconds kFindRetryDelay{50};
constexpr std::uint32_t kNumIterations{20U};
constexpr std::size_t kMaxSamples{16U};

}  // namespace

int main()
{
    score::mw::log::LogInfo("PxEsH") << "Starting event_send_receive/consumer_receive_handler";

    score::Result<score::mw::com::InstanceSpecifier> specifier_result =
        score::mw::com::InstanceSpecifier::Create(std::string{"/tutorial/event_send_receive/TirePressureService"});
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(specifier_result.has_value(), "Failed to create InstanceSpecifier!");

    score::mw::com::ServiceHandleContainer<TirePressureProxy::HandleType> handles{};
    for (std::uint32_t attempt = 0U; attempt < kMaxFindAttempts; ++attempt)
    {
        score::Result<score::mw::com::ServiceHandleContainer<TirePressureProxy::HandleType>> handles_result =
            TirePressureProxy::FindService(specifier_result.value());
        if (!handles_result.has_value())
        {
            score::mw::log::LogError("PxEsH") << "FindService failed — check mw_com_config.json";
            return EXIT_FAILURE;
        }
        handles = std::move(handles_result.value());
        if (!handles.empty())
        {
            break;
        }
        std::this_thread::sleep_for(kFindRetryDelay);
    }
    if (handles.empty())
    {
        score::mw::log::LogError("PxEsH")
            << "Service not found after all attempts — is event_send_receive/provider running?";
        return EXIT_FAILURE;
    }

    score::Result<TirePressureProxy> proxy_result = TirePressureProxy::Create(handles.front());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(proxy_result.has_value(), "Failed to create TirePressureProxy!");
    TirePressureProxy& proxy = proxy_result.value();

    score::concurrency::Notification event_received{};
    score::cpp::stop_source stop_source{};

    // SetReceiveHandler allocates on both the proxy side (make_shared<ScopedEventReceiveHandler>)
    // and on the skeleton background thread when it registers the notification. This binary
    // therefore does not call forbid_heap().
    score::Result<void> handler_result = proxy.tire_pressure_update.SetReceiveHandler([&event_received]() noexcept {
        event_received.notify();
    });
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(handler_result.has_value(),
                                                "Failed to register tire_pressure_update receive handler!");

    score::Result<void> subscribe_result = proxy.tire_pressure_update.Subscribe(kMaxSamples);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(subscribe_result.has_value(),
                                                "Failed to subscribe to tire_pressure_update!");

    score::mw::log::LogInfo("PxEsH") << "Entering receive loop";

    for (std::uint32_t i = 0U; i < kNumIterations; ++i)
    {
        event_received.waitWithAbort(stop_source.get_token());
        event_received.reset();

        score::Result<std::size_t> reading_result = proxy.tire_pressure_update.GetNewSamples(
            [](score::mw::com::SamplePtr<float> sample) noexcept {
                static_cast<void>(*sample);
            },
            kMaxSamples);
        if (!reading_result.has_value())
        {
            score::mw::log::LogError("PxEsH") << "tire_pressure_update.GetNewSamples failed";
            return EXIT_FAILURE;
        }
    }

    proxy.tire_pressure_update.Unsubscribe();

    score::mw::log::LogInfo("PxEsH") << "Completed successfully";
    return 0;
}
