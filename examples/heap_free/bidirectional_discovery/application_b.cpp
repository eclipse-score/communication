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

// Bi-directional discovery — application B.
// Offers its own SensorInterface instance immediately (OfferService before discovery completes),
// then discovers application A via StartFindService. Both applications can start in any order without deadlock.

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

constexpr std::chrono::seconds kDiscoveryTimeout{5};
constexpr std::uint32_t kNumIterations{20U};
constexpr std::chrono::milliseconds kCycleTime{50};
constexpr std::size_t kMaxSamples{16U};

}  // namespace

int main(int argc, char* argv[])
{
    score::mw::log::LogInfo("BiB") << "Starting bidirectional_discovery/application_b";

    // ---- INIT PHASE ----

    score::mw::com::runtime::RuntimeConfiguration runtime_config{};
    if (argc > 1)
    {
        runtime_config = score::mw::com::runtime::RuntimeConfiguration{score::filesystem::Path{argv[1]}};
    }
    score::mw::com::runtime::InitializeRuntime(runtime_config);

    score::Result<score::mw::com::InstanceSpecifier> own_spec_result =
        score::mw::com::InstanceSpecifier::Create(std::string{"/sensor/bidirectional_discovery/application_b"});
    if (!own_spec_result.has_value())
    {
        score::mw::log::LogError("BiB") << "InstanceSpecifier::Create failed for own specifier";
        return EXIT_FAILURE;
    }

    score::Result<sensor::SensorSkeleton> skeleton_result = sensor::SensorSkeleton::Create(own_spec_result.value());
    if (!skeleton_result.has_value())
    {
        score::mw::log::LogError("BiB") << "SensorSkeleton::Create failed — check mw_com_config.json";
        return EXIT_FAILURE;
    }
    sensor::SensorSkeleton& sk = skeleton_result.value();

    score::Result<void> init_update_result = sk.calibration_status.Update(0U);
    if (!init_update_result.has_value())
    {
        score::mw::log::LogError("BiB") << "calibration_status.Update (initial) failed";
        return EXIT_FAILURE;
    }

    // Offer immediately — this is what avoids the deadlock when both applications start concurrently.
    score::Result<void> offer_result = sk.OfferService();
    if (!offer_result.has_value())
    {
        score::mw::log::LogError("BiB") << "OfferService failed — check mw_com_config.json";
        return EXIT_FAILURE;
    }

    score::Result<score::mw::com::InstanceSpecifier> remote_spec_result =
        score::mw::com::InstanceSpecifier::Create(std::string{"/sensor/bidirectional_discovery/application_a"});
    if (!remote_spec_result.has_value())
    {
        score::mw::log::LogError("BiB") << "InstanceSpecifier::Create failed for remote specifier";
        return EXIT_FAILURE;
    }

    std::optional<sensor::SensorProxy> remote_proxy{};
    score::concurrency::Notification proxy_ready{};
    score::cpp::stop_source stop_source{};

    score::Result<score::mw::com::FindServiceHandle> fshandle_result = sensor::SensorProxy::StartFindService(
        [&remote_proxy, &proxy_ready](score::mw::com::ServiceHandleContainer<sensor::SensorProxy::HandleType> handles,
                                      score::mw::com::FindServiceHandle /*fsh*/) {
            if (handles.empty())
            {
                return;
            }
            score::Result<sensor::SensorProxy> proxy_result = sensor::SensorProxy::Create(handles.front());
            if (!proxy_result.has_value())
            {
                score::mw::log::LogError("BiB") << "SensorProxy::Create failed in discovery callback";
                proxy_ready.notify();
                return;
            }
            remote_proxy.emplace(std::move(proxy_result.value()));
            proxy_ready.notify();
        },
        remote_spec_result.value());
    if (!fshandle_result.has_value())
    {
        score::mw::log::LogError("BiB") << "StartFindService failed";
        return EXIT_FAILURE;
    }

    // Wait for application A's service before forbid_heap() — Proxy::Create() allocates.
    bool notified = proxy_ready.waitForWithAbort(kDiscoveryTimeout, stop_source.get_token());

    score::Result<void> stop_result = sensor::SensorProxy::StopFindService(fshandle_result.value());
    if (!stop_result.has_value())
    {
        score::mw::log::LogError("BiB") << "StopFindService failed";
        return EXIT_FAILURE;
    }

    if (!notified || !remote_proxy.has_value())
    {
        score::mw::log::LogError("BiB") << "Application A not found within timeout — is application_a running?";
        return EXIT_FAILURE;
    }

    sensor::SensorProxy& proxy = remote_proxy.value();

    score::Result<void> subscribe_result = proxy.reading.Subscribe(kMaxSamples);
    if (!subscribe_result.has_value())
    {
        score::mw::log::LogError("BiB") << "reading.Subscribe failed";
        return EXIT_FAILURE;
    }

    // ---- OPERATIONAL PHASE (see README.md for heap behavior of each API) ----
    score::mw::log::LogInfo("BiB") << "Entering operational phase (heap forbidden)";
    heap_check::forbid_heap();

    for (std::uint32_t i = 0U; i < kNumIterations; ++i)
    {
        score::Result<score::mw::com::SampleAllocateePtr<sensor::SensorReading>> sample_result = sk.reading.Allocate();
        if (!sample_result.has_value())
        {
            score::mw::log::LogError("BiB") << "reading.Allocate failed";
            heap_check::allow_heap();
            return EXIT_FAILURE;
        }

        sample_result.value()->sequence = i;
        sample_result.value()->value = static_cast<float>(i) * 0.1F;

        score::Result<void> send_result = sk.reading.Send(std::move(sample_result.value()));
        if (!send_result.has_value())
        {
            score::mw::log::LogError("BiB") << "reading.Send failed";
            heap_check::allow_heap();
            return EXIT_FAILURE;
        }

        score::Result<std::size_t> recv_result = proxy.reading.GetNewSamples(
            [](score::mw::com::SamplePtr<sensor::SensorReading> sample) noexcept {
                static_cast<void>(sample->sequence);
                static_cast<void>(sample->value);
            },
            kMaxSamples);
        if (!recv_result.has_value())
        {
            score::mw::log::LogError("BiB") << "reading.GetNewSamples failed";
            heap_check::allow_heap();
            return EXIT_FAILURE;
        }

        std::this_thread::sleep_for(kCycleTime);
    }

    // ---- CLEANUP ----
    heap_check::allow_heap();
    score::mw::log::LogInfo("BiB") << "Operational phase complete (" << kNumIterations << " iterations)";

    proxy.reading.Unsubscribe();
    sk.StopOfferService();

    score::mw::log::LogInfo("BiB") << "Completed successfully";
    return 0;
}
