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

#include "score/mw/com/test/partial_restart/consumer_restart/provider.h"

#include "score/mw/com/test/common_test_resources/check_point_control.h"
#include "score/mw/com/test/common_test_resources/general_resources.h"
#include "score/mw/com/test/common_test_resources/provider_resources.h"
#include "score/mw/com/test/partial_restart/test_datatype.h"

#include "score/mw/com/runtime.h"

#include <chrono>
#include <iostream>
#include <thread>

namespace score::mw::com::test
{

namespace
{

constexpr auto kSkeletonInstanceSpecifierString{"partial_restart/small_but_great"};

const std::chrono::milliseconds kSampleSendCycleTime{40};

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
    const auto offer_service_result =
        OfferService<TestServiceSkeleton>("Provider", service_instance, check_point_control);
    if (!(offer_service_result.has_value()))
    {
        return;
    }

    SimpleEventDatatype event_data{1U, 1U};
    while (!stop_token.stop_requested())
    {
        // ********************************************************************************
        // Step (P.3) - Send data until controller triggers to finish
        // ********************************************************************************
        auto result = service_instance.simple_event_.Send(event_data);
        event_data.member_1++;
        event_data.member_2++;
        if (!result.has_value())
        {
            std::cerr << "Provider Step (P.3): Sending of event failed: " << result.error().Message() << std::endl;
        }
        else
        {
            std::cout << "Provider Step (P.3): Sent data: (" << event_data.member_1 << ", " << event_data.member_2
                      << ")" << std::endl;
        }
        std::this_thread::sleep_for(kSampleSendCycleTime);

        const auto proceed_instruction = check_point_control.GetProceedInstruction();
        if (proceed_instruction == CheckPointControl::ProceedInstruction::FINISH_ACTIONS)
        {
            break;
        }
        if (proceed_instruction != CheckPointControl::ProceedInstruction::STILL_PROCESSING)
        {
            std::cerr << "Provider Step (P.3): Unexpected proceed instruction received: "
                      << static_cast<int>(proceed_instruction) << std::endl;
            check_point_control.ErrorOccurred();
            return;
        }
    }
    std::cout << "Provider: Finishing actions!" << std::endl;
}

}  // namespace score::mw::com::test
