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
#include "score/mw/com/test/common_test_resources/child_process_guard.h"
#include "score/mw/com/test/common_test_resources/general_resources.h"
#include "score/mw/com/test/common_test_resources/provider_resources.h"
#include "score/mw/com/test/common_test_resources/shared_memory_object_guard.h"
#include "score/mw/com/test/common_test_resources/stop_token_sig_term_handler.h"
#include "score/mw/com/test/partial_restart/test_datatype.h"
#include "score/mw/com/types.h"
#include <cstdlib>
#include <iostream>

using namespace bmw::mw::com::test;

const auto kShmSkeletonCheckpointControlFileName = "skeleton_checks_number_of_allocations_checkpoint_file";
const amp::string_view kSkeletonCheckpointControlName = "Skeleton";
constexpr int kMaxNumSamples{10};
const auto kInstanceSpecifier = "partial_restart/small_but_great";
const std::chrono::seconds kMaxWaitTimeToReachCheckpoint{30U};

void PerformProviderActions(CheckPointControl &check_point_control, amp::stop_token stop_token)
{
    // *********************************************
    // Step (1)- Provider: create and offer service
    // *********************************************
    auto skeleton_result =
        CreateSkeleton<TestServiceSkeleton>("Provider Step(1):", kInstanceSpecifier, check_point_control);
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

    // *********************************************
    // Step (2)- Allocate the maximum number of samples allowed by the configuration
    // *********************************************
    std::vector<bmw::mw::com::SampleAllocateePtr<SimpleEventDatatype>> sample_ptrs{};
    for (int allocated_samples = 0; allocated_samples < kMaxNumSamples; allocated_samples++)
    {
        auto sample_result = service_instance.simple_event_.Allocate();
        if (!sample_result.has_value())
        {
            std::cout << "Provider Step (2): Allocating of sample failed" << sample_result.error() << "\n";
            check_point_control.ErrorOccurred();
            return;
        }
        sample_ptrs.push_back(std::move(sample_result.value()));
    }

    // *********************************************
    // Step (3)- Try to Allocate one more sample. This shall fail.
    // *********************************************
    auto sample_result = service_instance.simple_event_.Allocate();
    if (sample_result.has_value())
    {
        std::cout << "Provider Step (3): Allocating one additional sample. This should not be possible."
                  << "\n";
        check_point_control.ErrorOccurred();
        return;
    }

    // *********************************************
    // Step (4)- Provider: ACK check point
    // *********************************************
    check_point_control.CheckPointReached(1);

    // *********************************************
    // Step (5)- Wait for Controller command to proceed
    // *********************************************
    std::cout << "Provider Step (5): waiting for proceed\n";
    if (WaitForChildProceed(check_point_control, stop_token) != CheckPointControl::ProceedInstruction::FINISH_ACTIONS)
    {
        std::cerr << "Consumer Step (5): Received proceed-trigger from controller, but expected finish-trigger!"
                  << std::endl;
        check_point_control.ErrorOccurred();
        return;
    }
    std::cout << "Provider Step (5): after waiting for proceed\n";
}

int main()
{
    // Prerequisites for the test steps/sequence
    ObjectCleanupGuard object_cleanup_guard{};
    amp::stop_source test_stop_source{};
    const bool sig_term_handler_setup_success = bmw::mw::com::SetupStopTokenSigTermHandler(test_stop_source);
    if (!sig_term_handler_setup_success)
    {
        std::cout << "Test main: Unable to set signal handler for SIGINT and/or SIGTERM.\n";
        return EXIT_FAILURE;
    }

    // *********************************************
    // Step (1)- Create a check point
    // *********************************************
    auto skeleton_checkpoint_control_result = CreateSharedCheckPointControl(
        "Controller Step(1):", kShmSkeletonCheckpointControlFileName, kSkeletonCheckpointControlName);
    if (!(skeleton_checkpoint_control_result.has_value()))
    {
        return EXIT_FAILURE;
    }
    auto &skeleton_check_point_control = skeleton_checkpoint_control_result.value().GetObject();
    object_cleanup_guard.AddProviderCheckpointControlGuard(skeleton_checkpoint_control_result.value());

    // *********************************************
    // Step (2)- fork provider
    // *********************************************
    auto first_process_pid = ForkProcessAndRunInChildProcess(
        "Controller Step (2):", "Provider:", [&skeleton_check_point_control, &test_stop_source]()
        { PerformProviderActions(skeleton_check_point_control, test_stop_source.get_token()); });
    if (!first_process_pid.has_value())
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }
    object_cleanup_guard.AddForkProviderGuard(first_process_pid.value());

    // *********************************************
    // Step (3)- wait till provider has ACK check point
    // *********************************************
    if ((WaitAndVerifyCheckPoint("Controller Step (3):",
                                 skeleton_check_point_control,
                                 1,
                                 test_stop_source.get_token(),
                                 kMaxWaitTimeToReachCheckpoint) != EXIT_SUCCESS))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    // *********************************************
    // Step (4)- kill provider
    // *********************************************
    std::cout << "Controller Step (4): killing provider\n";
    if (!first_process_pid.value().KillChildProcess())
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    // *********************************************
    // Step (5)- fork provider again
    // *********************************************
    auto second_process_pid = ForkProcessAndRunInChildProcess(
        "Controller Step (5):", "Provider:", [&skeleton_check_point_control, &test_stop_source]()
        { PerformProviderActions(skeleton_check_point_control, test_stop_source.get_token()); });
    if (!second_process_pid.has_value())
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }
    object_cleanup_guard.AddForkProviderGuard(second_process_pid.value());

    // *********************************************
    // Step (6)- wait till provider has ACK check point
    // *********************************************
    if ((WaitAndVerifyCheckPoint("Controller Step (6):",
                                 skeleton_check_point_control,
                                 1,
                                 test_stop_source.get_token(),
                                 kMaxWaitTimeToReachCheckpoint) != EXIT_SUCCESS))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    // *********************************************
    // Step (7)- tell provider to finish
    // *********************************************
    std::cout << "Controller Step (7): tell provider to finish\n";
    skeleton_check_point_control.FinishActions();
    std::cout << "Controller Step (7): After provider FinishActions Call\n";

    // *********************************************
    // Step (8)- Wait for provider to terminate
    // *********************************************
    const auto provider_terminated = WaitForChildProcessToTerminate(
        "Controller: Step (8)", second_process_pid.value(), kMaxWaitTimeToReachCheckpoint);
    if (!provider_terminated)
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    if (!object_cleanup_guard.CleanUp())
    {
        std::cerr << "Controller Step (8): cleanup failed\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
