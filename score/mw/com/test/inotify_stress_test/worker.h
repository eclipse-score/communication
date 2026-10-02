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

#ifndef SCORE_MW_COM_TEST_INOTIFY_STRESS_TEST_WORKER_H
#define SCORE_MW_COM_TEST_INOTIFY_STRESS_TEST_WORKER_H

#include "score/mw/com/test/common_test_resources/check_point_control.h"
#include "score/mw/com/test/inotify_stress_test/inotify_stress_test_internal.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

namespace score::mw::com::test
{

/// \brief Entry point executed in each forked worker process.
///
/// Runs \p cycles iterations. Behavior depends on \p config.mode:
///
/// In TestMode::kWatchChurn each iteration:
///   1. Waits for the controller's start signal.
///   2. Creates (or verifies) its dedicated directory and file under kBaseFolder.
///   3. Adds an inotify watch on kBaseFolder, then removes it immediately.
///   4. Reports CheckPointReached to the controller.
///
/// In TestMode::kNotifyLatency an inotify watch on kBaseFolder is added once before the loop, and each
/// iteration:
///   1. Waits for the controller's signal that it is about to create/remove the shared notify test file
///      (alternating create/delete every cycle, starting with create).
///   2. Waits up to \p config.notify_timeout for the matching inotify event to arrive.
///   3. Reports CheckPointReached on success, or ErrorOccurred if the event doesn't arrive in time (or is
///      unexpected).
///
/// In TestMode::kBurstLoss an inotify watch on kBaseFolder is added once before the loop, and a background
/// thread continuously drains inotify events for the lifetime of the worker (so a burst of many events
/// arriving faster than the main thread cycles through checkpoints is never missed). Each iteration:
///   1. Waits for the controller's "cycle starting" signal, resets its per-file create/delete bookkeeping,
///      and reports kCycleReadyCheckpoint.
///   2. Waits for the controller's "cycle ended" signal (sent once it has created and removed all
///      config.burst_file_count files for this cycle).
///   3. Waits config.burst_check_delay to let any still in-flight notifications arrive.
///   4. Reports CheckPointReached if every file's create AND delete notification was observed *in the
///      expected order* (all creates ascending, then all deletes ascending — matching the order the
///      controller performed them in); otherwise logs which/how many are missing, or the first reordering
///      violation detected, and reports ErrorOccurred.
void RunWorkerProcess(std::size_t worker_index,
                      std::size_t cycles,
                      CheckPointControl& checkpoint_control,
                      const StressTestConfig& config);

/// \brief Sets the GID and UID of the calling process for worker \p worker_index.
///
/// GID is changed before UID so that root privileges are still held when setgid is called.
/// If \p base_gid or \p base_uid is zero the corresponding call is skipped.
///
/// \return true if all requested credential changes succeeded; false otherwise.
bool SetWorkerCredentials(const std::string& worker_name,
                          std::uint32_t base_gid,
                          std::uint32_t base_uid,
                          std::size_t worker_index);

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_INOTIFY_STRESS_TEST_WORKER_H
