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

#include "score/mw/com/test/inotify_stress_test/worker.h"

#include "score/mw/com/test/common_test_resources/check_point_control.h"
#include "score/mw/com/test/common_test_resources/general_resources.h"
#include "score/mw/com/test/inotify_stress_test/inotify_stress_test_internal.h"
#include "score/os/inotify.h"
#include "score/os/utils/inotify/inotify_instance_impl.h"

#include <score/stop_token.hpp>
#include <cstdint>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>

namespace score::mw::com::test
{

namespace
{

constexpr mode_t kExpectedDirMode{0755U};

/// \brief Maximum number of add+remove attempts per cycle before a worker reports failure. Tolerates the
///        transient EINVAL the QNX io-notify manager can return under heavy concurrent watch churn.
constexpr std::size_t kMaxWatchAttempts{5U};

/// \brief Returns true when \p path exists and its lower nine permission bits match \p expected_mode.
bool HasCorrectPermissions(const pid_t pid, const std::string& path, const mode_t expected_mode)
{
    struct stat st{};
    if (::stat(path.c_str(), &st) != 0)
    {
        std::cerr << pid << ": stat on " << path << " failed: " << strerror(errno) << std::endl;
        return false;
    }
    if ((st.st_mode & 0777U) != expected_mode)
    {
        std::cerr << pid << ": Directory " << path << " has wrong permissions: actual " << std::oct
                  << (st.st_mode & 0777U) << ", expected " << expected_mode << std::dec << std::endl;
        return false;
    }
    return true;
}

/// \brief Ensures the shared directory exists with the expected mode. Creates it when absent, otherwise
///        verifies its permissions. Returns false (after logging) on any failure.
bool EnsureDirectory(const pid_t pid, const std::string& test_dir)
{
    const int result{::mkdir(test_dir.c_str(), kExpectedDirMode)};
    if (result == 0)
    {
        // This process created the directory and owns it — safe to chmod.
        if (::chmod(test_dir.c_str(), kExpectedDirMode) != 0)
        {
            std::cerr << pid << ": chmod on " << test_dir << " failed: " << strerror(errno) << std::endl;
            return false;
        }
        std::cerr << pid << ": Created directory: " << test_dir << std::endl;
        return true;
    }
    if (errno == EEXIST)
    {
        // Another worker already created it — only verify permissions, never chmod.
        // HasCorrectPermissions logs the specific reason (stat failure or mode mismatch) on failure.
        if (!HasCorrectPermissions(pid, test_dir, kExpectedDirMode))
        {
            return false;
        }
        std::cerr << pid << ": Directory already exists, skipping: " << test_dir << std::endl;
        return true;
    }
    std::cerr << pid << ": mkdir " << test_dir << " failed: " << strerror(errno) << std::endl;
    return false;
}

}  // namespace

void RunWorkerProcess(const std::size_t worker_index, const std::size_t cycles, CheckPointControl& checkpoint_control)
{
    const auto pid{getpid()};
    const std::string test_dir{TestDir()};
    static_cast<void>(worker_index);

    const score::cpp::stop_source worker_stop_source{};
    os::InotifyInstanceImpl inotify{};

    for (std::size_t i{0U}; i < cycles; ++i)
    {
        // Wait for controller's start-of-cycle signal — blocks without polling
        const auto instruction = WaitForChildProceed(checkpoint_control, worker_stop_source.get_token());
        if (instruction != CheckPointControl::ProceedInstruction::PROCEED_NEXT_CHECKPOINT)
        {
            std::cerr << pid << ": Received non-proceed instruction at cycle " << i << ", exiting." << std::endl;
            return;
        }

        // Step 1: Ensure the shared process directory exists with the expected permissions
        if (!EnsureDirectory(pid, test_dir))
        {
            checkpoint_control.ErrorOccurred();
            return;
        }

        // Steps 2 & 3: Add an inotify watch on the shared base folder and remove it immediately.
        // Under heavy concurrent add/remove churn on the same directory, the QNX io-notify resource
        // manager can transiently fail a removal with EINVAL. Once that happens the watch descriptor is
        // permanently invalid — retrying RemoveWatch on the same descriptor keeps returning EINVAL — so
        // recovery requires obtaining a fresh descriptor via AddWatch. Therefore the whole add+remove
        // step is retried a bounded number of times, and the worker only fails if it cannot complete a
        // clean add+remove within kMaxWatchAttempts. Any non-EINVAL error is a genuine failure.
        bool watch_cycle_succeeded{false};
        for (std::size_t attempt{0U}; (attempt < kMaxWatchAttempts) && (!watch_cycle_succeeded); ++attempt)
        {
            auto watch_descriptor =
                inotify.AddWatch(kBaseFolder, os::Inotify::EventMask::kInCreate | os::Inotify::EventMask::kInDelete);
            if (!watch_descriptor.has_value())
            {
                std::cerr << pid << ": AddWatch failed: " << watch_descriptor.error() << " on " << kBaseFolder
                          << std::endl;
                checkpoint_control.ErrorOccurred();
                return;
            }

            const auto remove_result = inotify.RemoveWatch(watch_descriptor.value());
            if (remove_result.has_value())
            {
                watch_cycle_succeeded = true;
            }
            else if (remove_result.error().GetOsDependentErrorCode() != EINVAL)
            {
                std::cerr << pid << ": RemoveWatch failed: " << remove_result.error() << std::endl;
                checkpoint_control.ErrorOccurred();
                return;
            }
            else
            {
                // Transient EINVAL: the descriptor is now invalid, so the next attempt re-adds the watch
                // to obtain a fresh descriptor before removing again.
                std::cerr << pid << ": RemoveWatch transient EINVAL (attempt " << (attempt + 1U) << " of "
                          << kMaxWatchAttempts << "), re-adding watch and retrying" << std::endl;
            }
        }

        if (!watch_cycle_succeeded)
        {
            std::cerr << pid << ": RemoveWatch failed persistently after " << kMaxWatchAttempts << " attempts"
                      << std::endl;
            checkpoint_control.ErrorOccurred();
            return;
        }

        // Notify controller that this cycle completed successfully
        checkpoint_control.CheckPointReached(kCycleDoneCheckpoint);
    }

    // Wait for controller's final FinishActions signal before exiting
    static_cast<void>(WaitForChildProceed(checkpoint_control, worker_stop_source.get_token()));
    inotify.Close();
}

bool SetWorkerCredentials(const std::string& worker_name,
                          const std::uint32_t base_gid,
                          const std::uint32_t base_uid,
                          const std::size_t worker_index)
{
    if (base_gid != 0U)
    {
        if (::setgid(static_cast<gid_t>(base_gid + static_cast<std::uint32_t>(worker_index))) != 0)
        {
            std::cerr << worker_name << ": setgid failed: " << strerror(errno) << std::endl;
            return false;
        }
    }
    if (base_uid != 0U)
    {
        if (::setuid(static_cast<uid_t>(base_uid + static_cast<std::uint32_t>(worker_index))) != 0)
        {
            std::cerr << worker_name << ": setuid failed: " << strerror(errno) << std::endl;
            return false;
        }
    }
    return true;
}

}  // namespace score::mw::com::test
