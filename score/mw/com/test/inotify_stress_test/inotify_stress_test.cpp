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

/// \brief Inotify stress test — main entry point, argument parsing, setup and orchestration.
///
/// Spawns M worker processes. Each worker is optionally assigned a unique UID/GID. Every worker
/// owns a dedicated sub-directory and file under the shared base folder. On each cycle the worker:
///   1. Creates its directory (skipped when it already exists with the correct access rights).
///   2. Creates its file     (skipped when it already exists with the correct access rights).
///   3. Adds  an inotify watch on the shared base folder.
///   4. Removes the watch immediately — no artificial delay.
///
/// A controller (parent process) drives synchronisation via CheckPointControl:
///   - ProceedToNextCheckpoint() signals each worker to begin a cycle.
///   - WaitAndVerifyCheckPoint() waits for workers to report CheckPointReached or ErrorOccurred.
///   - After all cycles the controller sends FinishActions() so workers exit cleanly.

#include "score/filesystem/details/standard_filesystem.h"
#include "score/mw/com/test/common_test_resources/child_process_guard.h"
#include "score/mw/com/test/common_test_resources/shared_memory_object_creator.h"
#include "score/mw/com/test/inotify_stress_test/controller.h"
#include "score/mw/com/test/inotify_stress_test/inotify_stress_test_internal.h"
#include "score/mw/com/test/inotify_stress_test/worker.h"

#include "score/mw/com/test/common_test_resources/check_point_control.h"
#include "score/mw/com/test/common_test_resources/general_resources.h"

#include <boost/program_options/cmdline.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/value_semantic.hpp>
#include <boost/program_options/variables_map.hpp>
#include <score/stop_token.hpp>
#include <chrono>

#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace score::mw::com::test
{

namespace
{

const std::string kShmNamePrefix{"inotify_stress_test_worker_"};

int DoSetup()
{
    score::filesystem::StandardFilesystem fs;
    const auto create_dir = fs.CreateDirectory(kBaseFolder);
    if (!create_dir.has_value())
    {
        std::cerr << getpid() << ": Create base directory failed: " << create_dir.error() << std::endl;
        return -1;
    }
    // Allow non-root worker processes (when --base-uid / --base-gid are used) to create
    // subdirectories and files inside the shared base folder.
    if (::chmod(kBaseFolder.c_str(), 0777) != 0)
    {
        std::cerr << getpid() << ": chmod on base directory failed: " << strerror(errno) << std::endl;
        return -1;
    }
    std::cerr << getpid() << ": Created base directory: " << kBaseFolder << std::endl;
    return 0;
}

int ParseArguments(int argc,
                   const char** argv,
                   std::size_t& num_processes,
                   std::size_t& cycles,
                   std::uint32_t& base_uid,
                   std::uint32_t& base_gid)
{
    namespace po = boost::program_options;

    po::options_description options;
    // clang-format off
    options.add_options()
        ("help",          "Display the help message")
        ("num-processes", po::value<std::size_t>(&num_processes)->default_value(5U),
                          "Number of worker processes to spawn")
        ("cycles",        po::value<std::size_t>(&cycles)->default_value(100U),
                          "Number of stress cycles before terminating")
        ("base-uid",      po::value<std::uint32_t>(&base_uid)->default_value(0U),
                          "Base UID: worker N calls setuid(base-uid + N). 0 = skip setuid.")
        ("base-gid",      po::value<std::uint32_t>(&base_gid)->default_value(0U),
                          "Base GID: worker N calls setgid(base-gid + N). 0 = skip setgid.");
    // clang-format on

    po::variables_map args;
    const auto parsed_args =
        po::command_line_parser{argc, argv}
            .options(options)
            .style(po::command_line_style::unix_style | po::command_line_style::allow_long_disguise)
            .run();
    po::store(parsed_args, args);

    if (args.count("help") > 0U)
    {
        std::cerr << options << std::endl;
        return -1;
    }

    po::notify(args);
    return 0;
}

/// \brief Creates SHM checkpoints, forks workers, runs the controller loop, then cleans up.
int RunStressTest(const std::size_t num_processes,
                  const std::size_t cycles,
                  const std::uint32_t base_uid,
                  const std::uint32_t base_gid)
{
    const score::cpp::stop_source stop_source{};

    // Create one CheckPointControl in shared memory per worker — must happen BEFORE fork
    std::vector<SharedMemoryObjectCreator<CheckPointControl>> shmem_guards{};
    shmem_guards.reserve(num_processes);

    // CheckPointControl stores the owner name as a non-owning std::string_view, so the backing
    // strings must outlive every CheckPointControl — keep them alive for the whole function.
    std::vector<std::string> owner_names{};
    owner_names.reserve(num_processes);

    for (std::size_t i{0U}; i < num_processes; ++i)
    {
        const std::string shm_name{kShmNamePrefix + std::to_string(i)};
        const std::string& owner_name{owner_names.emplace_back("Worker_" + std::to_string(i))};
        auto guard_result = CreateOrOpenSharedCheckPointControl("main", shm_name, owner_name);
        if (!guard_result.has_value())
        {
            std::cerr << "Failed to create CheckPointControl for worker " << i << std::endl;
            return -1;
        }
        shmem_guards.push_back(std::move(guard_result.value()));
    }

    // Collect raw pointers — SHM addresses remain stable across the vector lifetime
    std::vector<CheckPointControl*> checkpoint_controls{};
    checkpoint_controls.reserve(num_processes);
    for (auto& guard : shmem_guards)
    {
        checkpoint_controls.push_back(&guard.GetObject());
    }

    // Fork worker processes
    std::vector<ChildProcessGuard> child_guards{};
    child_guards.reserve(num_processes);

    for (std::size_t worker{0U}; worker < num_processes; ++worker)
    {
        CheckPointControl& cp = *checkpoint_controls[worker];
        const std::string worker_name{"Worker_" + std::to_string(worker)};

        auto guard_opt = ForkProcessAndRunInChildProcess(
            "main", worker_name, [&cp, worker, cycles, base_uid, base_gid, worker_name]() {
                if (!SetWorkerCredentials(worker_name, base_gid, base_uid, worker))
                {
                    cp.ErrorOccurred();
                    return;
                }
                RunWorkerProcess(worker, cycles, cp);
            });

        if (!guard_opt.has_value())
        {
            std::cerr << "Failed to fork worker " << worker << ", aborting." << std::endl;
            for (auto& g : child_guards)
            {
                static_cast<void>(g.KillChildProcess());
            }
            return -1;
        }
        child_guards.push_back(guard_opt.value());
    }

    const bool test_passed = RunController(checkpoint_controls, num_processes, cycles, stop_source.get_token());

    // Wait for all worker processes to exit
    constexpr std::chrono::milliseconds worker_termination_timeout{5000U};
    for (auto& guard : child_guards)
    {
        if (!WaitForChildProcessToTerminate("main", guard, worker_termination_timeout))
        {
            std::cerr << "Worker PID " << guard.GetPid() << " did not terminate in time, killing." << std::endl;
            static_cast<void>(guard.KillChildProcess());
        }
    }

    // Release shared memory
    for (auto& guard : shmem_guards)
    {
        guard.CleanUp();
    }

    return test_passed ? EXIT_SUCCESS : EXIT_FAILURE;
}

}  // namespace

}  // namespace score::mw::com::test

int main(int argc, const char** argv)
{
    std::cerr << "inotify_stress_test: starting" << std::endl;

    std::size_t num_processes{0U};
    std::size_t cycles{0U};
    std::uint32_t base_uid{0U};
    std::uint32_t base_gid{0U};

    if (score::mw::com::test::ParseArguments(argc, argv, num_processes, cycles, base_uid, base_gid) == -1)
    {
        return -1;
    }

    if (score::mw::com::test::DoSetup() == -1)
    {
        return -1;
    }

    return score::mw::com::test::RunStressTest(num_processes, cycles, base_uid, base_gid);
}
