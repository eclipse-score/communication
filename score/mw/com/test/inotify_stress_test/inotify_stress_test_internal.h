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

#ifndef SCORE_MW_COM_TEST_INOTIFY_STRESS_TEST_INTERNAL_H
#define SCORE_MW_COM_TEST_INOTIFY_STRESS_TEST_INTERNAL_H

#include <chrono>
#include <cstdint>
#include <string>

#if defined(__QNXNTO__)
inline const std::string kBaseFolder{"/tmp_discovery/inotify_stress_test"};
#else
inline const std::string kBaseFolder{"/tmp/inotify_stress_test"};
#endif

namespace score::mw::com::test
{

/// Checkpoint number reported by each worker upon completing one stress cycle.
constexpr std::uint8_t kCycleDoneCheckpoint{1U};

/// Checkpoint number reported once by each worker in kNotifyLatency mode after its inotify watch has been
/// established, before the cycle loop starts. The controller waits for this from every worker before
/// performing the first file operation, so no notification can be missed due to a startup race between
/// forking workers and the controller acting.
constexpr std::uint8_t kWatchReadyCheckpoint{2U};

/// Maximum time the controller waits for a single worker to complete a cycle.
constexpr std::chrono::seconds kCheckpointWaitDuration{30U};

/// \brief Selects which aspect of the inotify subsystem a stress-test run exercises.
enum class TestMode : std::uint8_t
{
    /// Repeatedly adds and removes an inotify watch on the shared base folder (original behavior).
    kWatchChurn,
    /// Verifies that create/delete notifications on the shared base folder are dispatched to every
    /// watching worker within a configurable time span after the controller announces the change.
    kNotifyLatency,
};

/// Default maximum time a worker may wait for the expected inotify notification in kNotifyLatency mode.
constexpr std::chrono::milliseconds kDefaultNotifyTimeout{300U};

/// Name of the file the controller creates/removes under kBaseFolder in kNotifyLatency mode.
inline const std::string kNotifyTestFileName{"notify_latency_test_file"};

/// Returns the single shared directory that every worker concurrently attempts to create.
inline std::string TestDir()
{
    return kBaseFolder + "/shared_process";
}

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_INOTIFY_STRESS_TEST_INTERNAL_H
