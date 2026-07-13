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

#include "score/concurrency/notification.h"
#include "score/mw/com/runtime.h"
#include "score/mw/com/runtime_configuration.h"
#include "score/mw/log/logging.h"
#include "score/stop_token.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <thread>

namespace
{

// Allow up to 5 s for the service to appear before giving up.
constexpr std::chrono::seconds kDiscoveryTimeout{5};
constexpr std::uint32_t kNumIterations{20U};
constexpr std::chrono::milliseconds kCycleTime{50};
constexpr std::size_t kMaxSamples{16U};

}  // namespace

int main(int argc, char* argv[])
{
    score::mw::log::LogInfo("PxSF") << "Starting start_find_service/proxy";

    // ---- INIT PHASE ----

    score::mw::com::runtime::RuntimeConfiguration runtime_config{};
    if (argc > 1)
    {
        runtime_config = score::mw::com::runtime::RuntimeConfiguration{score::filesystem::Path{argv[1]}};
    }
    score::mw::com::runtime::InitializeRuntime(runtime_config);

    score::Result<score::mw::com::InstanceSpecifier> specifier_result =
        score::mw::com::InstanceSpecifier::Create(std::string{"/sensor/start_find_service/SensorInterface"});
    if (!specifier_result.has_value())
    {
        score::mw::log::LogError("PxSF") << "InstanceSpecifier::Create failed";
        return EXIT_FAILURE;
    }

    std::optional<sensor::SensorProxy> proxy_opt{};
    score::concurrency::Notification proxy_ready{};
    score::cpp::stop_source stop_source{};

    score::Result<score::mw::com::FindServiceHandle> fshandle_result = sensor::SensorProxy::StartFindService(
        [&proxy_opt, &proxy_ready](score::mw::com::ServiceHandleContainer<sensor::SensorProxy::HandleType> handles,
                                   score::mw::com::FindServiceHandle /*fsh*/) {
            if (handles.empty())
            {
                // Service disappeared — ignore; this example uses one-shot discovery.
                return;
            }
            score::Result<sensor::SensorProxy> proxy_result = sensor::SensorProxy::Create(handles.front());
            if (!proxy_result.has_value())
            {
                score::mw::log::LogError("PxSF") << "SensorProxy::Create failed in discovery callback";
                proxy_ready.notify();  // unblock main so it can check proxy_opt and exit
                return;
            }
            proxy_opt.emplace(std::move(proxy_result.value()));
            proxy_ready.notify();
        },
        specifier_result.value());
    if (!fshandle_result.has_value())
    {
        score::mw::log::LogError("PxSF") << "StartFindService failed";
        return EXIT_FAILURE;
    }

    // Wait for callback to complete Proxy::Create() before forbid_heap().
    bool notified = proxy_ready.waitForWithAbort(kDiscoveryTimeout, stop_source.get_token());

    // Stop continuous discovery — one-shot pattern, we only need to find once.
    score::Result<void> stop_result = sensor::SensorProxy::StopFindService(fshandle_result.value());
    if (!stop_result.has_value())
    {
        score::mw::log::LogError("PxSF") << "StopFindService failed";
        return EXIT_FAILURE;
    }

    if (!notified || !proxy_opt.has_value())
    {
        score::mw::log::LogError("PxSF")
            << "Service not found within timeout — is start_find_service/skeleton running?";
        return EXIT_FAILURE;
    }

    sensor::SensorProxy& proxy = proxy_opt.value();

    score::Result<void> subscribe_result = proxy.reading.Subscribe(kMaxSamples);
    if (!subscribe_result.has_value())
    {
        score::mw::log::LogError("PxSF") << "reading.Subscribe failed";
        return EXIT_FAILURE;
    }

    // ---- OPERATIONAL PHASE (see README.md for heap behavior of each API) ----
    score::mw::log::LogInfo("PxSF") << "Entering operational phase (heap forbidden)";
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
            score::mw::log::LogError("PxSF") << "reading.GetNewSamples failed";
            heap_check::allow_heap();
            return EXIT_FAILURE;
        }

        std::this_thread::sleep_for(kCycleTime);
    }

    // ---- CLEANUP ----
    heap_check::allow_heap();
    score::mw::log::LogInfo("PxSF") << "Operational phase complete (" << kNumIterations << " iterations)";

    proxy.reading.Unsubscribe();

    score::mw::log::LogInfo("PxSF") << "Completed successfully";
    return 0;
}
