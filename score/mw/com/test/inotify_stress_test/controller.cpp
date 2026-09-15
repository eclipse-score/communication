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

#include "score/mw/com/test/inotify_stress_test/controller.h"

#include "score/filesystem/details/standard_filesystem.h"
#include "score/mw/com/test/common_test_resources/check_point_control.h"
#include "score/mw/com/test/inotify_stress_test/inotify_stress_test_internal.h"

#include <fcntl.h>
#include <score/stop_token.hpp>
#include <unistd.h>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace score::mw::com::test
{

namespace
{

/// \brief Creates (empty) or removes the shared notify test file, depending on \p create.
/// \return true on success; logs and returns false on failure.
bool ApplyNotifyFileOperation(const pid_t pid, const bool create)
{
    const std::string file_path{kBaseFolder + "/" + kNotifyTestFileName};
    if (create)
    {
        const int fd{::open(file_path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644)};
        if (fd < 0)
        {
            std::cerr << pid << ": creating " << file_path << " failed: " << strerror(errno) << std::endl;
            return false;
        }
        static_cast<void>(::close(fd));
        return true;
    }

    if ((::unlink(file_path.c_str()) != 0) && (errno != ENOENT))
    {
        std::cerr << pid << ": removing " << file_path << " failed: " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

}  // namespace

bool RunController(std::vector<CheckPointControl*>& checkpoint_controls,
                   const std::size_t num_processes,
                   const std::size_t cycles,
                   const score::cpp::stop_token& stop_token,
                   const TestMode mode)
{
    const auto pid{getpid()};

    if (mode == TestMode::kNotifyLatency)
    {
        // Wait for every worker to confirm its inotify watch is armed before performing any file
        // operation — otherwise a worker could still be starting up (e.g. not yet forked/scheduled or
        // mid-AddWatch) when the controller acts, and would miss the notification entirely.
        for (std::size_t j{0U}; j < checkpoint_controls.size(); ++j)
        {
            const std::string tag{"[startup worker=" + std::to_string(j) + "] "};
            const int result =
                WaitAndVerifyCheckPoint(tag,
                                        *checkpoint_controls[j],
                                        kWatchReadyCheckpoint,
                                        stop_token,
                                        std::chrono::duration_cast<std::chrono::milliseconds>(kCheckpointWaitDuration));
            if (result != EXIT_SUCCESS)
            {
                std::cerr << pid << ": Worker " << j << " failed to arm its inotify watch" << std::endl;
                for (auto* const cp : checkpoint_controls)
                {
                    cp->FinishActions();
                }
                return false;
            }
        }
    }

    bool test_passed{true};

    for (std::size_t i{0U}; i < cycles; ++i)
    {
        // Signal all workers to begin this cycle. In kNotifyLatency mode this doubles as the announcement
        // that the controller is about to create/remove the shared notify test file.
        for (std::size_t j{0U}; j < checkpoint_controls.size(); ++j)
        {
            std::cerr << pid << ": Signalling worker " << j << " to start (cycle " << i << ")" << std::endl;
            checkpoint_controls[j]->ProceedToNextCheckpoint();
        }

        if (mode == TestMode::kNotifyLatency)
        {
            // Alternate create/delete every cycle, starting with create, matching the expectation each
            // worker independently derives from the cycle index.
            const bool create{(i % 2U) == 0U};
            if (!ApplyNotifyFileOperation(pid, create))
            {
                test_passed = false;
                break;
            }
        }

        // Wait for every worker to report checkpoint or error — collect all before deciding to abort
        for (std::size_t j{0U}; j < checkpoint_controls.size(); ++j)
        {
            const std::string tag{"[cycle=" + std::to_string(i) + " worker=" + std::to_string(j) + "] "};
            const int result =
                WaitAndVerifyCheckPoint(tag,
                                        *checkpoint_controls[j],
                                        kCycleDoneCheckpoint,
                                        stop_token,
                                        std::chrono::duration_cast<std::chrono::milliseconds>(kCheckpointWaitDuration));
            if (result != EXIT_SUCCESS)
            {
                std::cerr << pid << ": Worker " << j << " failed at cycle " << i << std::endl;
                test_passed = false;
            }
        }

        if (mode == TestMode::kWatchChurn)
        {
            // Remove the shared test directory so the next cycle exercises the creation path again
            static_cast<void>(num_processes);
            const score::filesystem::StandardFilesystem fs;
            const std::string test_dir{TestDir()};
            const auto remove = fs.RemoveAll(test_dir);
            if (!remove.has_value())
            {
                // Non-fatal: directory may not have been created (all workers errored before mkdir)
                std::cerr << pid << ": Remove " << test_dir << " info: " << remove.error() << std::endl;
            }
        }

        if (!test_passed)
        {
            break;
        }
    }

    // Unblock all workers regardless of outcome so they exit cleanly
    for (auto* const cp : checkpoint_controls)
    {
        cp->FinishActions();
    }

    if (mode == TestMode::kNotifyLatency)
    {
        // Best-effort cleanup of the shared notify test file, in case the run ended after a "create" cycle.
        static_cast<void>(::unlink((kBaseFolder + "/" + kNotifyTestFileName).c_str()));
    }

    return test_passed;
}

}  // namespace score::mw::com::test
