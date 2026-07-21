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
#include <thread>

using score::mw::com::tutorial::TirePressureSkeleton;

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
        score::mw::com::InstanceSpecifier::Create(std::string{"/tutorial/start_find_service/TirePressureService"});
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(specifier_result.has_value(), "Failed to create InstanceSpecifier!");

    score::Result<TirePressureSkeleton> skeleton_result = TirePressureSkeleton::Create(specifier_result.value());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(skeleton_result.has_value(), "Failed to create TirePressureSkeleton!");
    TirePressureSkeleton& skeleton = skeleton_result.value();

    score::Result<void> init_update_result = skeleton.tire_pressure_front_left.Update(0.0F);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(init_update_result.has_value(),
                                                "Failed to set tire_pressure_front_left!");

    score::Result<void> init_fr_result = skeleton.tire_pressure_front_right.Update(0.0F);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(init_fr_result.has_value(), "Failed to set tire_pressure_front_right!");

    score::Result<void> init_rl_result = skeleton.tire_pressure_rear_left.Update(0.0F);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(init_rl_result.has_value(), "Failed to set tire_pressure_rear_left!");

    score::Result<void> init_rr_result = skeleton.tire_pressure_rear_right.Update(0.0F);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(init_rr_result.has_value(), "Failed to set tire_pressure_rear_right!");

    score::Result<void> offer_result = skeleton.OfferService();
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(offer_result.has_value(), "Failed to offer TirePressureSkeleton!");

    // ---- OPERATIONAL PHASE (see README.rst for heap behavior of each API) ----
    score::mw::log::LogInfo("SKSF") << "Entering operational phase (heap forbidden)";
    heap_check::forbid_heap();

    for (std::uint32_t i = 0U; i < kNumIterations; ++i)
    {
        score::Result<score::mw::com::SampleAllocateePtr<float>> sample_result =
            skeleton.tire_pressure_update.Allocate();
        if (!sample_result.has_value())
        {
            score::mw::log::LogError("SKSF")
                << "tire_pressure_update.Allocate failed — all SHM slots may be held by slow subscribers";
            heap_check::allow_heap();
            return EXIT_FAILURE;
        }

        *sample_result.value() = static_cast<float>(i) * 0.1F;

        score::Result<void> send_result = skeleton.tire_pressure_update.Send(std::move(sample_result.value()));
        if (!send_result.has_value())
        {
            score::mw::log::LogError("SKSF") << "tire_pressure_update.Send failed";
            heap_check::allow_heap();
            return EXIT_FAILURE;
        }

        std::this_thread::sleep_for(kCycleTime);
    }

    // ---- CLEANUP ----
    heap_check::allow_heap();
    skeleton.StopOfferService();

    score::mw::log::LogInfo("SKSF") << "Operational phase complete (" << kNumIterations << " iterations)";
    score::mw::log::LogInfo("SKSF") << "Completed successfully";
    return 0;
}
