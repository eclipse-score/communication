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

#include "score/mw/com/test/common_test_resources/child_process_guard.h"
#include <sys/wait.h>
#include <stdlib.h>

#include <cerrno>
#include <csignal>
#include <cstring>
#include <iostream>

namespace score::mw::com::test
{

bool ChildProcessGuard::KillChildProcess() noexcept
{
    std::cout << "Calling KillChildProcess(): " << (pid_.has_value() ? pid_.value() : -1) << std::endl;

    // Check if KillChildProcess was previously called and process is already dead
    if (!(pid_.has_value()))
    {
        std::cout << "ChildProcessGuard: Process has already been killed" << std::endl;
        return true;
    }
    const auto pid = pid_.value();

    const bool pid_is_invalid{pid == -1};
    if (pid_is_invalid)
    {
        std::cerr << "ChildProcessGuard: Could not kill child process: invalid PID " << pid << std::endl;
        return false;
    }
    const auto owned_pid_is_self = pid == 0;
    if (owned_pid_is_self)
    {
        std::cout << "ChildProcessGuard: Owned PID is own process, cannot kill" << std::endl;
        return true;
    }

    // Check if the process is already dead
    const bool check_should_block{false};
    const auto is_process_already_dead_result = IsProcessDead(check_should_block);
    if (!(is_process_already_dead_result.has_value()))
    {
        std::cerr << "ChildProcessGuard: Could not check if process is already dead." << std::endl;
        return false;
    }
    if (is_process_already_dead_result.value())
    {
        std::cerr << "ChildProcessGuard: Child process with PID " << pid
                  << " was already dead, no need killing it with SIGKILL." << std::endl;
        pid_.reset();
        return true;
    }

    // If the process is not already dead, kill it with SIGKILL
    const auto kill_result = kill(pid, SIGKILL);
    const bool kill_failed{kill_result != 0};
    if (kill_failed)
    {
        std::cerr << pid << ": ChildProcessGuard: Error killing child process: " << strerror(errno) << "." << std::endl;
        return false;
    }

    // Wait until the process dies due to the SIGKILL
    const bool final_check_should_block{true};
    const auto is_process_finally_dead_result = IsProcessDead(final_check_should_block);
    if (!(is_process_finally_dead_result.has_value()))
    {
        std::cerr << "ChildProcessGuard: Could not check if process is already dead after killing." << std::endl;
        return false;
    }
    pid_.reset();
    std::cout << "ChildProcessGuard: Child process successfully killed." << std::endl;
    return true;
}

score::cpp::optional<bool> ChildProcessGuard::IsProcessDead(bool should_block) noexcept
{
    // Check if KillChildProcess was previously called and process is already dead
    if (!(pid_.has_value()))
    {
        return true;
    }
    const auto pid = pid_.value();

    const int options = should_block ? 0 : WNOHANG;
    int status{};
    auto wait_result = waitpid(pid, &status, options);

    const bool waitpid_failed{wait_result == -1};
    const bool child_process_died{wait_result == pid};
    if (waitpid_failed)
    {
        const bool waitpid_failed_because_no_child_process{errno == ECHILD};
        if (!waitpid_failed_because_no_child_process)
        {
            std::cerr << "ChildProcessGuard: waitpid for PID " << pid << " failed with error: " << strerror(errno)
                      << std::endl;
            return {};
        }
        return true;
    }
    if (child_process_died)
    {
        std::cerr << "ChildProcessGuard: Process with PID " << pid << " died with exit status: " << status << std::endl;
        return true;
    }
    // If the WNOHANG was not set, then waitpid should only return -1 in the error case or the process PID when
    // it has died.
    if (should_block)
    {
        std::cerr << "ChildProcessGuard: waitpid for PID " << pid << " returned unexpected code: " << wait_result
                  << std::endl;
        return {};
    }

    // If the WNOHANG flag was set, then waitpid will return 0 if the process has not changed state e.g. is
    // still alive.
    return false;
}

}  // namespace score::mw::com::test
