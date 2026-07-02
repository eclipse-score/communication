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

#include "score/filesystem/filesystem.h"
#include "score/mw/com/test/common_test_resources/general_resources.h"
#include "score/mw/com/test/inotify_stress_test/inotify_stress_test_internal.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace score::mw::com::test
{

bool RunController(std::vector<CheckPointControl*>& checkpoint_controls,
                   const std::size_t num_processes,
                   const std::size_t cycles,
                   const score::cpp::stop_token& stop_token)
{
    const auto pid{getpid()};
    bool test_passed{true};

    for (std::size_t i{0U}; i < cycles; ++i)
    {
        // Signal all workers to begin this cycle
        for (std::size_t j{0U}; j < checkpoint_controls.size(); ++j)
        {
            std::cerr << pid << ": Signalling worker " << j << " to start (cycle " << i << ")" << std::endl;
            checkpoint_controls[j]->ProceedToNextCheckpoint();
        }

        // Wait for every worker to report checkpoint or error — collect all before deciding to abort
        for (std::size_t j{0U}; j < checkpoint_controls.size(); ++j)
        {
            const std::string tag{"[cycle=" + std::to_string(i) + " worker=" + std::to_string(j) + "] "};
            const int result =
                WaitAndVerifyCheckPoint(tag, *checkpoint_controls[j], kCycleDoneCheckpoint, stop_token,
                                        std::chrono::duration_cast<std::chrono::milliseconds>(kCheckpointWaitDuration));
            if (result != EXIT_SUCCESS)
            {
                std::cerr << pid << ": Worker " << j << " failed at cycle " << i << std::endl;
                test_passed = false;
            }
        }

        // Remove per-process directories so the next cycle exercises the creation path again
        const score::filesystem::StandardFilesystem fs;
        for (std::size_t child{0U}; child < num_processes; ++child)
        {
            const std::string process_dir{ProcessDir(child)};
            const auto remove = fs.RemoveAll(process_dir);
            if (!remove.has_value())
            {
                // Non-fatal: directory may not have been created (worker errored before mkdir)
                std::cerr << pid << ": RemoveAll " << process_dir << " info: " << remove.error() << std::endl;
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

    return test_passed;
}

}  // namespace score::mw::com::test
