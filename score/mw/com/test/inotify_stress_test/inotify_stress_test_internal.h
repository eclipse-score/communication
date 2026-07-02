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

/// Maximum time the controller waits for a single worker to complete a cycle.
constexpr std::chrono::seconds kCheckpointWaitDuration{30U};

/// Returns the per-worker sub-directory path for the given worker index.
inline std::string ProcessDir(const std::size_t worker_index)
{
    return kBaseFolder + "/process_" + std::to_string(worker_index);
}

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_INOTIFY_STRESS_TEST_INTERNAL_H
