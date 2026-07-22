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

#include "common/tire_pressure_service.h"
#include "heap_check/heap_check.h"

#include "score/concurrency/notification.h"
#include "score/mw/log/logging.h"
#include "score/stop_token.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <thread>

using score::mw::com::tutorial::TirePressureProxy;

namespace
{

// Allow up to 5 s for the service to appear before giving up.
constexpr std::chrono::seconds kDiscoveryTimeout{5};
constexpr std::uint32_t kNumIterations{20U};
constexpr std::chrono::milliseconds kCycleTime{50};
constexpr std::size_t kMaxSamples{16U};

}  // namespace

int main()
{
    score::mw::log::LogInfo("PxSF") << "Starting start_find_service/consumer";

    // ---- INIT PHASE ----

    score::Result<score::mw::com::InstanceSpecifier> specifier_result =
        score::mw::com::InstanceSpecifier::Create(std::string{"/tutorial/start_find_service/TirePressureService"});
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(specifier_result.has_value(), "Failed to create InstanceSpecifier!");

    std::optional<TirePressureProxy> proxy_opt{};
    score::concurrency::Notification proxy_ready{};
    score::cpp::stop_source stop_source{};

    score::Result<score::mw::com::FindServiceHandle> find_service_handle_result = TirePressureProxy::StartFindService(
        [&proxy_opt, &proxy_ready](score::mw::com::ServiceHandleContainer<TirePressureProxy::HandleType> handles,
                                   score::mw::com::FindServiceHandle /*fsh*/) {
            if (handles.empty())
            {
                // Service disappeared — ignore; this example uses one-shot discovery.
                return;
            }
            score::Result<TirePressureProxy> proxy_result = TirePressureProxy::Create(handles.front());
            if (!proxy_result.has_value())
            {
                score::mw::log::LogError("PxSF") << "TirePressureProxy::Create failed in discovery callback";
                proxy_ready.notify();  // unblock main so it can check proxy_opt and exit
                return;
            }
            proxy_opt.emplace(std::move(proxy_result.value()));
            proxy_ready.notify();
        },
        specifier_result.value());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(find_service_handle_result.has_value(),
                                                "Failed to start service discovery!");

    // Wait for callback to complete Proxy::Create() before forbid_heap().
    bool notified = proxy_ready.waitForWithAbort(kDiscoveryTimeout, stop_source.get_token());

    // Stop continuous discovery — one-shot pattern, we only need to find once.
    score::Result<void> stop_result = TirePressureProxy::StopFindService(find_service_handle_result.value());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(stop_result.has_value(), "Failed to stop service discovery!");

    if (!notified || !proxy_opt.has_value())
    {
        score::mw::log::LogError("PxSF")
            << "Service not found within timeout — is start_find_service/provider running?";
        return EXIT_FAILURE;
    }

    TirePressureProxy& proxy = proxy_opt.value();

    score::Result<void> subscribe_result = proxy.tire_pressure_update.Subscribe(kMaxSamples);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(subscribe_result.has_value(),
                                                "Failed to subscribe to tire_pressure_update!");

    // ---- OPERATIONAL PHASE (see README.rst for heap behavior of each API) ----
    score::mw::log::LogInfo("PxSF") << "Entering operational phase (heap forbidden)";
    heap_check::forbid_heap();

    for (std::uint32_t i = 0U; i < kNumIterations; ++i)
    {
        // GetNewSamples reads from the LoLa shared-memory ring buffer — no heap allocation.
        score::Result<std::size_t> reading_result = proxy.tire_pressure_update.GetNewSamples(
            [](score::mw::com::SamplePtr<float> sample) noexcept {
                static_cast<void>(*sample);
            },
            kMaxSamples);
        if (!reading_result.has_value())
        {
            score::mw::log::LogError("PxSF") << "tire_pressure_update.GetNewSamples failed";
            heap_check::allow_heap();
            return EXIT_FAILURE;
        }

        std::this_thread::sleep_for(kCycleTime);
    }

    // ---- CLEANUP ----
    heap_check::allow_heap();
    score::mw::log::LogInfo("PxSF") << "Operational phase complete (" << kNumIterations << " iterations)";

    proxy.tire_pressure_update.Unsubscribe();

    score::mw::log::LogInfo("PxSF") << "Completed successfully";
    return 0;
}
