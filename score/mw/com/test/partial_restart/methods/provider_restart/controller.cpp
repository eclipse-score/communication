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
#include "score/mw/com/test/partial_restart/methods/provider_restart/controller.h"
#include "score/mw/com/test/partial_restart/methods/provider_restart/consumer.h"
#include "score/mw/com/test/partial_restart/methods/provider_restart/provider.h"

#include "score/mw/com/test/common_test_resources/check_point_control.h"
#include "score/mw/com/test/common_test_resources/child_process_guard.h"
#include "score/mw/com/test/common_test_resources/general_resources.h"
#include "score/mw/com/test/common_test_resources/shared_memory_object_creator.h"

#include <score/stop_token.hpp>

#include <chrono>
#include <csignal>
#include <iostream>
#include <string_view>

namespace score::mw::com::test
{

namespace
{

const std::chrono::seconds kMaxWaitTimeToReachCheckpoint{10U};

const std::string_view kShmProviderCheckpointControlFileName = "methods_provider_restart_provider_checkpoint_file";
const std::string_view kShmConsumerCheckpointControlFileName = "methods_provider_restart_consumer_checkpoint_file";
const std::string_view kConsumerCheckpointControlName = "Consumer";
const std::string_view kProviderCheckpointControlName = "Provider";
constexpr std::uint8_t kCheckpointMethodCallsSucceeded{1U};
constexpr std::uint8_t kCheckpointServiceStopped{2U};
constexpr std::uint8_t kCheckpointServiceReappeared{3U};
constexpr std::uint8_t kCheckpointMethodsCalledAfterRestart{4U};

}  // namespace

int DoProviderNormalRestartWithProxy(score::cpp::stop_token test_stop_token, int argc, const char** argv) noexcept
{
    ObjectCleanupGuard object_cleanup_guard{};

    // Step (1) - Fork consumer process
    std::cerr << "Controller Step (1) - Fork consumer process" << std::endl;
    auto consumer_checkpoint_control_guard_result = CreateSharedCheckPointControl(
        "Controller", kShmConsumerCheckpointControlFileName, kConsumerCheckpointControlName);
    if (!(consumer_checkpoint_control_guard_result.has_value()))
    {
        return EXIT_FAILURE;
    }
    auto& consumer_checkpoint_control = consumer_checkpoint_control_guard_result.value().GetObject();
    object_cleanup_guard.AddConsumerCheckpointControlGuard(consumer_checkpoint_control_guard_result.value());

    score::mw::com::test::ConsumerParameters consumer_params{true};  // with_proxy = true
    auto fork_consumer_pid_guard = score::mw::com::test::ForkProcessAndRunInChildProcess(
        "Controller Step (1)",
        "Consumer",
        [&consumer_checkpoint_control, &consumer_params, test_stop_token, argc, argv]() {
            score::mw::com::test::DoConsumerActions(
                consumer_checkpoint_control, test_stop_token, argc, argv, consumer_params);
        });
    if (!fork_consumer_pid_guard.has_value())
    {
        std::cerr << "Controller Step (1) failed, exiting." << std::endl;
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }
    object_cleanup_guard.AddForkConsumerGuard(fork_consumer_pid_guard.value());

    // Step (2) - Fork provider process
    std::cerr << "Controller Step (2) - Fork provider process" << std::endl;
    auto provider_checkpoint_control_guard_result = CreateSharedCheckPointControl(
        "Controller Step (2)", kShmProviderCheckpointControlFileName, kProviderCheckpointControlName);
    if (!(provider_checkpoint_control_guard_result.has_value()))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }
    auto& provider_checkpoint_control = provider_checkpoint_control_guard_result.value().GetObject();
    object_cleanup_guard.AddProviderCheckpointControlGuard(provider_checkpoint_control_guard_result.value());

    auto fork_provider_pid_guard = score::mw::com::test::ForkProcessAndRunInChildProcess(
        "Controller Step (2)", "Provider", [&provider_checkpoint_control, test_stop_token, argc, argv]() {
            score::mw::com::test::DoProviderActions(provider_checkpoint_control, test_stop_token, argc, argv);
        });
    if (!fork_provider_pid_guard.has_value())
    {
        std::cerr << "Controller Step (2) failed, exiting." << std::endl;
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }
    object_cleanup_guard.AddForkProviderGuard(fork_provider_pid_guard.value());

    score::mw::com::test::TimeoutSupervisor timeout_supervisor{};

    // Step (3) - Wait for provider to reach checkpoint (1) - service offered
    std::cerr << "Controller Step (3) - Waiting for provider to reach checkpoint 1" << std::endl;
    auto notification_happened = provider_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint(
            "Controller Step (3)", notification_happened, provider_checkpoint_control, kCheckpointMethodCallsSucceeded))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    // Step (4) - Wait for consumer to reach checkpoint (1) - methods called successfully
    std::cerr << "Controller Step (4) - Waiting for consumer to reach checkpoint 1" << std::endl;
    notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint(
            "Controller Step (4)", notification_happened, consumer_checkpoint_control, kCheckpointMethodCallsSucceeded))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    // Step (5) - Trigger Consumer to proceed
    std::cerr << "Controller Step (5) - Trigger Consumer to proceed to next checkpoint" << std::endl;
    consumer_checkpoint_control.ProceedToNextCheckpoint();

    // Step (6) - Trigger provider to proceed (will call StopOffer)
    std::cerr << "Controller Step (6) - Trigger provider to proceed to next checkpoint" << std::endl;
    provider_checkpoint_control.ProceedToNextCheckpoint();

    // Step (7) - Wait for provider to reach checkpoint (2) - StopOffer completed
    std::cerr << "Controller Step (7) - Waiting for provider to reach checkpoint 2" << std::endl;
    notification_happened = provider_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint(
            "Controller Step (7)", notification_happened, provider_checkpoint_control, kCheckpointServiceStopped))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }
    // Step (10b) - Trigger consumer to proceed and wait for service reappearance
    std::cerr << "Controller Step (10b) - Trigger consumer to wait for service reappearance" << std::endl;
    consumer_checkpoint_control.ProceedToNextCheckpoint();
    // Step (8) - Wait for consumer to reach checkpoint (2) - detected service disappearance
    std::cerr << "Controller Step (8) - Waiting for consumer to reach checkpoint 2" << std::endl;
    notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint(
            "Controller Step (8)", notification_happened, consumer_checkpoint_control, kCheckpointServiceStopped))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    // Step (9) - Tell provider to finish gracefully
    std::cerr << "Controller Step (9) - Tell provider to finish gracefully" << std::endl;
    provider_checkpoint_control.FinishActions();

    // Step (10) - Wait for provider to exit
    std::cerr << "Controller Step (10) - Wait for provider to exit" << std::endl;
    if (!score::mw::com::test::WaitForChildProcessToTerminate(
            "Controller Step (10)", fork_provider_pid_guard.value(), kMaxWaitTimeToReachCheckpoint))
    {
        std::cerr << "Controller Step (10) Provider did not exit in time!" << std::endl;
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }
    std::cerr << "Controller Step (10) Provider exited successfully" << std::endl;

    // Step (11) - Restart provider
    std::cerr << "Controller Step (11) - Restarting provider" << std::endl;
    fork_provider_pid_guard = score::mw::com::test::ForkProcessAndRunInChildProcess(
        "Controller Step (11)", "Provider", [&provider_checkpoint_control, test_stop_token, argc, argv]() {
            score::mw::com::test::DoProviderActions(provider_checkpoint_control, test_stop_token, argc, argv);
        });
    if (!fork_provider_pid_guard.has_value())
    {
        std::cerr << "Controller Step (11) failed, exiting." << std::endl;
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }
    object_cleanup_guard.AddForkProviderGuard(fork_provider_pid_guard.value());

    // Step (12) - Wait for provider to reach checkpoint (1) again - service offered
    std::cerr << "Controller Step (12) - Waiting for restarted provider to reach checkpoint 1" << std::endl;
    notification_happened = provider_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Controller Step (12)",
                          notification_happened,
                          provider_checkpoint_control,
                          kCheckpointMethodCallsSucceeded))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    // Step (12b) - Trigger consumer to proceed and wait for service reappearance
    std::cerr << "Controller Step (12b) - Trigger consumer to wait for service reappearance" << std::endl;
    consumer_checkpoint_control.ProceedToNextCheckpoint();

    // Step (13) - Wait for consumer to reach checkpoint (3) - detected service reappearance
    std::cerr << "Controller Step (13) - Waiting for consumer to reach checkpoint 3" << std::endl;
    notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint(
            "Controller: Step (13)", notification_happened, consumer_checkpoint_control, kCheckpointServiceReappeared))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    // Consumer will now create new proxy and call methods to reach checkpoint 4
    // No trigger needed here - consumer already proceeded from Step 9 wait with the Step 12b trigger

    // Step (14) - Wait for consumer to reach checkpoint (4) - methods called successfully after restart
    std::cerr << "Controller Step (14) - Waiting for consumer to reach checkpoint 4" << std::endl;
    notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Controller: Step (14)",
                          notification_happened,
                          consumer_checkpoint_control,
                          kCheckpointMethodsCalledAfterRestart))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    // Step (15) - Tell both consumer and provider to finish
    std::cerr << "Controller Step (15) - Tell consumer and provider to finish" << std::endl;
    consumer_checkpoint_control.FinishActions();
    provider_checkpoint_control.FinishActions();

    std::cerr << "Controller: Test completed successfully!" << std::endl;
    object_cleanup_guard.CleanUp();
    return EXIT_SUCCESS;
}

int DoProviderNormalRestartWithoutProxy(score::cpp::stop_token test_stop_token, int argc, const char** argv) noexcept
{
    ObjectCleanupGuard object_cleanup_guard{};

    // Similar to with-proxy test but consumer doesn't create proxy
    std::cerr << "Controller Step (1) - Fork consumer process (without proxy)" << std::endl;
    auto consumer_checkpoint_control_guard_result = CreateSharedCheckPointControl(
        "Controller", kShmConsumerCheckpointControlFileName, kConsumerCheckpointControlName);
    if (!(consumer_checkpoint_control_guard_result.has_value()))
    {
        return EXIT_FAILURE;
    }
    auto& consumer_checkpoint_control = consumer_checkpoint_control_guard_result.value().GetObject();
    object_cleanup_guard.AddConsumerCheckpointControlGuard(consumer_checkpoint_control_guard_result.value());

    score::mw::com::test::ConsumerParameters consumer_params{false};  // with_proxy = false
    auto fork_consumer_pid_guard = score::mw::com::test::ForkProcessAndRunInChildProcess(
        "Controller Step (1)",
        "Consumer",
        [&consumer_checkpoint_control, &consumer_params, test_stop_token, argc, argv]() {
            score::mw::com::test::DoConsumerActions(
                consumer_checkpoint_control, test_stop_token, argc, argv, consumer_params);
        });
    if (!fork_consumer_pid_guard.has_value())
    {
        std::cerr << "Controller: Step (1) failed, exiting." << std::endl;
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }
    object_cleanup_guard.AddForkConsumerGuard(fork_consumer_pid_guard.value());

    std::cerr << "Controller Step (2) - Fork provider process" << std::endl;
    auto provider_checkpoint_control_guard_result = CreateSharedCheckPointControl(
        "Controller Step (2)", kShmProviderCheckpointControlFileName, kProviderCheckpointControlName);
    if (!(provider_checkpoint_control_guard_result.has_value()))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }
    auto& provider_checkpoint_control = provider_checkpoint_control_guard_result.value().GetObject();
    object_cleanup_guard.AddProviderCheckpointControlGuard(provider_checkpoint_control_guard_result.value());

    auto fork_provider_pid_guard = score::mw::com::test::ForkProcessAndRunInChildProcess(
        "Controller Step (2)", "Provider", [&provider_checkpoint_control, test_stop_token, argc, argv]() {
            score::mw::com::test::DoProviderActions(provider_checkpoint_control, test_stop_token, argc, argv);
        });
    if (!fork_provider_pid_guard.has_value())
    {
        std::cerr << "Controller: Step (2) failed, exiting." << std::endl;
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }
    object_cleanup_guard.AddForkProviderGuard(fork_provider_pid_guard.value());

    score::mw::com::test::TimeoutSupervisor timeout_supervisor{};

    // Wait for both to reach checkpoint 1
    std::cerr << "Controller Step (3) - Waiting for provider checkpoint 1" << std::endl;
    auto notification_happened = provider_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Controller: Step (3)",
                          notification_happened,
                          provider_checkpoint_control,
                          kCheckpointMethodCallsSucceeded))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    std::cerr << "Controller Step (4) - Waiting for consumer checkpoint 1" << std::endl;
    notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Controller: Step (4)",
                          notification_happened,
                          consumer_checkpoint_control,
                          kCheckpointMethodCallsSucceeded))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    // Trigger provider restart sequence
    std::cerr << "Controller Step (5) - Trigger consumer and provider to proceed" << std::endl;
    consumer_checkpoint_control.ProceedToNextCheckpoint();
    provider_checkpoint_control.ProceedToNextCheckpoint();

    std::cerr << "Controller Step (6) - Waiting for provider checkpoint 2" << std::endl;
    notification_happened = provider_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint(
            "Controller: Step (6)", notification_happened, provider_checkpoint_control, kCheckpointServiceStopped))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    std::cerr << "Controller Step (7) - Waiting for consumer checkpoint 2" << std::endl;
    notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint(
            "Controller: Step (7)", notification_happened, consumer_checkpoint_control, kCheckpointServiceStopped))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    // Restart provider
    std::cerr << "Controller Step (8) - Tell provider to finish" << std::endl;
    provider_checkpoint_control.FinishActions();

    std::cerr << "Controller Step (9) - Wait for provider to exit" << std::endl;

    if (!score::mw::com::test::WaitForChildProcessToTerminate(
            "Controller", fork_provider_pid_guard.value(), kMaxWaitTimeToReachCheckpoint))
    {
        std::cerr << "Controller: Provider did not exit in time!" << std::endl;
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    std::cerr << "Controller Step (10) - Restarting provider" << std::endl;
    fork_provider_pid_guard = score::mw::com::test::ForkProcessAndRunInChildProcess(
        "Controller Step (10)", "Provider", [&provider_checkpoint_control, test_stop_token, argc, argv]() {
            score::mw::com::test::DoProviderActions(provider_checkpoint_control, test_stop_token, argc, argv);
        });
    if (!fork_provider_pid_guard.has_value())
    {
        std::cerr << "Controller: Step (10) failed, exiting." << std::endl;
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }
    object_cleanup_guard.AddForkProviderGuard(fork_provider_pid_guard.value());

    std::cerr << "Controller Step (11) - Waiting for restarted provider checkpoint 1" << std::endl;
    notification_happened = provider_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Controller: Step (11)",
                          notification_happened,
                          provider_checkpoint_control,
                          kCheckpointMethodCallsSucceeded))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    // Step (11b) - Trigger consumer to proceed and wait for service reappearance
    std::cerr << "Controller Step (11b) - Trigger consumer to wait for service reappearance" << std::endl;
    consumer_checkpoint_control.ProceedToNextCheckpoint();

    std::cerr << "Controller Step (12) - Waiting for consumer checkpoint 3" << std::endl;
    notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint(
            "Controller: Step (12)", notification_happened, consumer_checkpoint_control, kCheckpointServiceReappeared))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    // Finish both
    std::cerr << "Controller Step (13) - Tell consumer and provider to finish" << std::endl;
    consumer_checkpoint_control.FinishActions();
    provider_checkpoint_control.FinishActions();

    std::cerr << "Controller: Test completed successfully!" << std::endl;
    object_cleanup_guard.CleanUp();
    return EXIT_SUCCESS;
}

int DoProviderKillRestartWithProxy(score::cpp::stop_token test_stop_token, int argc, const char** argv) noexcept
{
    // Similar to normal restart but kills provider instead of graceful shutdown
    ObjectCleanupGuard object_cleanup_guard{};

    std::cerr << "Controller Step (1) - Fork consumer process" << std::endl;
    auto consumer_checkpoint_control_guard_result = CreateSharedCheckPointControl(
        "Controller", kShmConsumerCheckpointControlFileName, kConsumerCheckpointControlName);
    if (!(consumer_checkpoint_control_guard_result.has_value()))
    {
        return EXIT_FAILURE;
    }
    auto& consumer_checkpoint_control = consumer_checkpoint_control_guard_result.value().GetObject();
    object_cleanup_guard.AddConsumerCheckpointControlGuard(consumer_checkpoint_control_guard_result.value());

    score::mw::com::test::ConsumerParameters consumer_params{true};
    auto fork_consumer_pid_guard = score::mw::com::test::ForkProcessAndRunInChildProcess(
        "Controller Step (1)",
        "Consumer",
        [&consumer_checkpoint_control, &consumer_params, test_stop_token, argc, argv]() {
            score::mw::com::test::DoConsumerActions(
                consumer_checkpoint_control, test_stop_token, argc, argv, consumer_params);
        });
    if (!fork_consumer_pid_guard.has_value())
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }
    object_cleanup_guard.AddForkConsumerGuard(fork_consumer_pid_guard.value());

    std::cerr << "Controller Step (2) - Fork provider process" << std::endl;
    auto provider_checkpoint_control_guard_result = CreateSharedCheckPointControl(
        "Controller Step (2)", kShmProviderCheckpointControlFileName, kProviderCheckpointControlName);
    if (!(provider_checkpoint_control_guard_result.has_value()))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }
    auto& provider_checkpoint_control = provider_checkpoint_control_guard_result.value().GetObject();
    object_cleanup_guard.AddProviderCheckpointControlGuard(provider_checkpoint_control_guard_result.value());

    auto fork_provider_pid_guard = score::mw::com::test::ForkProcessAndRunInChildProcess(
        "Controller Step (2)", "Provider", [&provider_checkpoint_control, test_stop_token, argc, argv]() {
            score::mw::com::test::DoProviderActions(provider_checkpoint_control, test_stop_token, argc, argv);
        });
    if (!fork_provider_pid_guard.has_value())
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }
    object_cleanup_guard.AddForkProviderGuard(fork_provider_pid_guard.value());

    score::mw::com::test::TimeoutSupervisor timeout_supervisor{};

    // Wait for initial setup
    std::cerr << "Controller Step (3) - Waiting for provider checkpoint 1" << std::endl;
    auto notification_happened = provider_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Controller: Step (3)",
                          notification_happened,
                          provider_checkpoint_control,
                          kCheckpointMethodCallsSucceeded))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    std::cerr << "Controller Step (4) - Waiting for consumer checkpoint 1" << std::endl;
    notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Controller: Step (4)",
                          notification_happened,
                          consumer_checkpoint_control,
                          kCheckpointMethodCallsSucceeded))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    // KILL provider instead of graceful shutdown
    std::cerr << "Controller Step (5) - KILLING provider process" << std::endl;

    const auto kill_result = fork_provider_pid_guard.value().KillChildProcess();
    if (!kill_result)
    {
        std::cerr << "Controller: Step (5) failed. Error killing provider child process" << std::endl;
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    std::cerr << "Controller Step (6) - Waiting for provider to exit" << std::endl;
    if (!score::mw::com::test::WaitForChildProcessToTerminate(
            "Controller", fork_provider_pid_guard.value(), kMaxWaitTimeToReachCheckpoint))
    {
        std::cerr << "Controller: Provider did not exit after SIGKILL!" << std::endl;
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    // Trigger consumer to detect disappearance
    std::cerr << "Controller Step (7) - Trigger consumer to proceed" << std::endl;
    consumer_checkpoint_control.ProceedToNextCheckpoint();

    std::cerr << "Controller Step (8.1) - Waiting for consumer checkpoint 2" << std::endl;
    // PERF: This step takes roughly 5 seconds to complete, which is two orders of magnitude longer than any other
    // segment of the test. If the test keeps timing out then this should be investigated.
    notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint(
            "Controller: Step (8.2)", notification_happened, consumer_checkpoint_control, kCheckpointServiceStopped))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }
    // Restart provider
    std::cerr << "Controller Step (9) - Restarting provider after kill" << std::endl;
    fork_provider_pid_guard = score::mw::com::test::ForkProcessAndRunInChildProcess(
        "Controller Step (9)", "Provider", [&provider_checkpoint_control, test_stop_token, argc, argv]() {
            score::mw::com::test::DoProviderActions(provider_checkpoint_control, test_stop_token, argc, argv);
        });
    if (!fork_provider_pid_guard.has_value())
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }
    object_cleanup_guard.AddForkProviderGuard(fork_provider_pid_guard.value());

    std::cerr << "Controller Step (10) - Waiting for restarted provider checkpoint 1" << std::endl;
    notification_happened = provider_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Controller: Step (10)",
                          notification_happened,
                          provider_checkpoint_control,
                          kCheckpointMethodCallsSucceeded))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    // Step (10b) - Trigger consumer to proceed from Step 7 wait and enter WaitTillServiceAppears
    std::cerr << "Controller Step (10b) - Trigger consumer to proceed to wait for service reappearance" << std::endl;
    consumer_checkpoint_control.ProceedToNextCheckpoint();

    std::cerr << "Controller Step (11) - Waiting for consumer checkpoint 3" << std::endl;
    notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint(
            "Controller: Step (11)", notification_happened, consumer_checkpoint_control, kCheckpointServiceReappeared))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    // Now trigger consumer to proceed from Step 9 wait and create new proxy
    std::cerr << "Controller Step (12) - Trigger consumer to create new proxy and call methods" << std::endl;
    consumer_checkpoint_control.ProceedToNextCheckpoint();

    std::cerr << "Controller Step (12) - Waiting for consumer checkpoint 4" << std::endl;
    notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);

    if (!VerifyCheckpoint("Controller: Step (13)",
                          notification_happened,
                          consumer_checkpoint_control,
                          kCheckpointMethodsCalledAfterRestart))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    std::cerr << "Controller Step (14) - Trigger consumer to finish" << std::endl;
    consumer_checkpoint_control.FinishActions();
    provider_checkpoint_control.FinishActions();

    std::cerr << "Controller: Test completed successfully!" << std::endl;
    object_cleanup_guard.CleanUp();
    return EXIT_SUCCESS;
}

int DoProviderKillRestartWithoutProxy(score::cpp::stop_token test_stop_token, int argc, const char** argv) noexcept
{
    // Combination of without-proxy and kill variants
    ObjectCleanupGuard object_cleanup_guard{};

    std::cerr << "Controller Step (1) - Fork consumer process (without proxy)" << std::endl;
    auto consumer_checkpoint_control_guard_result = CreateSharedCheckPointControl(
        "Controller", kShmConsumerCheckpointControlFileName, kConsumerCheckpointControlName);
    if (!(consumer_checkpoint_control_guard_result.has_value()))
    {
        return EXIT_FAILURE;
    }
    auto& consumer_checkpoint_control = consumer_checkpoint_control_guard_result.value().GetObject();
    object_cleanup_guard.AddConsumerCheckpointControlGuard(consumer_checkpoint_control_guard_result.value());

    score::mw::com::test::ConsumerParameters consumer_params{false};
    auto fork_consumer_pid_guard = score::mw::com::test::ForkProcessAndRunInChildProcess(
        "Controller Step (1)",
        "Consumer",
        [&consumer_checkpoint_control, &consumer_params, test_stop_token, argc, argv]() {
            score::mw::com::test::DoConsumerActions(
                consumer_checkpoint_control, test_stop_token, argc, argv, consumer_params);
        });
    if (!fork_consumer_pid_guard.has_value())
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }
    object_cleanup_guard.AddForkConsumerGuard(fork_consumer_pid_guard.value());

    std::cerr << "Controller Step (2) - Fork provider process" << std::endl;
    auto provider_checkpoint_control_guard_result = CreateSharedCheckPointControl(
        "Controller Step (2)", kShmProviderCheckpointControlFileName, kProviderCheckpointControlName);
    if (!(provider_checkpoint_control_guard_result.has_value()))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }
    auto& provider_checkpoint_control = provider_checkpoint_control_guard_result.value().GetObject();
    object_cleanup_guard.AddProviderCheckpointControlGuard(provider_checkpoint_control_guard_result.value());

    auto fork_provider_pid_guard = score::mw::com::test::ForkProcessAndRunInChildProcess(
        "Controller Step (2)", "Provider", [&provider_checkpoint_control, test_stop_token, argc, argv]() {
            score::mw::com::test::DoProviderActions(provider_checkpoint_control, test_stop_token, argc, argv);
        });
    if (!fork_provider_pid_guard.has_value())
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }
    object_cleanup_guard.AddForkProviderGuard(fork_provider_pid_guard.value());

    score::mw::com::test::TimeoutSupervisor timeout_supervisor{};

    std::cerr << "Controller Step (3) - Waiting for provider checkpoint 1" << std::endl;
    auto notification_happened = provider_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Controller: Step (3)",
                          notification_happened,
                          provider_checkpoint_control,
                          kCheckpointMethodCallsSucceeded))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    std::cerr << "Controller Step (4) - Waiting for consumer checkpoint 1" << std::endl;
    notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Controller: Step (4)",
                          notification_happened,
                          consumer_checkpoint_control,
                          kCheckpointMethodCallsSucceeded))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    // KILL provider
    std::cerr << "Controller Step (5) - KILLING provider process" << std::endl;

    if (kill(fork_provider_pid_guard.value().GetPid(), SIGKILL) != 0)
    {
        std::cerr << "Controller: Failed to kill provider!" << std::endl;
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    std::cerr << "Controller Step (6) - Waiting for provider to exit" << std::endl;
    if (!score::mw::com::test::WaitForChildProcessToTerminate(
            "Controller", fork_provider_pid_guard.value(), kMaxWaitTimeToReachCheckpoint))
    {
        std::cerr << "Controller: Provider did not exit after SIGKILL!" << std::endl;
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    std::cerr << "Controller Step (7) - Trigger consumer to proceed" << std::endl;
    consumer_checkpoint_control.ProceedToNextCheckpoint();

    std::cerr << "Controller Step (8) - Waiting for consumer checkpoint 2" << std::endl;
    notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Controller: Step (8)", notification_happened, consumer_checkpoint_control, 2))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    // Restart provider
    std::cerr << "Controller Step (9) - Restarting provider after kill" << std::endl;
    fork_provider_pid_guard = score::mw::com::test::ForkProcessAndRunInChildProcess(
        "Controller Step (9)", "Provider", [&provider_checkpoint_control, test_stop_token, argc, argv]() {
            score::mw::com::test::DoProviderActions(provider_checkpoint_control, test_stop_token, argc, argv);
        });
    if (!fork_provider_pid_guard.has_value())
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }
    object_cleanup_guard.AddForkProviderGuard(fork_provider_pid_guard.value());

    std::cerr << "Controller Step (10) - Waiting for restarted provider checkpoint 1" << std::endl;
    notification_happened = provider_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Controller Step (10)",
                          notification_happened,
                          provider_checkpoint_control,
                          kCheckpointMethodCallsSucceeded))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    // Step (10b) - Trigger consumer to proceed and wait for service reappearance
    std::cerr << "Controller Step (10b) - Trigger consumer to wait for service reappearance" << std::endl;
    consumer_checkpoint_control.ProceedToNextCheckpoint();

    std::cerr << "Controller Step (11) - Waiting for consumer checkpoint 3" << std::endl;
    notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint(
            "Controller Step (11)", notification_happened, consumer_checkpoint_control, kCheckpointServiceReappeared))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    std::cerr << "Controller Step (12) - Tell consumer and provider to finish" << std::endl;
    consumer_checkpoint_control.FinishActions();
    provider_checkpoint_control.FinishActions();

    std::cerr << "Controller: Test completed successfully!" << std::endl;
    object_cleanup_guard.CleanUp();
    return EXIT_SUCCESS;
}

}  // namespace score::mw::com::test
