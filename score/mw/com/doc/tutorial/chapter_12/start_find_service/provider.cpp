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

#include "common/service_interface.h"
#include "heap_check/heap_check.h"

#include "score/mw/log/logging.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <thread>

namespace
{

constexpr std::uint32_t kNumIterations{100U};
constexpr std::chrono::milliseconds kCycleTime{10};

}  // namespace

int main()
{
    score::mw::log::LogInfo("SKSF") << "Starting start_find_service/provider";

    // ---- INIT PHASE ----

    score::Result<score::mw::com::InstanceSpecifier> specifier_result =
        score::mw::com::InstanceSpecifier::Create(std::string{"/sensor/start_find_service/SensorInterface"});
    if (!specifier_result.has_value())
    {
        score::mw::log::LogError("SKSF") << "InstanceSpecifier::Create failed";
        return EXIT_FAILURE;
    }

    score::Result<sensor::SensorSkeleton> skeleton_result = sensor::SensorSkeleton::Create(specifier_result.value());
    if (!skeleton_result.has_value())
    {
        score::mw::log::LogError("SKSF") << "SensorSkeleton::Create failed — check mw_com_config.json";
        return EXIT_FAILURE;
    }
    sensor::SensorSkeleton& sk = skeleton_result.value();

    score::Result<void> init_update_result = sk.calibration_status.Update(0U);
    if (!init_update_result.has_value())
    {
        score::mw::log::LogError("SKSF") << "calibration_status.Update (initial) failed";
        return EXIT_FAILURE;
    }

    score::Result<void> offer_result = sk.OfferService();
    if (!offer_result.has_value())
    {
        score::mw::log::LogError("SKSF") << "OfferService failed — check mw_com_config.json";
        return EXIT_FAILURE;
    }

    // ---- OPERATIONAL PHASE (see README.rst for heap behavior of each API) ----
    score::mw::log::LogInfo("SKSF") << "Entering operational phase (heap forbidden)";
    heap_check::forbid_heap();

    for (std::uint32_t i = 0U; i < kNumIterations; ++i)
    {
        score::Result<score::mw::com::SampleAllocateePtr<sensor::SensorReading>> sample_result = sk.reading.Allocate();
        if (!sample_result.has_value())
        {
            score::mw::log::LogError("SKSF")
                << "reading.Allocate failed — all SHM slots may be held by slow subscribers";
            heap_check::allow_heap();
            return EXIT_FAILURE;
        }

        sample_result.value()->sequence = i;
        sample_result.value()->value = static_cast<float>(i) * 0.1F;

        score::Result<void> send_result = sk.reading.Send(std::move(sample_result.value()));
        if (!send_result.has_value())
        {
            score::mw::log::LogError("SKSF") << "reading.Send failed";
            heap_check::allow_heap();
            return EXIT_FAILURE;
        }

        std::this_thread::sleep_for(kCycleTime);
    }

    // ---- CLEANUP ----
    heap_check::allow_heap();
    sk.StopOfferService();

    score::mw::log::LogInfo("SKSF") << "Operational phase complete (" << kNumIterations << " iterations)";
    score::mw::log::LogInfo("SKSF") << "Completed successfully";
    return 0;
}
