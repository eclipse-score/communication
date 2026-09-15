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

#ifndef SCORE_MW_COM_TEST_INOTIFY_STRESS_TEST_CONTROLLER_H
#define SCORE_MW_COM_TEST_INOTIFY_STRESS_TEST_CONTROLLER_H

#include "score/mw/com/test/common_test_resources/check_point_control.h"
#include "score/mw/com/test/inotify_stress_test/inotify_stress_test_internal.h"

#include <score/stop_token.hpp>

#include <cstddef>
#include <vector>

namespace score::mw::com::test
{

/// \brief Runs the controller loop for \p cycles iterations. Behavior depends on \p mode.
///
/// In TestMode::kNotifyLatency, before the first cycle the controller first waits for every worker to
/// report kWatchReadyCheckpoint (confirming its inotify watch is armed), to avoid a startup race where the
/// controller could act before a worker is ready to observe it.
///
/// In TestMode::kWatchChurn each iteration:
///   1. Signals every worker via ProceedToNextCheckpoint().
///   2. Waits for every worker to report CheckPointReached or ErrorOccurred.
///   3. Removes per-worker directories so the next cycle exercises the creation path.
///
/// In TestMode::kNotifyLatency each iteration:
///   1. Signals every worker via ProceedToNextCheckpoint(), announcing that the controller is about to
///      create/remove the shared notify test file (alternating create/delete every cycle, starting with
///      create).
///   2. Performs that file operation on kNotifyTestFileName under kBaseFolder.
///   3. Waits for every worker to report CheckPointReached (i.e. it observed the matching notification in
///      time) or ErrorOccurred.
///
/// Sends FinishActions() to all workers before returning regardless of outcome.
///
/// \return true if all cycles completed without any worker error; false otherwise.
bool RunController(std::vector<CheckPointControl*>& checkpoint_controls,
                   std::size_t num_processes,
                   std::size_t cycles,
                   const score::cpp::stop_token& stop_token,
                   TestMode mode);

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_INOTIFY_STRESS_TEST_CONTROLLER_H
