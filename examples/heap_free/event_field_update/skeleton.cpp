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

// Skeleton: event + field update, no heap in the operational phase.
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

constexpr std::uint32_t kNumIterations{200U};
constexpr std::chrono::milliseconds kCycleTime{10};

}  // namespace

int main(int argc, char* argv[])
{
    score::mw::log::LogInfo("SkEf") << "Starting event_field_update/skeleton";

    // ---- INIT PHASE ----

    score::mw::com::runtime::RuntimeConfiguration runtime_config{};
    if (argc > 1)
    {
        runtime_config = score::mw::com::runtime::RuntimeConfiguration{score::filesystem::Path{argv[1]}};
    }
    score::mw::com::runtime::InitializeRuntime(runtime_config);

    score::Result<score::mw::com::InstanceSpecifier> specifier_result =
        score::mw::com::InstanceSpecifier::Create(std::string{"/sensor/event_field_update/SensorInterface"});
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(specifier_result.has_value(), "InstanceSpecifier::Create failed");

    score::Result<examples::SensorSkeleton> skeleton_result =
        examples::SensorSkeleton::Create(specifier_result.value());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(skeleton_result.has_value(),
                                                "SensorSkeleton::Create failed — check mw_com_config.json");
    examples::SensorSkeleton& sk = skeleton_result.value();

    score::Result<void> init_update_result = sk.calibration_status.Update(0U);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(init_update_result.has_value(),
                                                "calibration_status.Update (initial) failed");

    score::Result<void> offer_result = sk.OfferService();
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(offer_result.has_value(),
                                                "OfferService failed — check mw_com_config.json");

    // ---- OPERATIONAL PHASE (see README.md for heap behavior of each API) ----
    score::mw::log::LogInfo("SkEf") << "Entering operational phase (heap forbidden)";
    heap_check::forbid_heap();

    for (std::uint32_t i = 0U; i < kNumIterations; ++i)
    {
        score::Result<score::mw::com::SampleAllocateePtr<examples::SensorReading>> sample_result =
            sk.reading.Allocate();
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
            sample_result.has_value(), "reading.Allocate failed — all SHM slots may be held by slow subscribers");

        sample_result.value()->sequence = i;
        sample_result.value()->value = static_cast<float>(i) * 0.1F;

        score::Result<void> send_result = sk.reading.Send(std::move(sample_result.value()));
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(send_result.has_value(), "reading.Send failed");

        score::Result<void> field_result = sk.calibration_status.Update(i);
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(field_result.has_value(), "calibration_status.Update failed");

        std::this_thread::sleep_for(kCycleTime);
    }

    // ---- CLEANUP ----
    heap_check::allow_heap();
    sk.StopOfferService();

    score::mw::log::LogInfo("SkEf") << "Operational phase complete (" << kNumIterations << " iterations)";
    score::mw::log::LogInfo("SkEf") << "Completed successfully";
    return 0;
}
