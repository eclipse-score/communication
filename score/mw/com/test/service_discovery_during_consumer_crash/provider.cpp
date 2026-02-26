/*******************************************************************************
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
 *******************************************************************************/

#include "score/mw/com/test/service_discovery_during_consumer_crash/provider.h"

#include "score/mw/com/test/common_test_resources/check_point_control.h"
#include "score/mw/com/test/common_test_resources/general_resources.h"
#include "score/mw/com/test/common_test_resources/provider_resources.h"
#include "score/mw/com/test/service_discovery_during_consumer_crash/test_datatype.h"

#include "score/mw/com/runtime.h"

#include <chrono>
#include <iostream>
#include <thread>

namespace score::mw::com::test
{
namespace
{
constexpr auto kSkeletonInstanceSpecifierString{"test/service_discovery_during_consumer_crash"};
}  // namespace

void DoProviderActions(CheckPointControl& check_point_control,
                       score::cpp::stop_token stop_token,
                       int argc,
                       const char** argv) noexcept
{
    if (argc > 0 && argv != nullptr)
    {
        std::cerr
            << "Provider: Initializing LoLa/mw::com runtime from cmd-line args handed over by parent/controller ..."
            << std::endl;
        mw::com::runtime::InitializeRuntime(argc, argv);
        std::cerr << "Provider: Initializing LoLa/mw::com runtime done." << std::endl;
    }

    // ********************************************************************************
    // Step (P.1) - Create service instance/skeleton
    // ********************************************************************************
    std::cerr << "Provider Step (P.1): Create service instance/skeleton" << std::endl;
    auto skeleton_wrapper_result =
        CreateSkeleton<TestServiceSkeleton>("Provider", kSkeletonInstanceSpecifierString, check_point_control);
    if (!(skeleton_wrapper_result.has_value()))
    {
        return;
    }
    auto& service_instance = skeleton_wrapper_result.value();

    std::cerr << "Provider Step (P.1): skeleton was created. Waiting for proceed instruction to  offer service."
              << std::endl;
    check_point_control.CheckPointReached(1);
    if (WaitForChildProceed(check_point_control, stop_token) !=
        CheckPointControl::ProceedInstruction::PROCEED_NEXT_CHECKPOINT)
    {
        std::cerr << "Provider Step (P.1): Incorrect instruction received." << std::endl;
        check_point_control.ErrorOccurred();
        return;
    }
    // ********************************************************************************
    // Step (P.2) - Offer Service
    // ********************************************************************************
    std::cerr << "Provider Step (P.2): Offer Service" << std::endl;
    const auto offer_service_result =
        OfferService<TestServiceSkeleton>("Provider", service_instance, check_point_control);
    if (!(offer_service_result.has_value()))
    {
        return;
    }

    if (WaitForChildProceed(check_point_control, stop_token) != CheckPointControl::ProceedInstruction::FINISH_ACTIONS)
    {
        std::cerr << "Provider Step (P.2): Received proceed-trigger from controller, but expected finish-trigger!"
                  << std::endl;
        check_point_control.ErrorOccurred();
        return;
    }
    std::cerr << "Provider: Finishing actions!" << std::endl;
}
}  // namespace score::mw::com::test
