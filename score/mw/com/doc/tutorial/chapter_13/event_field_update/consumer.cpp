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

#include "score/mw/log/logging.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <thread>

using score::mw::com::tutorial::TirePressureProxy;

namespace
{

constexpr std::uint32_t kMaxFindAttempts{100U};
constexpr std::chrono::milliseconds kFindRetryDelay{50};
constexpr std::uint32_t kNumIterations{20U};
constexpr std::chrono::milliseconds kCycleTime{50};
constexpr std::size_t kMaxSamples{16U};

}  // namespace

int main()
{
    score::mw::log::LogInfo("PxEf") << "Starting event_field_update/consumer";

    // ---- INIT PHASE ----

    score::Result<score::mw::com::InstanceSpecifier> specifier_result =
        score::mw::com::InstanceSpecifier::Create(std::string{"/tutorial/event_field_update/TirePressureService"});
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(specifier_result.has_value(), "Failed to create InstanceSpecifier!");

    score::mw::com::ServiceHandleContainer<TirePressureProxy::HandleType> handles{};
    for (std::uint32_t attempt = 0U; attempt < kMaxFindAttempts; ++attempt)
    {
        score::Result<score::mw::com::ServiceHandleContainer<TirePressureProxy::HandleType>> handles_result =
            TirePressureProxy::FindService(specifier_result.value());
        if (!handles_result.has_value())
        {
            score::mw::log::LogError("PxEf") << "FindService failed — check mw_com_config.json";
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
        score::mw::log::LogError("PxEf")
            << "Service not found after all attempts — is event_field_update/provider running?";
        return EXIT_FAILURE;
    }

    score::Result<TirePressureProxy> proxy_result = TirePressureProxy::Create(handles.front());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(proxy_result.has_value(), "Failed to create TirePressureProxy!");
    TirePressureProxy& proxy = proxy_result.value();

    score::Result<void> event_subscribe_result = proxy.tire_pressure_update.Subscribe(kMaxSamples);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(event_subscribe_result.has_value(),
                                                "Failed to subscribe to tire_pressure_update!");

    score::Result<void> field_subscribe_result = proxy.tire_pressure_front_left.Subscribe(1U);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(field_subscribe_result.has_value(),
                                                "Failed to subscribe to tire_pressure_front_left!");

    // ---- OPERATIONAL PHASE (see README.rst for heap behavior of each API) ----
    score::mw::log::LogInfo("PxEf") << "Entering operational phase (heap forbidden)";
    heap_check::forbid_heap();

    for (std::uint32_t i = 0U; i < kNumIterations; ++i)
    {
        score::Result<std::size_t> event_result = proxy.tire_pressure_update.GetNewSamples(
            [](score::mw::com::SamplePtr<float> sample) noexcept {
                static_cast<void>(*sample);
            },
            kMaxSamples);
        if (!event_result.has_value())
        {
            score::mw::log::LogError("PxEf") << "tire_pressure_update.GetNewSamples failed";
            heap_check::allow_heap();
            return EXIT_FAILURE;
        }

        score::Result<std::size_t> field_result = proxy.tire_pressure_front_left.GetNewSamples(
            [](score::mw::com::SamplePtr<float> sample) noexcept {
                static_cast<void>(*sample);
            },
            1U);
        if (!field_result.has_value())
        {
            score::mw::log::LogError("PxEf") << "tire_pressure_front_left.GetNewSamples failed";
            heap_check::allow_heap();
            return EXIT_FAILURE;
        }

        std::this_thread::sleep_for(kCycleTime);
    }

    // ---- CLEANUP ----
    heap_check::allow_heap();
    score::mw::log::LogInfo("PxEf") << "Operational phase complete (" << kNumIterations << " iterations)";

    proxy.tire_pressure_update.Unsubscribe();
    proxy.tire_pressure_front_left.Unsubscribe();

    score::mw::log::LogInfo("PxEf") << "Completed successfully";
    return 0;
}
