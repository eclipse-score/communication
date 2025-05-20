/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
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

#include "score/mw/com/test/common_test_resources/check_point_control.h"
#include "score/mw/com/test/common_test_resources/general_resources.h"
#include "score/mw/com/test/common_test_resources/provider_resources.h"
#include "score/mw/com/test/common_test_resources/stop_token_sig_term_handler.h"
#include "score/mw/com/test/partial_restart/test_datatype.h"

namespace bmw::mw::com::test
{

    namespace
    {
        constexpr auto kInstanceSpecifierString{"partial_restart/small_but_great"};
        const auto kInstanceSpecifier = bmw::mw::com::InstanceSpecifier::Create(kInstanceSpecifierString).value();
        const std::chrono::milliseconds kDelayBetweenSendEvents{20U};
    } // namespace

    void PerformProviderActions(CheckPointControl &check_point_control, amp::stop_token stop_token)
    {
        //***************************************************
        // Step (1)- create and offer service
        //***************************************************
        auto skeleton_result =
            CreateSkeleton<TestServiceSkeleton>("Provider Step (1)", kInstanceSpecifierString, check_point_control);
        if (!skeleton_result.has_value())
        {
            return;
        }
        auto &service_instance = skeleton_result.value();
        const auto offer_service_result =
            OfferService<TestServiceSkeleton>("Provider Step (1)", service_instance, check_point_control);
        if (!offer_service_result.has_value())
        {
            return;
        }

        //*********************************************************
        // Step (2)- send samples till FINISH_ACTIONS is requested
        //*********************************************************
        while (!stop_token.stop_requested())
        {
            auto result = service_instance.simple_event_.Send({1U, 42U});
            if (!result.has_value())
            {
                std::cout << "Provider Step(2): Sending of event failed: " << result.error() << "\n";
                check_point_control.ErrorOccurred();
                return;
            }
            std::this_thread::sleep_for(kDelayBetweenSendEvents);

            const auto proceed_instruction = check_point_control.GetProceedInstruction();
            if (proceed_instruction == CheckPointControl::ProceedInstruction::FINISH_ACTIONS)
            {
                break;
            }
            if (proceed_instruction != CheckPointControl::ProceedInstruction::STILL_PROCESSING)
            {
                std::cerr << "Provider Step (2): Unexpected proceed instruction received: "
                          << static_cast<int>(proceed_instruction) << std::endl;
                check_point_control.ErrorOccurred();
                return;
            }
        }
        std::cout << "Provider: Finishing actions!" << std::endl;
    }

} // namespace bmw::mw::com::test
