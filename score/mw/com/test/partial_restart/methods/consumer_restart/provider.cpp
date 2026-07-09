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
#include "score/mw/com/test/partial_restart/methods/consumer_restart/test_checkpoints.h"
#include "score/mw/com/test/partial_restart/methods/test_method_datatype.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/general_resources.h"
#include "score/mw/com/test/common_test_resources/provider_resources.h"

#include "score/mw/com/runtime.h"

#include <score/stop_token.hpp>

#include <atomic>
#include <cstdint>
#include <iostream>

namespace score::mw::com::test
{

void DoProviderActions(CheckPointControl& check_point_control,
                       score::cpp::stop_token test_stop_token,
                       std::string_view service_instance_manifest) noexcept
{
    ExitFunctionGuard check_point_control_error_guard{[&check_point_control]() {
        check_point_control.ErrorOccurred();
    }};

    std::cout << "Provider: Initializing LoLa/mw::com runtime from passed service-instance manifest ..." << std::endl;
    mw::com::runtime::InitializeRuntime(mw::com::runtime::RuntimeConfiguration{std::string{service_instance_manifest}});

    // ********************************************************************************
    // Step (1) - Create service instance/skeleton
    // ********************************************************************************
    std::cout << "Provider Step (1): Create service instance/skeleton" << std::endl;
    constexpr auto instance_specifier_string{"partial_restart/methods/consumer_restart"};
    auto skeleton_wrapper_result =
        CreateSkeleton<TestMethodServiceSkeleton>("Provider", instance_specifier_string, check_point_control);
    if (!(skeleton_wrapper_result.has_value()))
    {
        FailTest("Provider Step (1): Create service instance/skeleton failed with error: ",
                 skeleton_wrapper_result.error());
    }
    auto& service_instance = skeleton_wrapper_result.value();

    // ********************************************************************************
    // Step (2) - Register method handler
    // ********************************************************************************
    std::cout << "Provider Step (2): Register method handler" << std::endl;
    std::atomic<std::uint32_t> method_call_count{0U};
    auto register_result = service_instance.test_method_.RegisterHandler(
        [&method_call_count](std::int32_t& return_value, const std::int32_t& a, const std::int32_t& b) noexcept {
            method_call_count++;
            std::cout << "Provider: Method called with arguments (" << a << ", " << b
                      << "), call count: " << method_call_count.load() << std::endl;
            return_value = a + b;
        });

    if (!register_result.has_value())
    {
        FailTest("Provider Step (2): Register method handler failed with error: ", register_result.error());
    }

    // ********************************************************************************
    // Step (3) - Offer Service. Checkpoint (1) is reached when service is offered - notify to Controller.
    // ********************************************************************************
    std::cout << "Provider Step (3): Offer service and notify controller" << std::endl;
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
    check_point_control.CheckPointReached(kCheckpointMethodCallsSucceeded);

    // ********************************************************************************
    // Step (4) - Wait for controller notification to finish.
    //            Provider stays up the entire time while consumers restart.
    // ********************************************************************************
    std::cout << "Provider Step (4): Wait for controller notification to finish" << std::endl;
    auto wait_for_child_proceed_result = WaitForChildProceed(check_point_control, test_stop_token);
    if (wait_for_child_proceed_result != CheckPointControl::ProceedInstruction::FINISH_ACTIONS)
    {
        FailTest(
            "Provider Step (4): Wait for controller notification to finish failed, expected FINISH_ACTIONS but got: ",
            ToString(wait_for_child_proceed_result));
    }

    // ********************************************************************************
    // Step (5) - StopOffer and exit
    // ********************************************************************************
    std::cout << "Provider Step (5): StopOffer and exit" << std::endl;
    service_instance.StopOfferService();

    check_point_control_error_guard.Release();
    std::cout << "Provider: Exiting gracefully. Total method calls: " << method_call_count << std::endl;
}

}  // namespace score::mw::com::test
