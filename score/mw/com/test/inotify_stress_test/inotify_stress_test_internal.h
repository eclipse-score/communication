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
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <sstream>
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

/// Checkpoint number reported once by each worker in kNotifyLatency/kBurstLoss mode after its inotify watch
/// has been established, before the cycle loop starts. The controller waits for this from every worker
/// before performing the first file operation, so no notification can be missed due to a startup race
/// between forking workers and the controller acting.
constexpr std::uint8_t kWatchReadyCheckpoint{2U};

/// Checkpoint number reported by each worker in kBurstLoss mode once it has reset its per-cycle bookkeeping
/// and is ready to observe a new burst. The controller waits for this from every worker before creating any
/// burst file, to avoid a race between beginning a new cycle and a worker still resetting its bookkeeping
/// from the previous one.
constexpr std::uint8_t kCycleReadyCheckpoint{3U};

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
    /// Verifies that every create/delete notification for a burst of many files created (then removed)
    /// back-to-back in one shot is eventually observed by every watching worker. Models production reports
    /// of processes that watch many events occasionally missing one, suspected to be caused by an inotify
    /// event queue overflow under load.
    kBurstLoss,
};

/// Default maximum time a worker may wait for the expected inotify notification in kNotifyLatency mode.
constexpr std::chrono::milliseconds kDefaultNotifyTimeout{300U};

/// Name of the file the controller creates/removes under kBaseFolder in kNotifyLatency mode.
inline const std::string kNotifyTestFileName{"notify_latency_test_file"};

/// Default number of files created (then removed) per cycle in kBurstLoss mode.
constexpr std::size_t kDefaultBurstFileCount{500U};

/// Default grace period a worker waits, after the controller announces a kBurstLoss cycle has ended, before
/// checking whether every expected notification has arrived.
constexpr std::chrono::milliseconds kDefaultBurstCheckDelay{500U};

/// Prefix of the files created/removed under kBaseFolder in kBurstLoss mode. Followed by a zero-padded,
/// consecutive file index (see BurstFileName()).
inline const std::string kBurstFilePrefix{"burst_file_"};

/// \brief Returns the (name-only, no path) file name used for burst-file \p index in kBurstLoss mode.
/// Zero-padded to a fixed width so names sort consistently and are easy to spot in directory listings.
inline std::string BurstFileName(const std::size_t index)
{
    std::ostringstream oss;
    oss << kBurstFilePrefix << std::setw(6) << std::setfill('0') << index;
    return oss.str();
}

/// \brief Bundles the settings that determine what a worker/controller pair actually does, so they don't
/// have to be threaded through individually as the set of modes (and their mode-specific knobs) grows.
struct StressTestConfig
{
    TestMode mode{TestMode::kWatchChurn};
    /// kNotifyLatency only: see kDefaultNotifyTimeout.
    std::chrono::milliseconds notify_timeout{kDefaultNotifyTimeout};
    /// kBurstLoss only: number of files created (then removed) per cycle.
    std::size_t burst_file_count{kDefaultBurstFileCount};
    /// kBurstLoss only: see kDefaultBurstCheckDelay.
    std::chrono::milliseconds burst_check_delay{kDefaultBurstCheckDelay};
};

/// Returns the single shared directory that every worker concurrently attempts to create.
inline std::string TestDir()
{
    return kBaseFolder + "/shared_process";
}

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_INOTIFY_STRESS_TEST_INTERNAL_H
