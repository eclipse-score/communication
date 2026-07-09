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
#include "score/mw/com/test/partial_restart/methods/provider_restart/provider.h"
#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/partial_restart/methods/provider_restart/test_checkpoints.h"
#include "score/mw/com/test/partial_restart/methods/test_method_datatype.h"

#include "score/mw/com/test/common_test_resources/general_resources.h"
#include "score/mw/com/test/common_test_resources/skeleton_container.h"

#include "score/mw/com/runtime.h"
#include "score/mw/com/types.h"

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
        std::cout << "Provider: Error occurred, notifying controller." << std::endl;
        check_point_control.ErrorOccurred();
    }};

    std::cout << "Provider: Initializing LoLa/mw::com runtime from passed service-instance manifest ..." << std::endl;
    mw::com::runtime::InitializeRuntime(mw::com::runtime::RuntimeConfiguration{std::string{service_instance_manifest}});
    std::cout << "Provider: Initializing LoLa/mw::com runtime done." << std::endl;

    // ********************************************************************************
    // Step (1) - Create service instance/skeleton
    // ********************************************************************************
    std::cout << "Provider Step (1): Create service instance/skeleton" << std::endl;
    constexpr auto instance_specifier_string{"partial_restart/methods/provider_restart"};
    auto instance_specifier_result = InstanceSpecifier::Create(std::string{instance_specifier_string});
    if (!instance_specifier_result.has_value())
    {
        FailTest("Provider Step (1): Create service instance/skeleton failed with error: ",
                 instance_specifier_result.error());
    }

    SkeletonContainer<TestMethodServiceSkeleton> skeleton_container{};
    skeleton_container.CreateSkeleton(instance_specifier_result.value(), "Provider");
    auto& service_instance = skeleton_container.GetSkeleton();

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
    // Step (3) - Offer Service. `Checkpoint` (1) is reached when service is offered - notify to `Controller`.
    // ********************************************************************************
    std::cout << "Provider Step (3): Offer service and notify controller" << std::endl;
    if (test_stop_token.stop_requested())
    {
        return;
    }

    skeleton_container.OfferService("Provider");
    std::cout << "Provider: Service instance is offered." << std::endl;
    check_point_control.CheckPointReached(kCheckpointMethodCallsSucceeded);

    // ********************************************************************************
    // Step (4) - Wait for proceed trigger from Controller
    // ********************************************************************************
    std::cout << "Provider Step (4): Wait for proceed trigger from controller" << std::endl;
    auto wait_for_child_proceed_result = WaitForChildProceed(check_point_control, test_stop_token);
    if (wait_for_child_proceed_result != CheckPointControl::ProceedInstruction::PROCEED_NEXT_CHECKPOINT)
    {
        FailTest("Provider Step (4): Expected to get notification to continue to next checkpoint but got: ",
                 ToString(wait_for_child_proceed_result));
    }

    // ********************************************************************************
    // Step (5) - Stop offering the service (StopOffer)
    // ********************************************************************************
    std::cout << "Provider Step (5): Stop offering the service" << std::endl;
    service_instance.StopOfferService();

    // ********************************************************************************
    // Step (6) - Checkpoint(2) reached - notify controller
    // ********************************************************************************
    std::cout << "Provider Step (6): Notify controller that checkpoint 2 has been reached" << std::endl;
    check_point_control.CheckPointReached(kCheckpointServiceStopped);

    // ********************************************************************************
    // Step (7) - Wait for Controller trigger to finish.
    // ********************************************************************************
    std::cout << "Provider Step (7): Wait for controller trigger to finish" << std::endl;
    wait_for_child_proceed_result = WaitForChildProceed(check_point_control, test_stop_token);
    if (wait_for_child_proceed_result != CheckPointControl::ProceedInstruction::FINISH_ACTIONS)
    {
        FailTest("Provider Step (7): Wait for controller trigger to finish failed, expected FINISH_ACTIONS but got: ",
                 ToString(wait_for_child_proceed_result));
    }

    check_point_control_error_guard.Release();
    std::cout << "Provider: Exiting gracefully. Total method calls: " << method_call_count.load() << std::endl;
}

}  // namespace score::mw::com::test
