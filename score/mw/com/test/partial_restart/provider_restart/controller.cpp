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

#include "score/mw/com/test/partial_restart/provider_restart/controller.h"

#include "score/mw/com/test/common_test_resources/check_point_control.h"
#include "score/mw/com/test/common_test_resources/child_process_guard.h"
#include "score/mw/com/test/common_test_resources/general_resources.h"
#include "score/mw/com/test/common_test_resources/shared_memory_object_creator.h"
#include "score/mw/com/test/partial_restart/provider_restart/consumer.h"
#include "score/mw/com/test/partial_restart/provider_restart/provider.h"

#include <amp_optional.hpp>
#include <amp_string_view.hpp>

#include <unistd.h>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <sstream>

namespace bmw::mw::com::test
{

    namespace
    {

        const std::chrono::seconds kMaxWaitTimeToReachCheckpoint{30U};

        const auto kShmProviderCheckpointControlFileName = "provider_restart_application_provider_checkpoint_file";
        const auto kShmConsumerCheckpointControlFileName = "provider_restart_application_consumer_checkpoint_file";
        const amp::string_view kConsumerCheckpointControlName = "Consumer";
        const amp::string_view kProviderCheckpointControlName = "Provider";

    } // namespace

    int DoProviderNormalRestartSubscribedProxy(amp::stop_token test_stop_token, int argc, const char **argv) noexcept
    {
        // Resources that need to be cleaned up on process exit
        ObjectCleanupGuard object_cleanup_guard{};

        // ********************************************************************************
        // Begin of test steps/sequence.
        // These are now the test steps, which the Controller (our main) does.
        // @see test/partial_restart/README.md#controller-process-activity
        // ********************************************************************************

        // ********************************************************************************
        // Step (1) - Fork consumer process and set up checkpoint-communication-objects in
        //            controller and consumer process to be able to communicate between
        //            them.
        // ********************************************************************************

        // Create the non-RAII consumer CheckPointControl in the controller process. It will be duplicated in the consumer
        // process. It must be manually cleaned up in all exit paths.
        auto consumer_checkpoint_control_guard_result = CreateSharedCheckPointControl(
            "Controller", kShmConsumerCheckpointControlFileName, kConsumerCheckpointControlName);
        if (!(consumer_checkpoint_control_guard_result.has_value()))
        {
            return EXIT_FAILURE;
        }
        auto &consumer_checkpoint_control = consumer_checkpoint_control_guard_result.value().GetObject();
        object_cleanup_guard.AddConsumerCheckpointControlGuard(consumer_checkpoint_control_guard_result.value());

        bmw::mw::com::test::ConsumerParameters consumer_params{true};
        auto fork_consumer_pid_guard = bmw::mw::com::test::ForkProcessAndRunInChildProcess(
            "Controller Step (1)",
            "Consumer",
            [&consumer_checkpoint_control, &consumer_params, test_stop_token, argc, argv]()
            {
                bmw::mw::com::test::DoConsumerActions(
                    consumer_checkpoint_control, test_stop_token, argc, argv, consumer_params);
            });
        if (!fork_consumer_pid_guard.has_value())
        {
            std::cerr << "Controller: Step (1) failed, exiting." << std::endl;
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }
        object_cleanup_guard.AddForkConsumerGuard(fork_consumer_pid_guard.value());

        // ********************************************************************************
        // Step (2) - Fork provider process and set up checkpoint-communication-objects in
        //            controller and provider process be able o communicate between them.
        // ********************************************************************************

        // Create the non-RAII provider CheckPointControl in the controller process. It will be duplicated in the provider
        // process. It must be manually cleaned up in all exit paths.
        auto provider_checkpoint_control_guard_result = CreateSharedCheckPointControl(
            "Controller Step (2)", kShmProviderCheckpointControlFileName, kProviderCheckpointControlName);
        if (!(provider_checkpoint_control_guard_result.has_value()))
        {
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }
        auto &provider_checkpoint_control = provider_checkpoint_control_guard_result.value().GetObject();
        object_cleanup_guard.AddProviderCheckpointControlGuard(provider_checkpoint_control_guard_result.value());

        auto fork_provider_pid_guard = bmw::mw::com::test::ForkProcessAndRunInChildProcess(
            "Controller Step (2)", "Provider", [&provider_checkpoint_control, test_stop_token, argc, argv]()
            { bmw::mw::com::test::DoProviderActions(provider_checkpoint_control, test_stop_token, argc, argv); });
        if (!fork_provider_pid_guard.has_value())
        {
            std::cerr << "Controller: Step (2) failed, exiting." << std::endl;
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }
        object_cleanup_guard.AddForkProviderGuard(fork_provider_pid_guard.value());

        // TimeoutSupervisor only needed in controller! And since it creates a thread on construction, which wouldn't
        // be handled in fork() it is also mandatory, to create it only after the children have been forked!
        bmw::mw::com::test::TimeoutSupervisor timeout_supervisor{};
        // ********************************************************************************
        // Step (3) - Wait for provider to reach checkpoint (1)
        // ********************************************************************************
        std::cout << "Controller: Waiting for provider to reach checkpoint 1" << std::endl;
        auto notification_happened = provider_checkpoint_control.WaitForCheckpointReachedOrError(
            kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
        if (!VerifyCheckpoint("Controller: Step (3)", notification_happened, provider_checkpoint_control, 1))
        {
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }

        // ********************************************************************************
        // Step (4) - Wait for consumer to reach checkpoint (1)
        // ********************************************************************************
        std::cout << "Controller: Waiting for consumer to reach checkpoint 1" << std::endl;
        notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
            kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
        if (!VerifyCheckpoint("Controller: Step (4)", notification_happened, consumer_checkpoint_control, 1))
        {
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }

        // ********************************************************************************
        // Step (5) - Trigger Consumer to proceed to next checkpoint (consumer now starts
        //            waiting for event subscription state switching to subscription-pending
        // ********************************************************************************
        consumer_checkpoint_control.ProceedToNextCheckpoint();

        // ********************************************************************************
        // Step (6) - Trigger provider to proceed to next checkpoint (provider will call
        //            StopOffer now)
        // ********************************************************************************
        provider_checkpoint_control.ProceedToNextCheckpoint();

        // ********************************************************************************
        // Step (7) - Wait for provider to reach checkpoint (2) - StopOffer has been
        //            successfully called.
        // ********************************************************************************
        notification_happened = provider_checkpoint_control.WaitForCheckpointReachedOrError(
            kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
        if (!VerifyCheckpoint("Controller: Step (7)", notification_happened, provider_checkpoint_control, 2))
        {
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }

        // ********************************************************************************
        // Step (8) - Wait for consumer to reach checkpoint (2) - subscription state
        //            switched to subscription-pending.
        // ********************************************************************************
        notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
            kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
        if (!VerifyCheckpoint("Controller: Step (8)", notification_happened, consumer_checkpoint_control, 2))
        {
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }

        // ********************************************************************************
        // Step (9) - Trigger provider to proceed to finish (provider will
        //            terminate now)
        // ********************************************************************************
        provider_checkpoint_control.FinishActions();

        // ********************************************************************************
        // Step (10) - Wait for provider process to terminate
        // ********************************************************************************
        const auto provider_terminated = WaitForChildProcessToTerminate(
            "Controller: Step (10)", fork_provider_pid_guard.value(), kMaxWaitTimeToReachCheckpoint);
        if (!provider_terminated)
        {
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }

        // Hyper cautious: Reset notification subsystem within provider_checkpoint_control as terminated provider might
        // have left it in an intermediate state. Need to clean it up before the next/to be forked provider will re-use it.
        provider_checkpoint_control.ResetCheckpointReachedNotifications();
        provider_checkpoint_control.ResetProceedNotifications();

        // ********************************************************************************
        // Step (11) - Trigger Consumer to proceed to next checkpoint (consumer now starts
        //            waiting for event subscription state switching to subscribed
        // ********************************************************************************
        consumer_checkpoint_control.ProceedToNextCheckpoint();

        // ********************************************************************************
        // Step (12) - (Re)Fork the Provider process
        // ********************************************************************************
        fork_provider_pid_guard = bmw::mw::com::test::ForkProcessAndRunInChildProcess(
            "Controller Step (12)", "Provider", [&provider_checkpoint_control, test_stop_token, argc, argv]()
            { bmw::mw::com::test::DoProviderActions(provider_checkpoint_control, test_stop_token, argc, argv); });
        if (!fork_provider_pid_guard.has_value())
        {
            std::cerr << "Controller: Step (12) failed, exiting." << std::endl;
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }

        // ********************************************************************************
        // Step (13) - Wait for (re-forked) provider to reach checkpoint (1)
        // ********************************************************************************
        notification_happened = provider_checkpoint_control.WaitForCheckpointReachedOrError(
            kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
        if (!VerifyCheckpoint("Controller: Step (13)", notification_happened, provider_checkpoint_control, 1))
        {
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }

        // ********************************************************************************
        // Step (14) - Wait for consumer to reach checkpoint (3) - subscription state
        //            switched to subscribed.
        // ********************************************************************************
        notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
            kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
        if (!VerifyCheckpoint("Controller: Step (14)", notification_happened, consumer_checkpoint_control, 3))
        {
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }

        // ********************************************************************************
        // Step (15) - Trigger Consumer to proceed to next checkpoint (consumer now starts
        //            receiving N samples
        // ********************************************************************************
        consumer_checkpoint_control.ProceedToNextCheckpoint();

        // ********************************************************************************
        // Step (16) - Wait for consumer to reach checkpoint (4) - reception of N samples
        //             succeeded.
        // ********************************************************************************
        notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
            kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
        if (!VerifyCheckpoint("Controller: Step (16)", notification_happened, consumer_checkpoint_control, 4))
        {
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }

        // ********************************************************************************
        // Step (17) - Trigger Consumer to terminate.
        // ********************************************************************************
        consumer_checkpoint_control.FinishActions();

        // ********************************************************************************
        // Step (18) - Wait for Consumer process to terminate
        // ********************************************************************************
        const auto consumer_terminated = WaitForChildProcessToTerminate(
            "Controller: Step (18)", fork_consumer_pid_guard.value(), kMaxWaitTimeToReachCheckpoint);
        if (!consumer_terminated)
        {
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }

        // ********************************************************************************
        // Step (19) - Trigger Provider to terminate.
        // ********************************************************************************
        provider_checkpoint_control.FinishActions();

        // ********************************************************************************
        // Step (20) - Wait for provider process to terminate
        // ********************************************************************************
        const auto restarted_provider_terminated = WaitForChildProcessToTerminate(
            "Controller: Step (20)", fork_provider_pid_guard.value(), kMaxWaitTimeToReachCheckpoint);
        if (!restarted_provider_terminated)
        {
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }

        std::cerr << "Controller: Test sequence finished with SUCCESS!" << std::endl;
        object_cleanup_guard.CleanUp();
        return EXIT_SUCCESS;
    }

    int DoProviderNormalRestartNoProxy(amp::stop_token /*test_stop_token*/, int /*argc*/, const char ** /*argv*/) noexcept
    {
        return EXIT_SUCCESS;
    }

    int DoProviderCrashRestartSubscribedProxy(amp::stop_token test_stop_token, int argc, const char **argv) noexcept
    {
        // Resources that need to be cleaned up on process exit
        ObjectCleanupGuard object_cleanup_guard{};

        // ********************************************************************************
        // Begin of test steps/sequence.
        // These are now the test steps, which the Controller (our main) does.
        // @see test/partial_restart/README.md#controller-process-activity
        // ********************************************************************************

        // ********************************************************************************
        // Step (1) - Fork consumer process and set up checkpoint-communication-objects in
        //            controller and consumer process be able o communicate between them.
        // ********************************************************************************

        // Create the non-RAII consumer CheckPointControl in the controller process. It will be duplicated in the consumer
        // process. It must be manually cleaned up in all exit paths.
        auto consumer_checkpoint_control_guard_result = CreateSharedCheckPointControl(
            "Controller", kShmConsumerCheckpointControlFileName, kConsumerCheckpointControlName);
        if (!(consumer_checkpoint_control_guard_result.has_value()))
        {
            return EXIT_FAILURE;
        }
        auto &consumer_checkpoint_control = consumer_checkpoint_control_guard_result.value().GetObject();
        object_cleanup_guard.AddConsumerCheckpointControlGuard(consumer_checkpoint_control_guard_result.value());

        bmw::mw::com::test::ConsumerParameters consumer_params{true};
        auto fork_consumer_pid_guard = bmw::mw::com::test::ForkProcessAndRunInChildProcess(
            "Controller Step (1)",
            "Consumer",
            [&consumer_checkpoint_control, &consumer_params, test_stop_token, argc, argv]()
            {
                bmw::mw::com::test::DoConsumerActions(
                    consumer_checkpoint_control, test_stop_token, argc, argv, consumer_params);
            });
        if (!fork_consumer_pid_guard.has_value())
        {
            std::cerr << "Controller: Step (1) failed, exiting." << std::endl;
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }
        object_cleanup_guard.AddForkConsumerGuard(fork_consumer_pid_guard.value());

        std::cerr << "Controller: Consumer process forked successfully with PID: "
                  << fork_consumer_pid_guard.value().GetPid() << std::endl;

        // ********************************************************************************
        // Step (2) - Fork provider process and set up checkpoint-communication-objects in
        //            controller and provider process be able o communicate between them.
        // ********************************************************************************

        // Create the non-RAII provider CheckPointControl in the controller process. It will be duplicated in the provider
        // process. It must be manually cleaned up in all exit paths.
        auto provider_checkpoint_control_guard_result = CreateSharedCheckPointControl(
            "Controller Step (2)", kShmProviderCheckpointControlFileName, kProviderCheckpointControlName);
        if (!(provider_checkpoint_control_guard_result.has_value()))
        {
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }
        auto &provider_checkpoint_control = provider_checkpoint_control_guard_result.value().GetObject();
        object_cleanup_guard.AddProviderCheckpointControlGuard(provider_checkpoint_control_guard_result.value());

        auto fork_provider_pid_guard = bmw::mw::com::test::ForkProcessAndRunInChildProcess(
            "Controller Step (2)", "Provider", [&provider_checkpoint_control, test_stop_token, argc, argv]()
            { bmw::mw::com::test::DoProviderActions(provider_checkpoint_control, test_stop_token, argc, argv); });
        if (!fork_provider_pid_guard.has_value())
        {
            std::cerr << "Controller: Step (2) failed, exiting." << std::endl;
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }
        object_cleanup_guard.AddForkProviderGuard(fork_provider_pid_guard.value());

        // TimeoutSupervisor only needed in controller! And since it creates a thread on construction, which wouldn't
        // be handled in fork() it is also mandatory, to create it only after the children have been forked!
        bmw::mw::com::test::TimeoutSupervisor timeout_supervisor{};
        // ********************************************************************************
        // Step (3) - Wait for provider to reach checkpoint (1)
        // ********************************************************************************
        auto notification_happened = provider_checkpoint_control.WaitForCheckpointReachedOrError(
            kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
        if (!VerifyCheckpoint("Controller: Step (3)", notification_happened, provider_checkpoint_control, 1))
        {
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }

        // ********************************************************************************
        // Step (4) - Wait for consumer to reach checkpoint (1)
        // ********************************************************************************
        notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
            kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
        if (!VerifyCheckpoint("Controller: Step (4)", notification_happened, consumer_checkpoint_control, 1))
        {
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }

        // ********************************************************************************
        // Step (5) - Trigger Consumer to proceed to next checkpoint (consumer now starts
        //            waiting for event subscription state switching to subscription-pending
        // ********************************************************************************
        consumer_checkpoint_control.ProceedToNextCheckpoint();

        // ********************************************************************************
        // Step (6) and (7) - Kill provider process and wait for its death. Steps are
        //                    combined here as KillChildProcess() includes both steps.
        // ********************************************************************************
        const auto kill_result = fork_provider_pid_guard.value().KillChildProcess();
        if (!kill_result)
        {
            std::cerr << "Controller: Step (6)/(7) failed. Error killing provider child process" << std::endl;
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }

        // since KillChildProcess() is a combination of kill and waitpid() after it returns true,
        // we are sure, that the provider process is dead. Therefore, the previous step is a
        // combined step 6/7 according to the README.md reference.

        // ********************************************************************************
        // Step (8) - (Re)Fork the Provider process
        // ********************************************************************************
        fork_provider_pid_guard = bmw::mw::com::test::ForkProcessAndRunInChildProcess(
            "Controller Step (8)", "Provider", [&provider_checkpoint_control, test_stop_token, argc, argv]()
            { bmw::mw::com::test::DoProviderActions(provider_checkpoint_control, test_stop_token, argc, argv); });
        if (!fork_provider_pid_guard.has_value())
        {
            std::cerr << "Controller: Step (8) failed, exiting. Error (re)forking provider." << std::endl;
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }

        // ********************************************************************************
        // Step (9) - Wait for (re-forked) provider to reach checkpoint (1)
        // ********************************************************************************
        notification_happened = provider_checkpoint_control.WaitForCheckpointReachedOrError(
            kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
        if (!VerifyCheckpoint("Controller: Step (9)", notification_happened, provider_checkpoint_control, 1))
        {
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }

        // ********************************************************************************
        // Step (10) - Wait for consumer to reach checkpoint (2) - subscription state
        //            switched to subscription-pending.
        // ********************************************************************************
        notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
            kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
        if (!VerifyCheckpoint("Controller: Step (10)", notification_happened, consumer_checkpoint_control, 2))
        {
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }

        // ********************************************************************************
        // Step (11) - Trigger Consumer to proceed to next checkpoint (consumer now starts
        //            waiting for event subscription state switching to subscribed.
        // ********************************************************************************
        consumer_checkpoint_control.ProceedToNextCheckpoint();

        // ********************************************************************************
        // Step (12) - Wait for consumer to reach checkpoint (3) - subscription state
        //            switched to subscribed.
        // ********************************************************************************
        notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
            kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
        if (!VerifyCheckpoint("Controller: Step (12)", notification_happened, consumer_checkpoint_control, 3))
        {
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }

        // ********************************************************************************
        // Step (13) - Trigger Consumer to proceed to next checkpoint (consumer now starts
        //            receiving N samples
        // ********************************************************************************
        consumer_checkpoint_control.ProceedToNextCheckpoint();

        // ********************************************************************************
        // Step (14) - Wait for consumer to reach checkpoint (4) - reception of N samples
        //             succeeded.
        // ********************************************************************************
        notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
            kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
        if (!VerifyCheckpoint("Controller: Step (14)", notification_happened, consumer_checkpoint_control, 4))
        {
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }

        // ********************************************************************************
        // Step (15) - Trigger Consumer to terminate.
        // ********************************************************************************
        consumer_checkpoint_control.FinishActions();

        // ********************************************************************************
        // Step (16) - Wait for Consumer process to terminate
        // ********************************************************************************
        const auto consumer_terminated = WaitForChildProcessToTerminate(
            "Controller: Step (16)", fork_consumer_pid_guard.value(), kMaxWaitTimeToReachCheckpoint);
        if (!consumer_terminated)
        {
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }

        // ********************************************************************************
        // Step (17) - Trigger Provider to terminate.
        // ********************************************************************************
        provider_checkpoint_control.FinishActions();

        // ********************************************************************************
        // Step (18) - Wait for provider process to terminate
        // ********************************************************************************
        const auto provider_terminated = WaitForChildProcessToTerminate(
            "Controller: Step (18)", fork_provider_pid_guard.value(), kMaxWaitTimeToReachCheckpoint);
        if (!provider_terminated)
        {
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }

        std::cerr << "Controller: Test sequence finished with SUCCESS!" << std::endl;
        object_cleanup_guard.CleanUp();
        return EXIT_SUCCESS;
    }

    int DoProviderCrashRestartNoProxy(amp::stop_token /*test_stop_token*/, int /*argc*/, const char ** /*argv*/) noexcept
    {
        // not yet implemented.
        return EXIT_SUCCESS;
    }

} // namespace bmw::mw::com::test
