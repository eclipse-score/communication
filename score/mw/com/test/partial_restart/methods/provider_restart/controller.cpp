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
#include "score/mw/com/test/partial_restart/methods/provider_restart/test_checkpoints.h"

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

const std::chrono::seconds kMaxWaitTimeToReachCheckpoint{10U};

const std::string_view kShmProviderCheckpointControlFileName = "methods_provider_restart_provider_checkpoint_file";
const std::string_view kShmConsumerCheckpointControlFileName = "methods_provider_restart_consumer_checkpoint_file";
const std::string_view kConsumerCheckpointControlName = "Consumer";
const std::string_view kProviderCheckpointControlName = "Provider";

}  // namespace

void DoProviderNormalRestartWithProxy(score::cpp::stop_token test_stop_token,
                                      std::string_view service_instance_manifest) noexcept
{
    ObjectCleanupGuard object_cleanup_guard{};

    // ********************************************************************************
    // Step (1) - Fork consumer process
    // ********************************************************************************
    std::cout << "Controller Step (1): Fork consumer process" << std::endl;
    auto consumer_checkpoint_control_guard_result = CreateSharedCheckPointControl(
        "Controller", kShmConsumerCheckpointControlFileName, kConsumerCheckpointControlName);
    if (!(consumer_checkpoint_control_guard_result.has_value()))
    {
        FailTest("Controller Step (1): Fork consumer process failed.");
    }
    auto& consumer_checkpoint_control = consumer_checkpoint_control_guard_result.value().GetObject();
    object_cleanup_guard.AddConsumerCheckpointControlGuard(consumer_checkpoint_control_guard_result.value());

    auto fork_consumer_pid_guard = score::mw::com::test::ForkProcessAndRunInChildProcess(
        "Controller Step (1)",
        "Consumer",
        [&consumer_checkpoint_control, test_stop_token, service_instance_manifest]() {
            const bool with_proxy = true;
            score::mw::com::test::DoConsumerActions(
                consumer_checkpoint_control, test_stop_token, service_instance_manifest, with_proxy);
        });
    if (!fork_consumer_pid_guard.has_value())
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (1): Fork consumer process failed.");
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
    if (!VerifyCheckpoint(
            "Controller Step (3)", notification_happened, provider_checkpoint_control, kCheckpointMethodCallsSucceeded))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (3): Waiting for provider to reach checkpoint 1 failed.");
    }

    // ********************************************************************************
    // Step (4) - Wait for consumer to reach checkpoint (1) - methods called successfully
    // ********************************************************************************
    std::cout << "Controller Step (4): Waiting for consumer to reach checkpoint 1" << std::endl;
    notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint(
            "Controller Step (4)", notification_happened, consumer_checkpoint_control, kCheckpointMethodCallsSucceeded))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (4): Waiting for consumer to reach checkpoint 1 failed.");
    }

    // ********************************************************************************
    // Step (5) - Trigger Consumer to proceed
    // ********************************************************************************
    std::cout << "Controller Step (5): Trigger Consumer to proceed to next checkpoint" << std::endl;
    consumer_checkpoint_control.ProceedToNextCheckpoint();

    // ********************************************************************************
    // Step (6) - Trigger provider to proceed (will call StopOffer)
    // ********************************************************************************
    std::cout << "Controller Step (6): Trigger provider to proceed to next checkpoint" << std::endl;
    provider_checkpoint_control.ProceedToNextCheckpoint();

    // ********************************************************************************
    // Step (7) - Wait for provider to reach checkpoint (2) - StopOffer completed
    // ********************************************************************************
    std::cout << "Controller Step (7): Waiting for provider to reach checkpoint 2" << std::endl;
    notification_happened = provider_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint(
            "Controller Step (7)", notification_happened, provider_checkpoint_control, kCheckpointServiceStopped))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (7): Waiting for provider to reach checkpoint 2 failed.");
    }
    // ********************************************************************************
    // Step (8) - Trigger consumer to proceed and wait for service reappearance
    // ********************************************************************************
    std::cout << "Controller Step (8): Trigger consumer to wait for service reappearance" << std::endl;
    consumer_checkpoint_control.ProceedToNextCheckpoint();
    // ********************************************************************************
    // Step (9) - Wait for consumer to reach checkpoint (2) - detected service disappearance
    // ********************************************************************************
    std::cout << "Controller Step (9): Waiting for consumer to reach checkpoint 2" << std::endl;
    notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint(
            "Controller Step (9)", notification_happened, consumer_checkpoint_control, kCheckpointServiceStopped))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (9): Waiting for consumer to reach checkpoint 2 failed.");
    }

    // ********************************************************************************
    // Step (10) - Tell provider to finish gracefully
    // ********************************************************************************
    std::cout << "Controller Step (10): Tell provider to finish gracefully" << std::endl;
    provider_checkpoint_control.FinishActions();

    // ********************************************************************************
    // Step (11) - Wait for provider to exit
    // ********************************************************************************
    std::cout << "Controller Step (11): Wait for provider to exit" << std::endl;
    if (!score::mw::com::test::WaitForChildProcessToTerminate(
            "Controller Step (11)", fork_provider_pid_guard.value(), kMaxWaitTimeToReachCheckpoint))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (11): Wait for provider to exit failed.");
    }
    std::cout << "Controller Step (11): Provider exited successfully" << std::endl;
    if (provider_checkpoint_control.HasErrorOccurred())
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (11): Wait for provider to exit failed because provider reported an error.");
    }

    // Clear any stale proceed state before the restarted provider reuses the same checkpoint control.
    provider_checkpoint_control.ResetCheckpointReachedNotifications();
    provider_checkpoint_control.ResetProceedNotifications();

    // ********************************************************************************
    // Step (12) - Restart provider
    // ********************************************************************************
    std::cout << "Controller Step (12): Restarting provider" << std::endl;
    fork_provider_pid_guard = score::mw::com::test::ForkProcessAndRunInChildProcess(
        "Controller Step (12)",
        "Provider",
        [&provider_checkpoint_control, test_stop_token, service_instance_manifest]() {
            score::mw::com::test::DoProviderActions(
                provider_checkpoint_control, test_stop_token, service_instance_manifest);
        });
    if (!fork_provider_pid_guard.has_value())
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (12): Restarting provider failed.");
    }
    object_cleanup_guard.AddForkProviderGuard(fork_provider_pid_guard.value());

    // ********************************************************************************
    // Step (13) - Wait for provider to reach checkpoint (1) again - service offered
    // ********************************************************************************
    std::cout << "Controller Step (13): Waiting for restarted provider to reach checkpoint 1" << std::endl;
    notification_happened = provider_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Controller Step (13)",
                          notification_happened,
                          provider_checkpoint_control,
                          kCheckpointMethodCallsSucceeded))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (13): Waiting for restarted provider to reach checkpoint 1 failed.");
    }

    // ********************************************************************************

    // Step (14) - Trigger consumer to proceed and wait for service reappearance

    // ********************************************************************************
    std::cout << "Controller Step (14): Trigger consumer to wait for service reappearance" << std::endl;
    consumer_checkpoint_control.ProceedToNextCheckpoint();

    // ********************************************************************************

    // Step (15) - Wait for consumer to reach checkpoint (3) - detected service reappearance

    // ********************************************************************************
    std::cout << "Controller Step (15): Waiting for consumer to reach checkpoint 3" << std::endl;
    notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint(
            "Controller Step (15):", notification_happened, consumer_checkpoint_control, kCheckpointServiceReappeared))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (15): Waiting for consumer to reach checkpoint 3 failed.");
    }

    // Consumer will now create new proxy and call methods to reach checkpoint 4
    // No trigger needed here - consumer already proceeded from Step 7 wait with the Step 14 trigger

    // ********************************************************************************

    // Step (16) - Wait for consumer to reach checkpoint (4) - methods called successfully after restart

    // ********************************************************************************
    std::cout << "Controller Step (16): Waiting for consumer to reach checkpoint 4" << std::endl;
    notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Controller Step (16):",
                          notification_happened,
                          consumer_checkpoint_control,
                          kCheckpointMethodsCalledAfterRestart))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (16): Waiting for consumer to reach checkpoint 4 failed.");
    }

    // ********************************************************************************

    // Step (17) - Trigger provider to proceed (will call StopOffer)

    // ********************************************************************************
    std::cout << "Controller Step (17): Trigger provider to proceed to next checkpoint" << std::endl;
    provider_checkpoint_control.ProceedToNextCheckpoint();

    // ********************************************************************************

    // Step (18) - Wait for provider to reach checkpoint (2) - StopOffer completed

    // ********************************************************************************
    std::cout << "Controller Step (18): Waiting for provider to reach checkpoint 2" << std::endl;
    notification_happened = provider_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint(
            "Controller Step (18)", notification_happened, provider_checkpoint_control, kCheckpointServiceStopped))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (18): Waiting for provider to reach checkpoint 2 failed.");
    }

    // ********************************************************************************

    // Step (19) - Tell both consumer and provider to finish

    // ********************************************************************************
    std::cout << "Controller Step (19): Tell consumer and provider to finish" << std::endl;
    consumer_checkpoint_control.FinishActions();
    provider_checkpoint_control.FinishActions();

    const auto consumer_terminated = WaitForChildProcessToTerminate(
        "Controller Step (19):", fork_consumer_pid_guard.value(), kMaxWaitTimeToReachCheckpoint);
    if (!consumer_terminated)
    {
        object_cleanup_guard.CleanUp();
        FailTest(
            "Controller Step (19): Tell consumer and provider to finish failed because consumer did not terminate "
            "successfully.");
    }

    const auto provider_terminated = WaitForChildProcessToTerminate(
        "Controller Step (19):", fork_provider_pid_guard.value(), kMaxWaitTimeToReachCheckpoint);
    if (!provider_terminated)
    {
        object_cleanup_guard.CleanUp();
        FailTest(
            "Controller Step (19): Tell consumer and provider to finish failed because provider did not terminate "
            "successfully.");
    }

    if (consumer_checkpoint_control.HasErrorOccurred() || provider_checkpoint_control.HasErrorOccurred())
    {
        object_cleanup_guard.CleanUp();
        FailTest(
            "Controller Step (19): Tell consumer and provider to finish failed because consumer or provider reported "
            "an error.");
    }

    std::cout << "Controller: Test completed successfully!" << std::endl;
    object_cleanup_guard.CleanUp();
}

void DoProviderNormalRestartWithoutProxy(score::cpp::stop_token test_stop_token,
                                         std::string_view service_instance_manifest) noexcept
{
    ObjectCleanupGuard object_cleanup_guard{};

    // ********************************************************************************
    // Step (1) - Fork consumer process (without proxy)
    // ********************************************************************************
    std::cout << "Controller Step (1): Fork consumer process (without proxy)" << std::endl;
    auto consumer_checkpoint_control_guard_result = CreateSharedCheckPointControl(
        "Controller", kShmConsumerCheckpointControlFileName, kConsumerCheckpointControlName);
    if (!(consumer_checkpoint_control_guard_result.has_value()))
    {
        FailTest("Controller Step (1): Fork consumer process (without proxy) failed.");
    }
    auto& consumer_checkpoint_control = consumer_checkpoint_control_guard_result.value().GetObject();
    object_cleanup_guard.AddConsumerCheckpointControlGuard(consumer_checkpoint_control_guard_result.value());

    auto fork_consumer_pid_guard = score::mw::com::test::ForkProcessAndRunInChildProcess(
        "Controller Step (1)",
        "Consumer",
        [&consumer_checkpoint_control, test_stop_token, service_instance_manifest]() {
            const bool with_proxy = false;
            score::mw::com::test::DoConsumerActions(
                consumer_checkpoint_control, test_stop_token, service_instance_manifest, with_proxy);
        });
    if (!fork_consumer_pid_guard.has_value())
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (1): Fork consumer process (without proxy) failed.");
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
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (2): Fork provider process failed.");
    }
    object_cleanup_guard.AddForkProviderGuard(fork_provider_pid_guard.value());

    score::mw::com::test::TimeoutSupervisor timeout_supervisor{};

    // ********************************************************************************
    // Step (3) - Wait for provider checkpoint (1)
    // ********************************************************************************
    std::cout << "Controller Step (3): Waiting for provider checkpoint 1" << std::endl;
    auto notification_happened = provider_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Controller Step (3):",
                          notification_happened,
                          provider_checkpoint_control,
                          kCheckpointMethodCallsSucceeded))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (3): Waiting for provider checkpoint 1 failed.");
    }

    // ********************************************************************************
    // Step (4) - Wait for consumer checkpoint (1)
    // ********************************************************************************
    std::cout << "Controller Step (4): Waiting for consumer checkpoint 1" << std::endl;
    notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Controller Step (4):",
                          notification_happened,
                          consumer_checkpoint_control,
                          kCheckpointMethodCallsSucceeded))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (4): Waiting for consumer checkpoint 1 failed.");
    }

    // ********************************************************************************
    // Step (5) - Trigger consumer and provider to proceed
    // ********************************************************************************
    std::cout << "Controller Step (5): Trigger consumer and provider to proceed" << std::endl;
    consumer_checkpoint_control.ProceedToNextCheckpoint();
    provider_checkpoint_control.ProceedToNextCheckpoint();

    // ********************************************************************************
    // Step (6) - Wait for provider checkpoint (2)
    // ********************************************************************************
    std::cout << "Controller Step (6): Waiting for provider checkpoint 2" << std::endl;
    notification_happened = provider_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint(
            "Controller Step (6):", notification_happened, provider_checkpoint_control, kCheckpointServiceStopped))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (6): Waiting for provider checkpoint 2 failed.");
    }

    // ********************************************************************************
    // Step (7) - Wait for consumer checkpoint (2)
    // ********************************************************************************
    std::cout << "Controller Step (7): Waiting for consumer checkpoint 2" << std::endl;
    notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint(
            "Controller Step (7):", notification_happened, consumer_checkpoint_control, kCheckpointServiceStopped))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (7): Waiting for consumer checkpoint 2 failed.");
    }

    // ********************************************************************************
    // Step (8) - Tell provider to finish
    // ********************************************************************************
    std::cout << "Controller Step (8): Tell provider to finish" << std::endl;
    provider_checkpoint_control.FinishActions();

    // ********************************************************************************
    // Step (9) - Wait for provider to exit
    // ********************************************************************************
    std::cout << "Controller Step (9): Wait for provider to exit" << std::endl;

    if (!score::mw::com::test::WaitForChildProcessToTerminate(
            "Controller", fork_provider_pid_guard.value(), kMaxWaitTimeToReachCheckpoint))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (9): Wait for provider to exit failed.");
    }

    if (provider_checkpoint_control.HasErrorOccurred())
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (9): Wait for provider to exit failed because provider reported an error.");
    }

    provider_checkpoint_control.ResetCheckpointReachedNotifications();
    provider_checkpoint_control.ResetProceedNotifications();

    // ********************************************************************************
    // Step (10) - Restart provider
    // ********************************************************************************
    std::cout << "Controller Step (10): Restarting provider" << std::endl;
    fork_provider_pid_guard = score::mw::com::test::ForkProcessAndRunInChildProcess(
        "Controller Step (10)",
        "Provider",
        [&provider_checkpoint_control, test_stop_token, service_instance_manifest]() {
            score::mw::com::test::DoProviderActions(
                provider_checkpoint_control, test_stop_token, service_instance_manifest);
        });
    if (!fork_provider_pid_guard.has_value())
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (10): Restarting provider failed.");
    }
    object_cleanup_guard.AddForkProviderGuard(fork_provider_pid_guard.value());

    // ********************************************************************************
    // Step (11) - Wait for restarted provider checkpoint (1)
    // ********************************************************************************
    std::cout << "Controller Step (11): Waiting for restarted provider checkpoint 1" << std::endl;
    notification_happened = provider_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Controller Step (11):",
                          notification_happened,
                          provider_checkpoint_control,
                          kCheckpointMethodCallsSucceeded))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (11): Waiting for restarted provider checkpoint 1 failed.");
    }

    // ********************************************************************************
    // Step (12) - Trigger consumer to proceed and wait for service reappearance
    // ********************************************************************************
    std::cout << "Controller Step (12): Trigger consumer to wait for service reappearance" << std::endl;
    consumer_checkpoint_control.ProceedToNextCheckpoint();

    // ********************************************************************************
    // Step (13) - Wait for consumer checkpoint (3)
    // ********************************************************************************
    std::cout << "Controller Step (13): Waiting for consumer checkpoint 3" << std::endl;
    notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint(
            "Controller Step (13):", notification_happened, consumer_checkpoint_control, kCheckpointServiceReappeared))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (13): Waiting for consumer checkpoint 3 failed.");
    }

    // ********************************************************************************
    // Step (14) - Trigger provider to proceed (will call StopOffer)
    // ********************************************************************************
    std::cout << "Controller Step (14): Trigger provider to proceed to next checkpoint" << std::endl;
    provider_checkpoint_control.ProceedToNextCheckpoint();

    // ********************************************************************************
    // Step (15) - Wait for provider to reach checkpoint (2) - StopOffer completed
    // ********************************************************************************
    std::cout << "Controller Step (15): Waiting for provider to reach checkpoint 2" << std::endl;
    notification_happened = provider_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint(
            "Controller Step (15)", notification_happened, provider_checkpoint_control, kCheckpointServiceStopped))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (15): Waiting for provider to reach checkpoint 2 failed.");
    }

    // ********************************************************************************
    // Step (16) - Tell consumer and provider to finish
    // ********************************************************************************
    std::cout << "Controller Step (16): Tell consumer and provider to finish" << std::endl;
    consumer_checkpoint_control.FinishActions();
    provider_checkpoint_control.FinishActions();

    const auto consumer_terminated = WaitForChildProcessToTerminate(
        "Controller Step (16):", fork_consumer_pid_guard.value(), kMaxWaitTimeToReachCheckpoint);
    if (!consumer_terminated)
    {
        object_cleanup_guard.CleanUp();
        FailTest(
            "Controller Step (16): Tell consumer and provider to finish failed because consumer did not terminate "
            "successfully.");
    }

    const auto provider_terminated = WaitForChildProcessToTerminate(
        "Controller Step (16):", fork_provider_pid_guard.value(), kMaxWaitTimeToReachCheckpoint);
    if (!provider_terminated)
    {
        object_cleanup_guard.CleanUp();
        FailTest(
            "Controller Step (16): Tell consumer and provider to finish failed because provider did not terminate "
            "successfully.");
    }

    if (consumer_checkpoint_control.HasErrorOccurred() || provider_checkpoint_control.HasErrorOccurred())
    {
        object_cleanup_guard.CleanUp();
        FailTest(
            "Controller Step (16): Tell consumer and provider to finish failed because consumer or provider reported "
            "an error.");
    }

    std::cout << "Controller: Test completed successfully!" << std::endl;
    object_cleanup_guard.CleanUp();
}

void DoProviderKillRestartWithProxy(score::cpp::stop_token test_stop_token,
                                    std::string_view service_instance_manifest) noexcept
{
    // Similar to normal restart but kills provider instead of graceful shutdown
    ObjectCleanupGuard object_cleanup_guard{};

    // ********************************************************************************
    // Step (1) - Fork consumer process
    // ********************************************************************************
    std::cout << "Controller Step (1): Fork consumer process" << std::endl;
    auto consumer_checkpoint_control_guard_result = CreateSharedCheckPointControl(
        "Controller", kShmConsumerCheckpointControlFileName, kConsumerCheckpointControlName);
    if (!(consumer_checkpoint_control_guard_result.has_value()))
    {
        FailTest("Controller Step (1): Fork consumer process failed.");
    }
    auto& consumer_checkpoint_control = consumer_checkpoint_control_guard_result.value().GetObject();
    object_cleanup_guard.AddConsumerCheckpointControlGuard(consumer_checkpoint_control_guard_result.value());

    auto fork_consumer_pid_guard = score::mw::com::test::ForkProcessAndRunInChildProcess(
        "Controller Step (1)",
        "Consumer",
        [&consumer_checkpoint_control, test_stop_token, service_instance_manifest]() {
            const bool with_proxy = true;
            score::mw::com::test::DoConsumerActions(
                consumer_checkpoint_control, test_stop_token, service_instance_manifest, with_proxy);
        });
    if (!fork_consumer_pid_guard.has_value())
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (1): Fork consumer process failed.");
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
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (2): Fork provider process failed.");
    }
    object_cleanup_guard.AddForkProviderGuard(fork_provider_pid_guard.value());

    score::mw::com::test::TimeoutSupervisor timeout_supervisor{};

    // ********************************************************************************
    // Step (3) - Wait for provider checkpoint (1)
    // ********************************************************************************
    std::cout << "Controller Step (3): Waiting for provider checkpoint 1" << std::endl;
    auto notification_happened = provider_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Controller Step (3):",
                          notification_happened,
                          provider_checkpoint_control,
                          kCheckpointMethodCallsSucceeded))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (3): Waiting for provider checkpoint 1 failed.");
    }

    // ********************************************************************************
    // Step (4) - Wait for consumer checkpoint (1)
    // ********************************************************************************
    std::cout << "Controller Step (4): Waiting for consumer checkpoint 1" << std::endl;
    notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Controller Step (4):",
                          notification_happened,
                          consumer_checkpoint_control,
                          kCheckpointMethodCallsSucceeded))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (4): Waiting for consumer checkpoint 1 failed.");
    }

    // ********************************************************************************
    // Step (5) - Kill provider process and wait for its death.
    // ********************************************************************************
    std::cerr << "Controller Step (5): Kill provider process and wait for its death" << std::endl;
    const auto kill_result = fork_provider_pid_guard.value().KillChildProcess();
    if (!kill_result)
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (5): Kill provider process and wait for its death failed.");
    }

    // ********************************************************************************
    // Step (6) - Trigger consumer to proceed
    // ********************************************************************************
    std::cout << "Controller Step (6): Trigger consumer to proceed" << std::endl;
    consumer_checkpoint_control.ProceedToNextCheckpoint();

    // ********************************************************************************
    // Step (7) - Clear any stale proceed state before the restarted consumer reuses the same checkpoint control.
    // ********************************************************************************
    provider_checkpoint_control.ResetCheckpointReachedNotifications();
    provider_checkpoint_control.ResetProceedNotifications();

    // ********************************************************************************
    // Step (8) - Restart provider
    // ********************************************************************************
    std::cout << "Controller Step (8): Restarting provider after kill" << std::endl;
    fork_provider_pid_guard = score::mw::com::test::ForkProcessAndRunInChildProcess(
        "Controller Step (8)",
        "Provider",
        [&provider_checkpoint_control, test_stop_token, service_instance_manifest]() {
            score::mw::com::test::DoProviderActions(
                provider_checkpoint_control, test_stop_token, service_instance_manifest);
        });
    if (!fork_provider_pid_guard.has_value())
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (8): Restarting provider after kill failed.");
    }
    object_cleanup_guard.AddForkProviderGuard(fork_provider_pid_guard.value());

    // ********************************************************************************
    // Step (9) - Waiting for consumer checkpoint (2)
    // ********************************************************************************
    std::cout << "Controller Step (9): Waiting for consumer checkpoint 2" << std::endl;
    // PERF: This step takes roughly 5 seconds to complete, which is two orders of magnitude longer than any other
    // segment of the test. If the test keeps timing out then this should be investigated.
    notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint(
            "Controller Step (9):", notification_happened, consumer_checkpoint_control, kCheckpointServiceStopped))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (9): Waiting for consumer checkpoint 2 failed.");
    }

    // ********************************************************************************
    // Step (10) - Wait for restarted provider checkpoint (1)
    // ********************************************************************************
    std::cout << "Controller Step (10): Waiting for restarted provider checkpoint 1" << std::endl;
    notification_happened = provider_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Controller Step (10):",
                          notification_happened,
                          provider_checkpoint_control,
                          kCheckpointMethodCallsSucceeded))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (10): Waiting for restarted provider checkpoint 1 failed.");
    }

    // ********************************************************************************
    // Step (11) - Trigger consumer to proceed from Step (7) wait and enter WaitTillServiceAppears
    // ********************************************************************************
    std::cout << "Controller Step (11): Trigger consumer to proceed to wait for service reappearance" << std::endl;
    consumer_checkpoint_control.ProceedToNextCheckpoint();

    // ********************************************************************************
    // Step (12) - Wait for consumer checkpoint (3)
    // ********************************************************************************
    std::cout << "Controller Step (12): Waiting for consumer checkpoint 3" << std::endl;
    notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint(
            "Controller Step (12):", notification_happened, consumer_checkpoint_control, kCheckpointServiceReappeared))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (12): Waiting for consumer checkpoint 3 failed.");
    }

    // ********************************************************************************
    // Step (13) - Trigger consumer to create new proxy and call methods
    // ********************************************************************************
    std::cout << "Controller Step (13): Trigger consumer to create new proxy and call methods" << std::endl;
    consumer_checkpoint_control.ProceedToNextCheckpoint();

    // ********************************************************************************
    // Step (14) - Wait for consumer checkpoint (4)
    // ********************************************************************************
    std::cout << "Controller Step (14): Waiting for consumer checkpoint 4" << std::endl;
    notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);

    if (!VerifyCheckpoint("Controller Step (14):",
                          notification_happened,
                          consumer_checkpoint_control,
                          kCheckpointMethodsCalledAfterRestart))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (14): Waiting for consumer checkpoint 4 failed.");
    }

    // ********************************************************************************
    // Step (15) - Trigger provider to proceed (will call StopOffer)
    // ********************************************************************************
    std::cout << "Controller Step (15): Trigger provider to proceed to next checkpoint" << std::endl;
    provider_checkpoint_control.ProceedToNextCheckpoint();

    // ********************************************************************************
    // Step (16) - Wait for provider to reach checkpoint (2) - StopOffer completed
    // ********************************************************************************
    std::cout << "Controller Step (16): Waiting for provider to reach checkpoint 2" << std::endl;
    notification_happened = provider_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint(
            "Controller Step (16)", notification_happened, provider_checkpoint_control, kCheckpointServiceStopped))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (16): Waiting for provider to reach checkpoint 2 failed.");
    }

    // ********************************************************************************
    // Step (17) - Trigger consumer to finish
    // ********************************************************************************
    std::cout << "Controller Step (17): Trigger consumer to finish" << std::endl;
    consumer_checkpoint_control.FinishActions();
    provider_checkpoint_control.FinishActions();

    const auto consumer_terminated = WaitForChildProcessToTerminate(
        "Controller Step (17):", fork_consumer_pid_guard.value(), kMaxWaitTimeToReachCheckpoint);
    if (!consumer_terminated)
    {
        object_cleanup_guard.CleanUp();
        FailTest(
            "Controller Step (17): Trigger consumer to finish failed because consumer did not terminate successfully.");
    }

    const auto provider_terminated = WaitForChildProcessToTerminate(
        "Controller Step (17):", fork_provider_pid_guard.value(), kMaxWaitTimeToReachCheckpoint);
    if (!provider_terminated)
    {
        object_cleanup_guard.CleanUp();
        FailTest(
            "Controller Step (17): Trigger consumer to finish failed because provider did not terminate successfully.");
    }

    if (consumer_checkpoint_control.HasErrorOccurred() || provider_checkpoint_control.HasErrorOccurred())
    {
        object_cleanup_guard.CleanUp();
        FailTest(
            "Controller Step (17): Trigger consumer to finish failed because consumer or provider reported an error.");
    }

    std::cerr << "Controller: Test completed successfully!" << std::endl;
    object_cleanup_guard.CleanUp();
}

void DoProviderKillRestartWithoutProxy(score::cpp::stop_token test_stop_token,
                                       std::string_view service_instance_manifest) noexcept
{
    // Combination of without-proxy and kill variants
    ObjectCleanupGuard object_cleanup_guard{};

    // ********************************************************************************
    // Step (1) - Fork consumer process (without proxy)
    // ********************************************************************************
    std::cout << "Controller Step (1): Fork consumer process (without proxy)" << std::endl;
    auto consumer_checkpoint_control_guard_result = CreateSharedCheckPointControl(
        "Controller", kShmConsumerCheckpointControlFileName, kConsumerCheckpointControlName);
    if (!(consumer_checkpoint_control_guard_result.has_value()))
    {
        FailTest("Controller Step (1): Fork consumer process (without proxy) failed.");
    }
    auto& consumer_checkpoint_control = consumer_checkpoint_control_guard_result.value().GetObject();
    object_cleanup_guard.AddConsumerCheckpointControlGuard(consumer_checkpoint_control_guard_result.value());

    auto fork_consumer_pid_guard = score::mw::com::test::ForkProcessAndRunInChildProcess(
        "Controller Step (1)",
        "Consumer",
        [&consumer_checkpoint_control, test_stop_token, service_instance_manifest]() {
            const bool with_proxy = false;
            score::mw::com::test::DoConsumerActions(
                consumer_checkpoint_control, test_stop_token, service_instance_manifest, with_proxy);
        });
    if (!fork_consumer_pid_guard.has_value())
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (1): Fork consumer process (without proxy) failed.");
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
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (2): Fork provider process failed.");
    }
    object_cleanup_guard.AddForkProviderGuard(fork_provider_pid_guard.value());

    score::mw::com::test::TimeoutSupervisor timeout_supervisor{};

    // ********************************************************************************
    // Step (3) - Wait for provider checkpoint (1)
    // ********************************************************************************
    std::cout << "Controller Step (3): Waiting for provider checkpoint 1" << std::endl;
    auto notification_happened = provider_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Controller Step (3):",
                          notification_happened,
                          provider_checkpoint_control,
                          kCheckpointMethodCallsSucceeded))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (3): Waiting for provider checkpoint 1 failed.");
    }

    // ********************************************************************************
    // Step (4) - Wait for consumer checkpoint (1)
    // ********************************************************************************
    std::cout << "Controller Step (4): Waiting for consumer checkpoint 1" << std::endl;
    notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Controller Step (4):",
                          notification_happened,
                          consumer_checkpoint_control,
                          kCheckpointMethodCallsSucceeded))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (4): Waiting for consumer checkpoint 1 failed.");
    }

    // ********************************************************************************
    // Step (5) - Kill provider process and wait for its death.
    // ********************************************************************************
    std::cerr << "Controller Step (5): Kill provider process and wait for its death" << std::endl;
    const auto kill_result = fork_provider_pid_guard.value().KillChildProcess();
    if (!kill_result)
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (5): Kill provider process and wait for its death failed.");
    }

    // ********************************************************************************
    // Step (6) - Trigger consumer to proceed
    // ********************************************************************************
    std::cout << "Controller Step (6): Trigger consumer to proceed" << std::endl;
    consumer_checkpoint_control.ProceedToNextCheckpoint();

    // ********************************************************************************
    // Step (7) - Clear any stale proceed state before the restarted consumer reuses the same checkpoint control.
    // ********************************************************************************
    provider_checkpoint_control.ResetCheckpointReachedNotifications();
    provider_checkpoint_control.ResetProceedNotifications();

    // ********************************************************************************
    // Step (8) - Restart provider
    // ********************************************************************************
    std::cout << "Controller Step (8): Restarting provider after kill" << std::endl;
    fork_provider_pid_guard = score::mw::com::test::ForkProcessAndRunInChildProcess(
        "Controller Step (8)",
        "Provider",
        [&provider_checkpoint_control, test_stop_token, service_instance_manifest]() {
            score::mw::com::test::DoProviderActions(
                provider_checkpoint_control, test_stop_token, service_instance_manifest);
        });
    if (!fork_provider_pid_guard.has_value())
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (8): Restarting provider after kill failed.");
    }
    object_cleanup_guard.AddForkProviderGuard(fork_provider_pid_guard.value());

    // ********************************************************************************
    // Step (9) - Wait for consumer checkpoint (2)
    // ********************************************************************************
    std::cout << "Controller Step (9): Waiting for consumer checkpoint 2" << std::endl;
    notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Controller Step (9):", notification_happened, consumer_checkpoint_control, 2))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (9): Waiting for consumer checkpoint 2 failed.");
    }

    // ********************************************************************************
    // Step (10) - Wait for restarted provider checkpoint (1)
    // ********************************************************************************
    std::cout << "Controller Step (10): Waiting for restarted provider checkpoint 1" << std::endl;
    notification_happened = provider_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Controller Step (10)",
                          notification_happened,
                          provider_checkpoint_control,
                          kCheckpointMethodCallsSucceeded))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (10): Waiting for restarted provider checkpoint 1 failed.");
    }

    // ********************************************************************************
    // Step (11) - Trigger consumer to proceed and wait for service reappearance
    // ********************************************************************************
    std::cout << "Controller Step (11): Trigger consumer to wait for service reappearance" << std::endl;
    consumer_checkpoint_control.ProceedToNextCheckpoint();

    // ********************************************************************************
    // Step (12) - Wait for consumer checkpoint (3)
    // ********************************************************************************
    std::cout << "Controller Step (12): Waiting for consumer checkpoint 3" << std::endl;
    notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint(
            "Controller Step (12)", notification_happened, consumer_checkpoint_control, kCheckpointServiceReappeared))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (12): Waiting for consumer checkpoint 3 failed.");
    }

    // ********************************************************************************
    // Step (13) - Trigger provider to proceed (will call StopOffer)
    // ********************************************************************************
    std::cout << "Controller Step (13): Trigger provider to proceed to next checkpoint" << std::endl;
    provider_checkpoint_control.ProceedToNextCheckpoint();

    // ********************************************************************************
    // Step (14) - Wait for provider to reach checkpoint (2) - StopOffer completed
    // ********************************************************************************
    std::cout << "Controller Step (14): Waiting for provider to reach checkpoint 2" << std::endl;
    notification_happened = provider_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint(
            "Controller Step (14)", notification_happened, provider_checkpoint_control, kCheckpointServiceStopped))
    {
        object_cleanup_guard.CleanUp();
        FailTest("Controller Step (14): Waiting for provider to reach checkpoint 2 failed.");
    }

    // ********************************************************************************
    // Step (15) - Tell consumer and provider to finish
    // ********************************************************************************
    std::cout << "Controller Step (15): Tell consumer and provider to finish" << std::endl;
    consumer_checkpoint_control.FinishActions();
    provider_checkpoint_control.FinishActions();

    const auto consumer_terminated = WaitForChildProcessToTerminate(
        "Controller Step (15):", fork_consumer_pid_guard.value(), kMaxWaitTimeToReachCheckpoint);
    if (!consumer_terminated)
    {
        object_cleanup_guard.CleanUp();
        FailTest(
            "Controller Step (15): Tell consumer and provider to finish failed because consumer did not terminate "
            "successfully.");
    }

    const auto provider_terminated = WaitForChildProcessToTerminate(
        "Controller Step (15):", fork_provider_pid_guard.value(), kMaxWaitTimeToReachCheckpoint);
    if (!provider_terminated)
    {
        object_cleanup_guard.CleanUp();
        FailTest(
            "Controller Step (15): Tell consumer and provider to finish failed because provider did not terminate "
            "successfully.");
    }

    if (consumer_checkpoint_control.HasErrorOccurred() || provider_checkpoint_control.HasErrorOccurred())
    {
        object_cleanup_guard.CleanUp();
        FailTest(
            "Controller Step (15): Tell consumer and provider to finish failed because consumer or provider reported "
            "an error.");
    }

    std::cerr << "Controller: Test completed successfully!" << std::endl;
    object_cleanup_guard.CleanUp();
}

}  // namespace score::mw::com::test
