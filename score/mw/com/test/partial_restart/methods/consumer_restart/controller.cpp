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

#include "score/mw/com/test/common_test_resources/check_point_control.h"
#include "score/mw/com/test/common_test_resources/child_process_guard.h"
#include "score/mw/com/test/common_test_resources/general_resources.h"
#include "score/mw/com/test/common_test_resources/shared_memory_object_creator.h"

#include <score/stop_token.hpp>

#include <signal.h>
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
constexpr std::uint8_t kCheckpointMethodCallsSucceeded{1U};

}  // namespace

/// Consumer Normal Restart Flow:
///   1. Fork consumer (1st instance, enable_methods=true) → FindService, CreateProxy with methods, calls 5x
///   2. Fork provider → creates skeleton, offers service → provider checkpoint 1
///   3. Wait provider checkpoint 1, then wait consumer checkpoint 1 (methods verified)
///   4. Tell consumer to finish gracefully → wait for consumer to exit
///   5. Fork consumer (2nd instance, enable_methods=false) → FindService, CreateProxy without methods
///   6. Wait for second consumer checkpoint 1 (reconnection verified)
///   7. Tell consumer and provider to finish
int DoConsumerNormalRestart(score::cpp::stop_token test_stop_token, int argc, const char** argv) noexcept
{
    ObjectCleanupGuard object_cleanup_guard{};

    // ********************************************************************************
    // Step (1) - Fork first consumer (with methods enabled)
    // ********************************************************************************
    std::cerr << "Controller Step (1) - Fork first consumer instance (with methods)" << std::endl;
    auto consumer_checkpoint_control_guard_result = CreateSharedCheckPointControl(
        "Controller Step (1)", kShmConsumerCheckpointControlFileName, kConsumerCheckpointControlName);
    if (!(consumer_checkpoint_control_guard_result.has_value()))
    {
        return EXIT_FAILURE;
    }
    auto& consumer_checkpoint_control = consumer_checkpoint_control_guard_result.value().GetObject();
    object_cleanup_guard.AddConsumerCheckpointControlGuard(consumer_checkpoint_control_guard_result.value());

    constexpr bool kEnableMethods = true;
    constexpr bool kDisableMethods = false;

    auto fork_consumer_pid_guard = score::mw::com::test::ForkProcessAndRunInChildProcess(
        "Controller Step (1)", "Consumer", [&consumer_checkpoint_control, test_stop_token, argc, argv]() {
            score::mw::com::test::DoConsumerActions(
                consumer_checkpoint_control, test_stop_token, argc, argv, kEnableMethods);
        });
    if (!fork_consumer_pid_guard.has_value())
    {
        std::cerr << "Controller: Step (1) failed, exiting." << std::endl;
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }
    object_cleanup_guard.AddForkConsumerGuard(fork_consumer_pid_guard.value());

    // ********************************************************************************
    // Step (2) - Fork provider process
    // ********************************************************************************
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

    // ********************************************************************************
    // Step (3) - Wait for provider to reach checkpoint (1) - service offered
    // ********************************************************************************
    std::cerr << "Controller Step (3) - Waiting for provider to reach checkpoint 1" << std::endl;
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

    // ********************************************************************************
    // Step (4) - Wait for first consumer to reach checkpoint (1) - methods called successfully
    // ********************************************************************************
    std::cerr << "Controller Step (4) - Waiting for first consumer to reach checkpoint 1" << std::endl;
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

    // ********************************************************************************
    // Step (5) - Tell first consumer to finish gracefully and wait for exit
    // ********************************************************************************
    std::cerr << "Controller Step (5) - Tell first consumer to finish gracefully" << std::endl;
    consumer_checkpoint_control.FinishActions();

    std::cerr << "Controller Step (5) - Wait for first consumer to exit" << std::endl;
    if (!score::mw::com::test::WaitForChildProcessToTerminate(
            "Controller: Step (5)", fork_consumer_pid_guard.value(), kMaxWaitTimeToReachCheckpoint))
    {
        std::cerr << "Controller: First consumer did not exit in time!" << std::endl;
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }
    std::cerr << "Controller: First consumer exited successfully" << std::endl;

    // ********************************************************************************
    // Step (6) - Fork second consumer
    // ********************************************************************************
    std::cerr << "Controller Step (6) - Fork second consumer instance (reconnect, no methods)" << std::endl;
    fork_consumer_pid_guard = score::mw::com::test::ForkProcessAndRunInChildProcess(
        "Controller Step (6)", "Consumer", [&consumer_checkpoint_control, test_stop_token, argc, argv]() {
            score::mw::com::test::DoConsumerActions(
                consumer_checkpoint_control, test_stop_token, argc, argv, kDisableMethods);
        });
    if (!fork_consumer_pid_guard.has_value())
    {
        std::cerr << "Controller: Step (6) failed, exiting." << std::endl;
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }
    object_cleanup_guard.AddForkConsumerGuard(fork_consumer_pid_guard.value());

    // ********************************************************************************
    // Step (7) - Wait for second consumer to reach checkpoint (1) - reconnection verified
    // ********************************************************************************
    std::cerr << "Controller Step (7) - Waiting for second consumer to reach checkpoint 1" << std::endl;
    notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Controller: Step (7)",
                          notification_happened,
                          consumer_checkpoint_control,
                          kCheckpointMethodCallsSucceeded))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    // ********************************************************************************
    // Step (8) - Tell both consumer and provider to finish
    // ********************************************************************************
    std::cerr << "Controller Step (8) - Tell consumer and provider to finish" << std::endl;
    consumer_checkpoint_control.FinishActions();
    provider_checkpoint_control.FinishActions();

    std::cerr << "Controller: Consumer normal restart test completed successfully!" << std::endl;
    object_cleanup_guard.CleanUp();
    return EXIT_SUCCESS;
}

/// Consumer Kill Restart Flow:
///   1. Fork consumer (1st instance, enable_methods=true) → FindService, CreateProxy with methods, calls 5x
///   2. Fork provider → creates skeleton, offers service → provider checkpoint 1
///   3. Wait provider checkpoint 1, then wait consumer checkpoint 1 (methods verified)
///   4. SIGKILL consumer → wait for consumer to exit
///   5. Fork consumer (2nd instance, enable_methods=false) → FindService, CreateProxy without methods
///   6. Wait for second consumer checkpoint 1 (reconnection verified)
///   7. Tell consumer and provider to finish
int DoConsumerKillRestart(score::cpp::stop_token test_stop_token, int argc, const char** argv) noexcept
{
    ObjectCleanupGuard object_cleanup_guard{};

    // ********************************************************************************
    // Step (1) - Fork first consumer (with methods enabled)
    // ********************************************************************************
    std::cerr << "Controller Step (1) - Fork first consumer instance (with methods)" << std::endl;
    auto consumer_checkpoint_control_guard_result = CreateSharedCheckPointControl(
        "Controller Step (1)", kShmConsumerCheckpointControlFileName, kConsumerCheckpointControlName);
    if (!(consumer_checkpoint_control_guard_result.has_value()))
    {
        return EXIT_FAILURE;
    }
    auto& consumer_checkpoint_control = consumer_checkpoint_control_guard_result.value().GetObject();
    object_cleanup_guard.AddConsumerCheckpointControlGuard(consumer_checkpoint_control_guard_result.value());

    constexpr bool kEnableMethods = true;
    constexpr bool kDisableMethods = false;

    auto fork_consumer_pid_guard = score::mw::com::test::ForkProcessAndRunInChildProcess(
        "Controller Step (1)", "Consumer", [&consumer_checkpoint_control, test_stop_token, argc, argv]() {
            score::mw::com::test::DoConsumerActions(
                consumer_checkpoint_control, test_stop_token, argc, argv, kEnableMethods);
        });
    if (!fork_consumer_pid_guard.has_value())
    {
        std::cerr << "Controller: Step (1) failed, exiting." << std::endl;
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }
    object_cleanup_guard.AddForkConsumerGuard(fork_consumer_pid_guard.value());

    // ********************************************************************************
    // Step (2) - Fork provider process
    // ********************************************************************************
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

    // ********************************************************************************
    // Step (3) - Wait for provider to reach checkpoint (1) - service offered
    // ********************************************************************************
    std::cerr << "Controller Step (3) - Waiting for provider to reach checkpoint 1" << std::endl;
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

    // ********************************************************************************
    // Step (4) - Wait for first consumer to reach checkpoint (1) - methods called successfully
    // ********************************************************************************
    std::cerr << "Controller Step (4) - Waiting for first consumer to reach checkpoint 1" << std::endl;
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

    // ********************************************************************************
    // Step (5) - KILL first consumer process and wait for it to exit
    // ********************************************************************************
    std::cerr << "Controller Step (5) - KILLING first consumer process" << std::endl;
    if (kill(fork_consumer_pid_guard.value().GetPid(), SIGKILL) != 0)
    {
        std::cerr << "Controller: Failed to kill consumer!" << std::endl;
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    std::cerr << "Controller Step (5) - Waiting for killed consumer to exit" << std::endl;
    if (!score::mw::com::test::WaitForChildProcessToTerminate(
            "Controller: Step (5)", fork_consumer_pid_guard.value(), kMaxWaitTimeToReachCheckpoint))
    {
        std::cerr << "Controller: Killed consumer did not exit!" << std::endl;
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }
    std::cerr << "Controller: First consumer (killed) exited" << std::endl;

    // ********************************************************************************
    // Step (6) - Fork second consumer
    // ********************************************************************************
    std::cerr << "Controller Step (6) - Fork second consumer instance (reconnect, no methods)" << std::endl;
    fork_consumer_pid_guard = score::mw::com::test::ForkProcessAndRunInChildProcess(
        "Controller Step (6)", "Consumer", [&consumer_checkpoint_control, test_stop_token, argc, argv]() {
            score::mw::com::test::DoConsumerActions(
                consumer_checkpoint_control, test_stop_token, argc, argv, kDisableMethods);
        });
    if (!fork_consumer_pid_guard.has_value())
    {
        std::cerr << "Controller: Step (6) failed, exiting." << std::endl;
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }
    object_cleanup_guard.AddForkConsumerGuard(fork_consumer_pid_guard.value());

    // ********************************************************************************
    // Step (7) - Wait for second consumer to reach checkpoint (1) - reconnection verified
    // ********************************************************************************
    std::cerr << "Controller Step (7) - Waiting for second consumer to reach checkpoint 1" << std::endl;
    notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Controller: Step (7)",
                          notification_happened,
                          consumer_checkpoint_control,
                          kCheckpointMethodCallsSucceeded))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    // ********************************************************************************
    // Step (8) - Tell both consumer and provider to finish
    // ********************************************************************************
    std::cerr << "Controller Step (8) - Tell consumer and provider to finish" << std::endl;
    consumer_checkpoint_control.FinishActions();
    provider_checkpoint_control.FinishActions();

    std::cerr << "Controller: Consumer kill restart test completed successfully!" << std::endl;
    object_cleanup_guard.CleanUp();
    return EXIT_SUCCESS;
}

}  // namespace score::mw::com::test
