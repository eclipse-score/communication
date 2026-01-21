/*******************************************************************************
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
 *******************************************************************************/

#include "score/mw/com/test/common_test_resources/general_resources.h"

#include "score/mw/com/test/common_test_resources/check_point_control.h"
#include "score/mw/com/test/common_test_resources/shared_memory_object_guard.h"

#include <score/expected.hpp>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <utility>

namespace score::mw::com::test
{

void ObjectCleanupGuard::AddConsumerCheckpointControlGuard(
    SharedMemoryObjectCreator<CheckPointControl>& consumer_checkpoint_control_guard) noexcept
{
    consumer_checkpoint_control_guard_.emplace_back(consumer_checkpoint_control_guard);
}

void ObjectCleanupGuard::AddProviderCheckpointControlGuard(
    SharedMemoryObjectCreator<CheckPointControl>& provider_checkpoint_control_guard) noexcept
{
    provider_checkpoint_control_guard_.emplace_back(provider_checkpoint_control_guard);
}

void ObjectCleanupGuard::AddForkConsumerGuard(ChildProcessGuard& fork_consumer_pid_guard) noexcept
{
    fork_consumer_pid_guard_.emplace_back(fork_consumer_pid_guard);
}

void ObjectCleanupGuard::AddForkProviderGuard(ChildProcessGuard& fork_provider_pid_guard) noexcept
{
    fork_provider_pid_guard_.emplace_back(fork_provider_pid_guard);
}

bool ObjectCleanupGuard::CleanUp() noexcept
{
    for (auto& checkpoint_control_guard : consumer_checkpoint_control_guard_)
    {
        checkpoint_control_guard.get().CleanUp();
    }

    for (auto& checkpoint_control_guard : provider_checkpoint_control_guard_)
    {
        checkpoint_control_guard.get().CleanUp();
    }

    for (auto& pid_guard : fork_provider_pid_guard_)
    {
        if (!pid_guard.get().KillChildProcess())
        {
            std::cerr << "fork_provider_pid_guard_ clean up failed" << std::endl;
            return false;
        }
    }

    for (auto& pid_guard : fork_consumer_pid_guard_)
    {
        if (!pid_guard.get().KillChildProcess())
        {
            std::cerr << "fork_consumer_pid_guard_ clean up failed" << std::endl;
            return false;
        }
    }
    return true;
}

void assertion_stdout_handler(const score::cpp::handler_parameters& param) noexcept
{
    std::cerr << "In " << param.file << ":" << param.line << " " << param.function << " condition " << param.condition
              << " >> " << param.message << std::endl;
}

CheckPointControl::ProceedInstruction WaitForChildProceed(CheckPointControl& check_point_control,
                                                          score::cpp::stop_token test_stop_token)
{
    const auto notification_received = check_point_control.WaitForProceedOrFinishTrigger(std::move(test_stop_token));
    if (!notification_received)
    {
        std::cerr << check_point_control.GetOwnerName() << ": Wait for proceed/finish aborted via stop-token!"
                  << std::endl;
        return CheckPointControl::ProceedInstruction::INVALID;
    }
    const auto proceed_instruction = check_point_control.GetProceedInstruction();
    // we received a notification. Reset notifier to enable further/new proceed notifications.
    check_point_control.ResetProceedNotifications();
    return proceed_instruction;
}

os::Result<SharedMemoryObjectCreator<CheckPointControl>> CreateSharedCheckPointControl(
    const std::string_view message_prefix,
    const std::string_view shared_memory_file_path,
    const std::string_view check_point_owner_name) noexcept
{
    auto checkpoint_control_guard_result = SharedMemoryObjectCreator<CheckPointControl>::CreateObject(
        std::string{shared_memory_file_path}, check_point_owner_name);
    if (!(checkpoint_control_guard_result.has_value()))
    {
        std::cerr << message_prefix << ": Error creating SharedMemoryObjectCreator for checkpoint_control, exiting."
                  << std::endl;
        return score::cpp::make_unexpected(std::move(checkpoint_control_guard_result).error());
    }
    std::cerr << message_prefix << ": Successfully created SharedMemoryObjectCreator for checkpoint_control"
              << std::endl;
    return checkpoint_control_guard_result;
}

os::Result<SharedMemoryObjectCreator<CheckPointControl>> CreateOrOpenSharedCheckPointControl(
    const std::string_view message_prefix,
    const std::string_view shared_memory_file_path,
    const std::string_view check_point_owner_name) noexcept
{
    auto checkpoint_control_guard_result = SharedMemoryObjectCreator<CheckPointControl>::CreateOrOpenObject(
        std::string{shared_memory_file_path}, check_point_owner_name);
    if (!(checkpoint_control_guard_result.has_value()))
    {
        std::cerr << message_prefix
                  << ": Error creating or opening SharedMemoryObjectCreator for checkpoint_control, exiting."
                  << std::endl;
        return score::cpp::make_unexpected(std::move(checkpoint_control_guard_result).error());
    }
    std::cerr << message_prefix << ": Successfully created or opened SharedMemoryObjectCreator for checkpoint_control"
              << std::endl;
    return checkpoint_control_guard_result;
}

os::Result<SharedMemoryObjectCreator<CheckPointControl>> OpenSharedCheckPointControl(
    const std::string_view message_prefix,
    const std::string_view shared_memory_file_path) noexcept
{

    const std::string shared_memory_file_path_string{shared_memory_file_path};
    auto checkpoint_control_guard_result =
        SharedMemoryObjectCreator<CheckPointControl>::OpenObject(shared_memory_file_path_string);
    if (!(checkpoint_control_guard_result.has_value()))
    {
        std::uint8_t retry_counter{20};
        while ((retry_counter-- > 0) && (!checkpoint_control_guard_result.has_value()))
        {
            const std::chrono::milliseconds poll_interval{50U};
            std::this_thread::sleep_for(poll_interval);
            checkpoint_control_guard_result =
                SharedMemoryObjectCreator<CheckPointControl>::OpenObject(shared_memory_file_path_string);
        }
    }

    if (!(checkpoint_control_guard_result.has_value()))
    {
        std::cerr << message_prefix << ": Error opening SharedMemoryObjectCreator for checkpoint_control, exiting."
                  << std::endl;
        return score::cpp::make_unexpected(std::move(checkpoint_control_guard_result).error());
    }

    std::cerr << message_prefix << ": Successfully opened SharedMemoryObjectCreator for checkpoint_control"
              << std::endl;
    return checkpoint_control_guard_result;
}

score::cpp::optional<ChildProcessGuard> ForkProcessAndRunInChildProcess(std::string_view parent_message_prefix,
                                                                 std::string_view child_message_prefix,
                                                                 std::function<void()> child_callable) noexcept
{
    const int is_child_process{0};
    const int fork_failed{-1};

    std::cerr << parent_message_prefix << ": forking " << child_message_prefix << " process\n";
    score::mw::com::test::ChildProcessGuard pid_guard{fork()};
    if (pid_guard.GetPid() == fork_failed)
    {
        std::cerr << parent_message_prefix << ": Error forking child process: " << strerror(errno) << ", exiting."
                  << std::endl;
        return {};
    }
    if (pid_guard.GetPid() == is_child_process)
    {
        // in our ITF setup 3 concurrent processes (main/controller, provider, consumer) are all outputting to std::cerr
        // this might lead to corrupted output, etc. We could easily circumvent this by redirecting stderr to some
        // process specific sinks: See Ticket-140496
        child_callable();

        // Child process return/exit codes are basically irrelevant. They print any error to std::cerr anyhow and notify
        // the parent/controller via CheckPointControl object-notifications in shm.
        // So returning SUCCESS here is fine as the error-detection/reporting is the job of the parent/controller ;)
        std::cerr << child_message_prefix << ": Child callable returned. Calling EXIT!" << std::endl;
        std::exit(EXIT_SUCCESS);
    }

    std::cerr << parent_message_prefix << ": successfully forked " << child_message_prefix
              << "process with PID: " << pid_guard.GetPid() << std::endl;
    // Is in parent process
    return pid_guard;
}

bool WaitForChildProcessToTerminate(std::string_view message_prefix,
                                    score::mw::com::test::ChildProcessGuard& child_process_guard,
                                    std::chrono::milliseconds max_wait_time) noexcept
{
    const std::chrono::milliseconds poll_interval{50};
    std::chrono::milliseconds current_wait_time{0};
    while (current_wait_time < max_wait_time)
    {
        std::this_thread::sleep_for(poll_interval);
        current_wait_time += poll_interval;
        auto is_dead = child_process_guard.IsProcessDead(false);
        if (!is_dead.has_value())
        {
            std::cerr << message_prefix << ": failed to check if child process is dead" << std::endl;
            return false;
        }
        if (is_dead.value())
        {
            return true;
        }
    }
    std::cerr << message_prefix << ": child process did not terminate within expected time" << std::endl;
    return false;
}

}  // namespace score::mw::com::test
