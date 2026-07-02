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

#include "score/filesystem/filesystem.h"
#include "score/mw/com/test/common_test_resources/general_resources.h"
#include "score/mw/com/test/inotify_stress_test/inotify_stress_test_internal.h"
#include "score/os/utils/inotify/inotify_instance_impl.h"

#include <score/stop_token.hpp>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

namespace score::mw::com::test
{

namespace
{

const std::string kTestFileName{"test_file"};
constexpr mode_t kExpectedDirMode{0755U};
constexpr mode_t kExpectedFileMode{0644U};

std::string ProcessFile(const std::size_t worker_index)
{
    return ProcessDir(worker_index) + "/" + kTestFileName;
}

/// \brief Returns true when \p path exists and its lower nine permission bits match \p expected_mode.
bool HasCorrectPermissions(const std::string& path, const mode_t expected_mode)
{
    struct stat st{};
    if (::stat(path.c_str(), &st) != 0)
    {
        return false;
    }
    return (st.st_mode & 0777U) == expected_mode;
}

}  // namespace

void RunWorkerProcess(const std::size_t worker_index,
                      const std::size_t cycles,
                      CheckPointControl& checkpoint_control)
{
    const auto pid{getpid()};
    const std::string process_dir{ProcessDir(worker_index)};
    const std::string process_file{ProcessFile(worker_index)};

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

        // Step 1: Create process directory if it does not exist or has wrong access rights
        if (!HasCorrectPermissions(process_dir, kExpectedDirMode))
        {
            const score::filesystem::StandardFilesystem fs;
            const auto create_dir = fs.CreateDirectory(process_dir);
            if (!create_dir.has_value())
            {
                std::cerr << pid << ": CreateDirectory " << process_dir << " failed: " << create_dir.error()
                          << std::endl;
                checkpoint_control.ErrorOccurred();
                return;
            }
            if (::chmod(process_dir.c_str(), kExpectedDirMode) != 0)
            {
                std::cerr << pid << ": chmod on " << process_dir << " failed: " << strerror(errno) << std::endl;
                checkpoint_control.ErrorOccurred();
                return;
            }
            std::cerr << pid << ": Created directory: " << process_dir << std::endl;
        }
        else
        {
            std::cerr << pid << ": Directory exists with correct permissions, skipping: " << process_dir << std::endl;
        }

        // Step 2: Create test file if it does not exist or has wrong access rights
        if (!HasCorrectPermissions(process_file, kExpectedFileMode))
        {
            std::ofstream file{process_file};
            if (!file.good())
            {
                std::cerr << pid << ": Create file " << process_file << " failed" << std::endl;
                checkpoint_control.ErrorOccurred();
                return;
            }
            file.close();
            if (::chmod(process_file.c_str(), kExpectedFileMode) != 0)
            {
                std::cerr << pid << ": chmod on " << process_file << " failed: " << strerror(errno) << std::endl;
                checkpoint_control.ErrorOccurred();
                return;
            }
            std::cerr << pid << ": Created file: " << process_file << std::endl;
        }
        else
        {
            std::cerr << pid << ": File exists with correct permissions, skipping: " << process_file << std::endl;
        }

        // Step 3: Add inotify watch on the shared base folder (concurrent across all workers)
        auto watch_descriptor =
            inotify.AddWatch(kBaseFolder, os::Inotify::EventMask::kInCreate | os::Inotify::EventMask::kInDelete);
        if (!watch_descriptor.has_value())
        {
            std::cerr << pid << ": AddWatch failed: " << watch_descriptor.error() << " on " << kBaseFolder
                      << std::endl;
            checkpoint_control.ErrorOccurred();
            return;
        }

        // Step 4: Remove watch immediately — no artificial delay
        const auto remove_result = inotify.RemoveWatch(watch_descriptor.value());
        if (!remove_result.has_value())
        {
            std::cerr << pid << ": RemoveWatch failed" << std::endl;
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
