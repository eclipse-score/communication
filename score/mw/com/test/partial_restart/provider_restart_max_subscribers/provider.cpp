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

#include "score/mw/com/test/partial_restart/provider_restart_max_subscribers/provider.h"

#include "score/mw/com/test/common_test_resources/general_resources.h"
#include "score/mw/com/test/common_test_resources/provider_resources.h"
#include "score/mw/com/test/partial_restart/test_datatype.h"

#include "score/mw/com/runtime.h"

#include <iostream>

namespace score::mw::com::test
{

namespace
{

constexpr auto kSkeletonInstanceSpecifierString{"partial_restart/small_but_great0"};

}

void DoProviderActions(CheckPointControl& check_point_control,
                       score::cpp::stop_token test_stop_token,
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
    std::cout << "Provider Step (P.1): Create service instance/skeleton" << std::endl;
    auto skeleton_wrapper_result =
        CreateSkeleton<TestServiceSkeleton>("Provider", kSkeletonInstanceSpecifierString, check_point_control);
    if (!(skeleton_wrapper_result.has_value()))
    {
        return;
    }
    auto& service_instance = skeleton_wrapper_result.value();

    // ********************************************************************************
    // Step (P.2) - Offer Service
    // ********************************************************************************
    std::cout << "Provider Step (P.2): Offer Service" << std::endl;

    // before offering (which takes some time), we check, whether we shall already stop ...
    if (test_stop_token.stop_requested())
    {
        return;
    }

    const auto offer_service_result =
        OfferService<TestServiceSkeleton>("Provider", service_instance, check_point_control);
    if (!(offer_service_result.has_value()))
    {
        return;
    }

    // ********************************************************************************
    // Step (P.3) - Wait for proceed trigger from Controller
    // ********************************************************************************
    std::cout << "Provider Step (P.3): Wait for proceed trigger from Controller" << std::endl;
    auto wait_for_child_proceed_result = WaitForChildProceed(check_point_control, test_stop_token);
    if (wait_for_child_proceed_result != CheckPointControl::ProceedInstruction::PROCEED_NEXT_CHECKPOINT)
    {
        std::cerr << "Provider Step (P.3): Expected to get notification to continue to next checkpoint but got: "
                  << static_cast<int>(wait_for_child_proceed_result) << std::endl;
        check_point_control.ErrorOccurred();
        return;
    }
    // ********************************************************************************
    // Step (P.4) - Call StopOffer on the service instance (skeleton)
    // ********************************************************************************
    std::cerr << "Provider Step (P.4): Stopping service offering." << std::endl;
    service_instance.StopOfferService();

    // ********************************************************************************
    // Step (P.5) - Checkpoint(1) reached - notify controller
    // ********************************************************************************
    std::cerr << "Provider Step (P.5): Notifying controller, that checkpoint(1) has been reached." << std::endl;
    check_point_control.CheckPointReached(1);

    // ********************************************************************************
    // Step (P.6) - Wait for proceed trigger from Controller to indicate that provider can finish.
    // ********************************************************************************
    std::cerr << "Provider Step (P.6): Wait for proceed trigger from Controller to indicate that provider can finish."
              << std::endl;
    wait_for_child_proceed_result = WaitForChildProceed(check_point_control, test_stop_token);
    if (wait_for_child_proceed_result != CheckPointControl::ProceedInstruction::FINISH_ACTIONS)
    {
        std::cerr << "Provider Step (P.6): Expected to get notification to finish but got: "
                  << static_cast<int>(wait_for_child_proceed_result) << std::endl;
        check_point_control.ErrorOccurred();
        return;
    }
    std::cerr << "Provider: Finishing Actions!" << std::endl;
}

}  // namespace score::mw::com::test
