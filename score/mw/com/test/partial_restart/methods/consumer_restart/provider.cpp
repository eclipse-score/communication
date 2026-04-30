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
#include "score/mw/com/test/partial_restart/methods/consumer_restart/provider.h"
#include "score/mw/com/test/partial_restart/methods/test_method_datatype.h"

#include "score/mw/com/test/common_test_resources/general_resources.h"
#include "score/mw/com/test/common_test_resources/provider_resources.h"

#include "score/mw/com/runtime.h"
#include "score/mw/com/types.h"

#include <score/stop_token.hpp>

#include <atomic>
#include <cstdint>
#include <iostream>

namespace score::mw::com::test
{

namespace
{

std::atomic<std::uint32_t> method_call_count{0U};

// Method handler that adds two numbers
std::int32_t TestMethodHandler(std::int32_t a, std::int32_t b) noexcept
{
    method_call_count++;
    std::cout << "Provider: Method called with arguments (" << a << ", " << b
              << "), call count: " << method_call_count.load() << std::endl;
    return a + b;
}

}  // namespace

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
    // Step (1) - Create service instance/skeleton
    // ********************************************************************************
    constexpr auto instance_specifier_string{"partial_restart/methods/consumer_restart"};
    std::cerr << "Provider: Before Skeleton Creation." << std::endl;
    auto skeleton_wrapper_result =
        CreateSkeleton<TestMethodServiceSkeleton>("Provider", instance_specifier_string, check_point_control);
    if (!(skeleton_wrapper_result.has_value()))
    {
        return;
    }
    auto& service_instance = skeleton_wrapper_result.value();

    // ********************************************************************************
    // Step (2) - Register method handler
    // ********************************************************************************
    std::cerr << "Provider: Registering method handler." << std::endl;
    auto register_result = service_instance.test_method_.RegisterHandler([](std::int32_t a, std::int32_t b) noexcept {
        return TestMethodHandler(a, b);
    });
    if (!register_result.has_value())
    {
        std::cerr << "Provider: Failed to register method handler: " << register_result.error().Message() << std::endl;
        check_point_control.ErrorOccurred();
        return;
    }
    std::cerr << "Provider: Method handler registered successfully." << std::endl;

    // ********************************************************************************
    // Step (3) - Offer Service. Checkpoint (1) is reached when service is offered - notify to Controller.
    // ********************************************************************************
    if (test_stop_token.stop_requested())
    {
        return;
    }

    auto offer_service_result =
        OfferService<TestMethodServiceSkeleton>("Provider", service_instance, check_point_control);
    if (!(offer_service_result.has_value()))
    {
        return;
    }
    std::cerr << "Provider: Service instance is offered." << std::endl;
    check_point_control.CheckPointReached(1);

    // ********************************************************************************
    // Step (4) - Wait for controller notification to finish.
    //            Provider stays up the entire time while consumers restart.
    // ********************************************************************************
    auto wait_for_child_proceed_result = WaitForChildProceed(check_point_control, test_stop_token);
    if (wait_for_child_proceed_result != CheckPointControl::ProceedInstruction::FINISH_ACTIONS)
    {
        std::cerr << "Provider: Expected to get notification to finish gracefully but got: "
                  << static_cast<std::uint8_t>(wait_for_child_proceed_result) << std::endl;
        check_point_control.ErrorOccurred();
        return;
    }

    // ********************************************************************************
    // Step (5) - StopOffer and exit
    // ********************************************************************************
    service_instance.StopOfferService();
    std::cerr << "Provider: Exiting gracefully. Total method calls: " << method_call_count << std::endl;
}

}  // namespace score::mw::com::test
