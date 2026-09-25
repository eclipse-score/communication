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

/// \brief Signals every worker to proceed, then waits for every one of them to report \p checkpoint (or an
///        error). Waits for (and verifies) all workers before returning, even if one already failed, so the
///        controller's view of who succeeded/failed for this round is always complete.
///
/// \return true if every worker reported \p checkpoint; false if any worker failed or errored.
bool SignalAndWaitForAll(std::vector<CheckPointControl*>& checkpoint_controls,
                         const std::uint8_t checkpoint,
                         const score::cpp::stop_token& stop_token,
                         const std::string& round_tag)
{
    for (std::size_t j{0U}; j < checkpoint_controls.size(); ++j)
    {
        std::cerr << round_tag << " worker=" << j << "] signalling to proceed" << std::endl;
        checkpoint_controls[j]->ProceedToNextCheckpoint();
    }

    bool all_ok{true};
    for (std::size_t j{0U}; j < checkpoint_controls.size(); ++j)
    {
        const std::string tag{round_tag + " worker=" + std::to_string(j) + "] "};
        const int result =
            WaitAndVerifyCheckPoint(tag,
                                    *checkpoint_controls[j],
                                    checkpoint,
                                    stop_token,
                                    std::chrono::duration_cast<std::chrono::milliseconds>(kCheckpointWaitDuration));
        if (result != EXIT_SUCCESS)
        {
            std::cerr << round_tag << " worker " << j << " failed" << std::endl;
            all_ok = false;
        }
    }
    return all_ok;
}

/// \brief Waits, once at startup, for every worker to report \p checkpoint. On failure, unblocks all
///        workers via FinishActions() (since the main per-cycle loop, which normally does this, never
///        starts) and returns false.
bool WaitForAllAtStartup(std::vector<CheckPointControl*>& checkpoint_controls,
                         const std::uint8_t checkpoint,
                         const score::cpp::stop_token& stop_token)
{
    for (std::size_t j{0U}; j < checkpoint_controls.size(); ++j)
    {
        const std::string tag{"[startup worker=" + std::to_string(j) + "] "};
        const int result =
            WaitAndVerifyCheckPoint(tag,
                                    *checkpoint_controls[j],
                                    checkpoint,
                                    stop_token,
                                    std::chrono::duration_cast<std::chrono::milliseconds>(kCheckpointWaitDuration));
        if (result != EXIT_SUCCESS)
        {
            std::cerr << "Worker " << j << " failed at startup" << std::endl;
            for (auto* const cp : checkpoint_controls)
            {
                cp->FinishActions();
            }
            return false;
        }
    }
    return true;
}

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

/// \brief Creates or removes the burst-file with the given \p index under kBaseFolder.
/// \return true on success; logs and returns false on failure.
bool ApplyBurstFileOperation(const pid_t pid, const std::size_t index, const bool create)
{
    const std::string file_path{kBaseFolder + "/" + BurstFileName(index)};
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

/// \brief Runs one TestMode::kWatchChurn cycle: signal+wait, then remove the shared test directory so the
///        next cycle re-exercises the creation path.
bool RunWatchChurnCycle(std::vector<CheckPointControl*>& checkpoint_controls,
                        const std::size_t cycle_index,
                        const score::cpp::stop_token& stop_token)
{
    const std::string tag{"[cycle=" + std::to_string(cycle_index)};
    const bool cycle_ok = SignalAndWaitForAll(checkpoint_controls, kCycleDoneCheckpoint, stop_token, tag);

    const score::filesystem::StandardFilesystem fs;
    const std::string test_dir{TestDir()};
    const auto remove = fs.RemoveAll(test_dir);
    if (!remove.has_value())
    {
        // Non-fatal: directory may not have been created (all workers errored before mkdir)
        std::cerr << "Remove " << test_dir << " info: " << remove.error() << std::endl;
    }

    return cycle_ok;
}

/// \brief Runs one TestMode::kNotifyLatency cycle: announce the change, apply it, then wait for every
///        worker to confirm it observed the matching notification in time.
bool RunNotifyLatencyCycle(const pid_t pid,
                           std::vector<CheckPointControl*>& checkpoint_controls,
                           const std::size_t cycle_index,
                           const score::cpp::stop_token& stop_token)
{
    for (auto* const cp : checkpoint_controls)
    {
        cp->ProceedToNextCheckpoint();
    }

    // Alternate create/delete every cycle, starting with create, matching the expectation each worker
    // independently derives from the cycle index.
    const bool create{(cycle_index % 2U) == 0U};
    if (!ApplyNotifyFileOperation(pid, create))
    {
        return false;
    }

    bool all_ok{true};
    for (std::size_t j{0U}; j < checkpoint_controls.size(); ++j)
    {
        const std::string tag{"[cycle=" + std::to_string(cycle_index) + " worker=" + std::to_string(j) + "] "};
        const int result =
            WaitAndVerifyCheckPoint(tag,
                                    *checkpoint_controls[j],
                                    kCycleDoneCheckpoint,
                                    stop_token,
                                    std::chrono::duration_cast<std::chrono::milliseconds>(kCheckpointWaitDuration));
        if (result != EXIT_SUCCESS)
        {
            std::cerr << "Worker " << j << " failed at cycle " << cycle_index << std::endl;
            all_ok = false;
        }
    }
    return all_ok;
}

/// \brief Runs one TestMode::kBurstLoss cycle:
///   1. Announces the cycle is starting and waits for every worker to confirm its bookkeeping is reset.
///   2. Creates, then removes, every burst file (as fast as possible, no worker interaction in between).
///   3. Announces the cycle has ended and waits for every worker to confirm it observed every expected
///      notification (each worker itself waits config.burst_check_delay before checking).
bool RunBurstLossCycle(const pid_t pid,
                       std::vector<CheckPointControl*>& checkpoint_controls,
                       const std::size_t cycle_index,
                       const std::size_t burst_file_count,
                       const score::cpp::stop_token& stop_token)
{
    const std::string prepare_tag{"[cycle=" + std::to_string(cycle_index) + " prepare"};
    if (!SignalAndWaitForAll(checkpoint_controls, kCycleReadyCheckpoint, stop_token, prepare_tag))
    {
        return false;
    }

    bool files_ok{true};
    for (std::size_t f{0U}; (f < burst_file_count) && files_ok; ++f)
    {
        files_ok = ApplyBurstFileOperation(pid, f, true);
    }
    for (std::size_t f{0U}; (f < burst_file_count) && files_ok; ++f)
    {
        files_ok = ApplyBurstFileOperation(pid, f, false);
    }
    if (!files_ok)
    {
        std::cerr << pid << ": cycle " << cycle_index << ": failed to create/remove burst files" << std::endl;
    }

    const std::string done_tag{"[cycle=" + std::to_string(cycle_index) + " done"};
    const bool workers_ok = SignalAndWaitForAll(checkpoint_controls, kCycleDoneCheckpoint, stop_token, done_tag);

    return files_ok && workers_ok;
}

}  // namespace

bool RunController(std::vector<CheckPointControl*>& checkpoint_controls,
                   const std::size_t num_processes,
                   const std::size_t cycles,
                   const score::cpp::stop_token& stop_token,
                   const StressTestConfig& config)
{
    const auto pid{getpid()};
    static_cast<void>(num_processes);

    if ((config.mode == TestMode::kNotifyLatency) || (config.mode == TestMode::kBurstLoss))
    {
        // Wait for every worker to confirm its inotify watch is armed before performing any file
        // operation — otherwise a worker could still be starting up (e.g. not yet forked/scheduled or
        // mid-AddWatch) when the controller acts, and would miss the notification entirely.
        if (!WaitForAllAtStartup(checkpoint_controls, kWatchReadyCheckpoint, stop_token))
        {
            return false;
        }
    }

    bool test_passed{true};

    for (std::size_t i{0U}; (i < cycles) && test_passed; ++i)
    {
        switch (config.mode)
        {
            case TestMode::kWatchChurn:
                test_passed = RunWatchChurnCycle(checkpoint_controls, i, stop_token);
                break;
            case TestMode::kNotifyLatency:
                test_passed = RunNotifyLatencyCycle(pid, checkpoint_controls, i, stop_token);
                break;
            case TestMode::kBurstLoss:
                test_passed = RunBurstLossCycle(pid, checkpoint_controls, i, config.burst_file_count, stop_token);
                break;
        }
    }

    // Unblock all workers regardless of outcome so they exit cleanly
    for (auto* const cp : checkpoint_controls)
    {
        cp->FinishActions();
    }

    if (config.mode == TestMode::kNotifyLatency)
    {
        // Best-effort cleanup of the shared notify test file, in case the run ended after a "create" cycle.
        static_cast<void>(::unlink((kBaseFolder + "/" + kNotifyTestFileName).c_str()));
    }
    else if (config.mode == TestMode::kBurstLoss)
    {
        // Best-effort cleanup, in case the run ended mid-burst (e.g. a worker error was reported between
        // creating and removing the files).
        for (std::size_t f{0U}; f < config.burst_file_count; ++f)
        {
            static_cast<void>(::unlink((kBaseFolder + "/" + BurstFileName(f)).c_str()));
        }
    }

    return test_passed;
}

}  // namespace score::mw::com::test
