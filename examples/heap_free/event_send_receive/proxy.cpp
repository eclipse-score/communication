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

#include "common/sensor_interface.h"
#include "heap_check/heap_check.h"

#include "score/mw/com/runtime.h"
#include "score/mw/com/runtime_configuration.h"
#include "score/mw/log/logging.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <thread>

namespace
{

// Poll FindService for up to 5 seconds (100 attempts * 50 ms) before giving up.
constexpr std::uint32_t kMaxFindAttempts{100U};
constexpr std::chrono::milliseconds kFindRetryDelay{50};
constexpr std::uint32_t kNumIterations{20U};
constexpr std::chrono::milliseconds kCycleTime{50};
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
    if (!specifier_result.has_value())
    {
        score::mw::log::LogError("PxEs") << "InstanceSpecifier::Create failed";
        return EXIT_FAILURE;
    }

    score::mw::com::ServiceHandleContainer<sensor::SensorProxy::HandleType> handles{};
    for (std::uint32_t attempt = 0U; attempt < kMaxFindAttempts; ++attempt)
    {
        score::Result<score::mw::com::ServiceHandleContainer<sensor::SensorProxy::HandleType>> handles_result =
            sensor::SensorProxy::FindService(specifier_result.value());
        if (!handles_result.has_value())
        {
            score::mw::log::LogError("PxEs") << "FindService failed — check mw_com_config.json";
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
        score::mw::log::LogError("PxEs")
            << "Service not found after all attempts — is event_send_receive/skeleton running?";
        return EXIT_FAILURE;
    }

    score::Result<sensor::SensorProxy> proxy_result = sensor::SensorProxy::Create(handles.front());
    if (!proxy_result.has_value())
    {
        score::mw::log::LogError("PxEs") << "SensorProxy::Create failed";
        return EXIT_FAILURE;
    }
    sensor::SensorProxy& proxy = proxy_result.value();

    score::Result<void> subscribe_result = proxy.reading.Subscribe(kMaxSamples);
    if (!subscribe_result.has_value())
    {
        score::mw::log::LogError("PxEs") << "reading.Subscribe failed";
        return EXIT_FAILURE;
    }

    // ---- OPERATIONAL PHASE (see README.md for heap behavior of each API) ----
    score::mw::log::LogInfo("PxEs") << "Entering operational phase (heap forbidden)";
    heap_check::forbid_heap();

    for (std::uint32_t i = 0U; i < kNumIterations; ++i)
    {
        // GetNewSamples reads from the LoLa shared-memory ring buffer — no heap allocation.
        score::Result<std::size_t> reading_result = proxy.reading.GetNewSamples(
            [](score::mw::com::SamplePtr<sensor::SensorReading> sample) noexcept {
                static_cast<void>(sample->sequence);
                static_cast<void>(sample->value);
            },
            kMaxSamples);
        if (!reading_result.has_value())
        {
            score::mw::log::LogError("PxEs") << "reading.GetNewSamples failed";
            heap_check::allow_heap();
            return EXIT_FAILURE;
        }

        std::this_thread::sleep_for(kCycleTime);
    }

    // ---- CLEANUP ----
    heap_check::allow_heap();
    score::mw::log::LogInfo("PxEs") << "Operational phase complete (" << kNumIterations << " iterations)";

    proxy.reading.Unsubscribe();

    score::mw::log::LogInfo("PxEs") << "Completed successfully";
    return 0;

    // ---- ASYNC RECEIVE HANDLER (NOT HEAP-FREE — kept for reference) ----
    //
    // NOT HEAP-FREE: registering a receive handler causes the skeleton to allocate.
    // See TODO in score/mw/com/impl/scoped_event_receive_handler.h.
    //
    // How to use the async receive API:
    //   1. In the init phase, call SetReceiveHandler to register a callback.
    //   2. The callback fires on the middleware thread when new data arrives.
    //   3. From your receive loop, call GetNewSamples to drain the available samples.
    //   4. Use waitWithAbort(stop_token) on a Notification to block until the handler wakes you.
    //
    // #include "score/concurrency/notification.h"
    // #include "score/stop_token.hpp"
    //
    // score::concurrency::Notification event_received{};
    // score::cpp::stop_source stop_source{};
    //
    // score::Result<void> handler_result = proxy.reading.SetReceiveHandler(
    //     [&event_received]() noexcept { event_received.notify(); });
    // if (!handler_result.has_value())
    // {
    //     score::mw::log::LogError("PxEs") << "reading.SetReceiveHandler failed";
    //     return EXIT_FAILURE;
    // }
    //
    // heap_check::forbid_heap();
    // for (std::uint32_t i = 0U; i < kNumIterations; ++i)
    // {
    //     event_received.waitWithAbort(stop_source.get_token());
    //     event_received.reset();
    //     score::Result<std::size_t> reading_result = proxy.reading.GetNewSamples(
    //         [](score::mw::com::SamplePtr<sensor::SensorReading> sample) noexcept {
    //             static_cast<void>(sample->sequence);
    //             static_cast<void>(sample->value);
    //         },
    //         kMaxSamples);
    //     if (!reading_result.has_value())
    //     {
    //         score::mw::log::LogError("PxEs") << "reading.GetNewSamples failed";
    //         heap_check::allow_heap();
    //         return EXIT_FAILURE;
    //     }
    // }
    // heap_check::allow_heap();
}
