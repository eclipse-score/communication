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
#include "score/mw/com/test/partial_restart/methods/consumer_restart/controller.h"
#include "score/mw/com/test/partial_restart/methods/consumer_restart/consumer.h"
#include "score/mw/com/test/partial_restart/methods/consumer_restart/provider.h"
#include "score/mw/com/test/partial_restart/methods/consumer_restart/test_checkpoints.h"

#include "score/mw/com/test/common_test_resources/check_point_control.h"
#include "score/mw/com/test/common_test_resources/child_process_guard.h"
#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/general_resources.h"
#include "score/mw/com/test/common_test_resources/shared_memory_object_creator.h"

#include <score/stop_token.hpp>

#include <chrono>
#include <iostream>
#include <string_view>

namespace score::mw::com::test
{

namespace
{

const std::chrono::seconds kMaxWaitTimeToReachCheckpoint{30U};

const std::string_view kShmProviderCheckpointControlFileName = "methods_consumer_restart_provider_checkpoint_file";
const std::string_view kShmConsumerCheckpointControlFileName = "methods_consumer_restart_consumer_checkpoint_file";
const std::string_view kConsumerCheckpointControlName = "Consumer";
const std::string_view kProviderCheckpointControlName = "Provider";

}  // namespace

/// Consumer Normal Restart Flow:
///   1. Fork consumer (1st instance) → FindService, CreateProxy with methods, calls 5x
///   2. Fork provider → creates skeleton, offers service → provider checkpoint 1
///   3. Wait provider checkpoint 1, then wait consumer checkpoint 1 (methods verified)
///   4. Tell consumer to finish gracefully → wait for consumer to exit
///   5. Fork consumer (2nd instance) → FindService, CreateProxy without methods
///   6. Wait for second consumer checkpoint 1 (reconnection verified)
///   7. Tell consumer and provider to finish
void DoConsumerNormalRestart(score::cpp::stop_token test_stop_token,
                             std::string_view service_instance_manifest) noexcept
{
    ObjectCleanupGuard object_cleanup_guard{};

    // ********************************************************************************
    // Step (1) - Fork first consumer
    // ********************************************************************************
    std::cout << "Controller Step (1): Fork first consumer instance (with methods)" << std::endl;
    auto consumer_checkpoint_control_guard_result = CreateSharedCheckPointControl(
        "Controller Step (1)", kShmConsumerCheckpointControlFileName, kConsumerCheckpointControlName);
    if (!(consumer_checkpoint_control_guard_result.has_value()))
    {
        FailTest("Controller Step (1): Fork first consumer instance (with methods) failed.");
    }
    auto& consumer_checkpoint_control = consumer_checkpoint_control_guard_result.value().GetObject();
    object_cleanup_guard.AddConsumerCheckpointControlGuard(consumer_checkpoint_control_guard_result.value());

    auto fork_consumer_pid_guard = score::mw::com::test::ForkProcessAndRunInChildProcess(
        "Controller Step (1)",
        "Consumer",
        [&consumer_checkpoint_control, test_stop_token, service_instance_manifest]() {
            score::mw::com::test::DoConsumerActions(
                consumer_checkpoint_control, test_stop_token, service_instance_manifest);
        });
    if (!fork_consumer_pid_guard.has_value())
    {
        std::cout << "Controller Step (1): failed, exiting." << std::endl;
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (1): Fork first consumer instance (with methods) failed.");
    }
    object_cleanup_guard.AddForkConsumerGuard(fork_consumer_pid_guard.value());

    // ********************************************************************************
    // Step (2) - Fork provider process
    // ********************************************************************************
    std::cout << "Controller Step (2): Fork provider process" << std::endl;
    auto provider_checkpoint_control_guard_result = CreateSharedCheckPointControl(
        "Controller Step (2)", kShmProviderCheckpointControlFileName, kProviderCheckpointControlName);
    if (!(provider_checkpoint_control_guard_result.has_value()))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (2): Fork provider process failed.");
    }
    auto& provider_checkpoint_control = provider_checkpoint_control_guard_result.value().GetObject();
    object_cleanup_guard.AddProviderCheckpointControlGuard(provider_checkpoint_control_guard_result.value());

    auto fork_provider_pid_guard = score::mw::com::test::ForkProcessAndRunInChildProcess(
        "Controller Step (2)",
        "Provider",
        [&provider_checkpoint_control, test_stop_token, service_instance_manifest]() {
            score::mw::com::test::DoProviderActions(
                provider_checkpoint_control, test_stop_token, service_instance_manifest);
        });
    if (!fork_provider_pid_guard.has_value())
    {
        std::cout << "Controller Step (2): failed, exiting." << std::endl;
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (2): Fork provider process failed.");
    }
    object_cleanup_guard.AddForkProviderGuard(fork_provider_pid_guard.value());

    score::mw::com::test::TimeoutSupervisor timeout_supervisor{};

    // ********************************************************************************
    // Step (3) - Wait for provider to reach checkpoint (1) - service offered
    // ********************************************************************************
    std::cout << "Controller Step (3): Waiting for provider to reach checkpoint 1" << std::endl;
    auto notification_happened = provider_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Controller Step (3):",
                          notification_happened,
                          provider_checkpoint_control,
                          kCheckpointMethodCallsSucceeded))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (3): Waiting for provider to reach checkpoint 1 failed.");
    }

    // ********************************************************************************
    // Step (4) - Wait for first consumer to reach checkpoint (1) - methods called successfully
    // ********************************************************************************
    std::cout << "Controller Step (4): Waiting for first consumer to reach checkpoint 1" << std::endl;
    notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Controller Step (4):",
                          notification_happened,
                          consumer_checkpoint_control,
                          kCheckpointMethodCallsSucceeded))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (4): Waiting for first consumer to reach checkpoint 1 failed.");
    }

    // ********************************************************************************
    // Step (5) - Tell first consumer to finish gracefully and wait for exit
    // ********************************************************************************
    std::cout << "Controller Step (5): Tell first consumer to finish gracefully" << std::endl;
    consumer_checkpoint_control.FinishActions();

    std::cout << "Controller Step (5): Wait for first consumer to exit" << std::endl;
    if (!score::mw::com::test::WaitForChildProcessToTerminate(
            "Controller Step (5):", fork_consumer_pid_guard.value(), kMaxWaitTimeToReachCheckpoint))
    {
        std::cerr << "Controller: First consumer did not exit in time!" << std::endl;
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (5): Wait for first consumer to exit failed.");
    }

    if (consumer_checkpoint_control.HasErrorOccurred())
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (5): Tell consumer to finish failed because consumer an error.");
    }

    // Clear any stale proceed state before the restarted consumer reuses the same checkpoint control.
    consumer_checkpoint_control.ResetCheckpointReachedNotifications();
    consumer_checkpoint_control.ResetProceedNotifications();

    // ********************************************************************************
    // Step (6) - Fork second consumer
    // ********************************************************************************
    std::cout << "Controller Step (6): Fork second consumer instance (reconnect, no methods)" << std::endl;
    fork_consumer_pid_guard = score::mw::com::test::ForkProcessAndRunInChildProcess(
        "Controller Step (6)",
        "Consumer",
        [&consumer_checkpoint_control, test_stop_token, service_instance_manifest]() {
            score::mw::com::test::DoConsumerActions(
                consumer_checkpoint_control, test_stop_token, service_instance_manifest);
        });
    if (!fork_consumer_pid_guard.has_value())
    {
        std::cout << "Controller Step (6): failed, exiting." << std::endl;
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (6): Fork second consumer instance (reconnect, no methods) failed.");
    }
    object_cleanup_guard.AddForkConsumerGuard(fork_consumer_pid_guard.value());

    // ********************************************************************************
    // Step (7) - Wait for second consumer to reach checkpoint (1) - reconnection verified
    // ********************************************************************************
    std::cout << "Controller Step (7): Waiting for second consumer to reach checkpoint 1" << std::endl;
    notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Controller Step (7):",
                          notification_happened,
                          consumer_checkpoint_control,
                          kCheckpointMethodCallsSucceeded))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (7): Waiting for second consumer to reach checkpoint 1 failed.");
    }

    // ********************************************************************************
    // Step (8) - Tell both consumer and provider to finish
    // ********************************************************************************
    std::cout << "Controller Step (8): Tell consumer and provider to finish" << std::endl;
    consumer_checkpoint_control.FinishActions();
    provider_checkpoint_control.FinishActions();

    if (!score::mw::com::test::WaitForChildProcessToTerminate(
            "Controller Step (8):", fork_consumer_pid_guard.value(), kMaxWaitTimeToReachCheckpoint))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (8): Tell consumer and provider to finish failed.");
    }

    if (!score::mw::com::test::WaitForChildProcessToTerminate(
            "Controller Step (8):", fork_provider_pid_guard.value(), kMaxWaitTimeToReachCheckpoint))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (8): Tell consumer and provider to finish failed.");
    }

    if (consumer_checkpoint_control.HasErrorOccurred() || provider_checkpoint_control.HasErrorOccurred())
    {
        object_cleanup_guard.CleanUp();
        FailTest(
            "Controller Step (8): Tell consumer and provider to finish failed because consumer or provider reported an "
            "error.");
    }

    std::cout << "Controller: Consumer normal restart test completed successfully!" << std::endl;
    object_cleanup_guard.CleanUp();
}

/// Consumer Kill Restart Flow:
///   1. Fork consumer (1st instance) → FindService, CreateProxy with methods, calls 5x
///   2. Fork provider → creates skeleton, offers service → provider checkpoint 1
///   3. Wait provider checkpoint 1, then wait consumer checkpoint 1 (methods verified)
///   4. SIGKILL consumer → wait for consumer to exit
///   5. Fork consumer (2nd instance) → FindService, CreateProxy without methods
///   6. Wait for second consumer checkpoint 1 (reconnection verified)
///   7. Tell consumer and provider to finish
void DoConsumerKillRestart(score::cpp::stop_token test_stop_token, std::string_view service_instance_manifest) noexcept
{
    ObjectCleanupGuard object_cleanup_guard{};

    // ********************************************************************************
    // Step (1) - Fork first consumer
    // ********************************************************************************
    std::cout << "Controller Step (1): Fork first consumer instance (with methods)" << std::endl;
    auto consumer_checkpoint_control_guard_result = CreateSharedCheckPointControl(
        "Controller Step (1)", kShmConsumerCheckpointControlFileName, kConsumerCheckpointControlName);
    if (!(consumer_checkpoint_control_guard_result.has_value()))
    {
        FailTest("Controller Step (1): Fork first consumer instance (with methods) failed.");
    }
    auto& consumer_checkpoint_control = consumer_checkpoint_control_guard_result.value().GetObject();
    object_cleanup_guard.AddConsumerCheckpointControlGuard(consumer_checkpoint_control_guard_result.value());

    auto fork_consumer_pid_guard = score::mw::com::test::ForkProcessAndRunInChildProcess(
        "Controller Step (1)",
        "Consumer",
        [&consumer_checkpoint_control, test_stop_token, service_instance_manifest]() {
            score::mw::com::test::DoConsumerActions(
                consumer_checkpoint_control, test_stop_token, service_instance_manifest);
        });
    if (!fork_consumer_pid_guard.has_value())
    {
        std::cout << "Controller Step (1): failed, exiting." << std::endl;
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (1): Fork first consumer instance (with methods) failed.");
    }
    object_cleanup_guard.AddForkConsumerGuard(fork_consumer_pid_guard.value());

    // ********************************************************************************
    // Step (2) - Fork provider process
    // ********************************************************************************
    std::cout << "Controller Step (2): Fork provider process" << std::endl;
    auto provider_checkpoint_control_guard_result = CreateSharedCheckPointControl(
        "Controller Step (2)", kShmProviderCheckpointControlFileName, kProviderCheckpointControlName);
    if (!(provider_checkpoint_control_guard_result.has_value()))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (2): Fork provider process failed.");
    }
    auto& provider_checkpoint_control = provider_checkpoint_control_guard_result.value().GetObject();
    object_cleanup_guard.AddProviderCheckpointControlGuard(provider_checkpoint_control_guard_result.value());

    auto fork_provider_pid_guard = score::mw::com::test::ForkProcessAndRunInChildProcess(
        "Controller Step (2)",
        "Provider",
        [&provider_checkpoint_control, test_stop_token, service_instance_manifest]() {
            score::mw::com::test::DoProviderActions(
                provider_checkpoint_control, test_stop_token, service_instance_manifest);
        });
    if (!fork_provider_pid_guard.has_value())
    {
        std::cout << "Controller Step (2): failed, exiting." << std::endl;
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (2): Fork provider process failed.");
    }
    object_cleanup_guard.AddForkProviderGuard(fork_provider_pid_guard.value());

    score::mw::com::test::TimeoutSupervisor timeout_supervisor{};

    // ********************************************************************************
    // Step (3) - Wait for provider to reach checkpoint (1) - service offered
    // ********************************************************************************
    std::cout << "Controller Step (3): Waiting for provider to reach checkpoint 1" << std::endl;
    auto notification_happened = provider_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Controller Step (3):",
                          notification_happened,
                          provider_checkpoint_control,
                          kCheckpointMethodCallsSucceeded))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (3): Waiting for provider to reach checkpoint 1 failed.");
    }

    // ********************************************************************************
    // Step (4) - Wait for first consumer to reach checkpoint (1) - methods called successfully
    // ********************************************************************************
    std::cout << "Controller Step (4): Waiting for first consumer to reach checkpoint 1" << std::endl;
    notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Controller Step (4):",
                          notification_happened,
                          consumer_checkpoint_control,
                          kCheckpointMethodCallsSucceeded))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (4): Waiting for first consumer to reach checkpoint 1 failed.");
    }

    // ********************************************************************************
    // Step (5) - Kill consumer process and wait for its death.
    // ********************************************************************************
    std::cerr << "Controller Step (5): Kill consumer process and wait for its death" << std::endl;
    const auto kill_result = fork_consumer_pid_guard.value().KillChildProcess();
    if (!kill_result)
    {
        std::cerr << "Controller Step (5): failed. Error killing consumer child process" << std::endl;
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (5): Kill consumer process and wait for its death failed.");
    }

    // ********************************************************************************
    // Step (6) - Clear any stale proceed state before the restarted consumer reuses the same checkpoint control.
    // ********************************************************************************
    consumer_checkpoint_control.ResetCheckpointReachedNotifications();
    consumer_checkpoint_control.ResetProceedNotifications();

    // ********************************************************************************
    // Step (7) - Fork second consumer
    // ********************************************************************************
    std::cout << "Controller Step (7): Fork second consumer instance " << std::endl;
    fork_consumer_pid_guard = score::mw::com::test::ForkProcessAndRunInChildProcess(
        "Controller Step (7)",
        "Consumer",
        [&consumer_checkpoint_control, test_stop_token, service_instance_manifest]() {
            score::mw::com::test::DoConsumerActions(
                consumer_checkpoint_control, test_stop_token, service_instance_manifest);
        });
    if (!fork_consumer_pid_guard.has_value())
    {
        std::cout << "Controller Step (7): failed, exiting." << std::endl;
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (7): Fork second consumer instance failed.");
    }
    object_cleanup_guard.AddForkConsumerGuard(fork_consumer_pid_guard.value());

    // ********************************************************************************
    // Step (8) - Wait for second consumer to reach checkpoint (1) - methods called
    // ********************************************************************************
    std::cout << "Controller Step (8): Waiting for second consumer to reach checkpoint 1" << std::endl;
    notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Controller Step (8):",
                          notification_happened,
                          consumer_checkpoint_control,
                          kCheckpointMethodCallsSucceeded))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (8): Waiting for second consumer to reach checkpoint 1 failed.");
    }

    // ********************************************************************************
    // Step (9) - Tell both consumer and provider to finish
    // ********************************************************************************
    std::cout << "Controller Step (9): Tell consumer and provider to finish" << std::endl;
    consumer_checkpoint_control.FinishActions();
    provider_checkpoint_control.FinishActions();

    if (!score::mw::com::test::WaitForChildProcessToTerminate(
            "Controller Step (9):", fork_consumer_pid_guard.value(), kMaxWaitTimeToReachCheckpoint))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (9): Wait for consumer to finish.");
    }

    if (!score::mw::com::test::WaitForChildProcessToTerminate(
            "Controller Step (9):", fork_provider_pid_guard.value(), kMaxWaitTimeToReachCheckpoint))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (9): Wait for provider to finish.");
    }

    if (consumer_checkpoint_control.HasErrorOccurred() || provider_checkpoint_control.HasErrorOccurred())
    {
        object_cleanup_guard.CleanUp();
        FailTest(
            "Controller Step (9): Tell consumer and provider to finish failed because consumer or provider reported an "
            "error.");
    }

    std::cout << "Controller: Consumer kill restart test completed successfully!" << std::endl;
    object_cleanup_guard.CleanUp();
}

}  // namespace score::mw::com::test
