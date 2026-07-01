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

#include <cstddef>
#include <cstdint>
#include <string>

namespace score::mw::com::test
{

/// \brief Entry point executed in each forked worker process.
///
/// Runs \p cycles iterations. Each iteration:
///   1. Waits for the controller's start signal.
///   2. Creates (or verifies) its dedicated directory and file under kBaseFolder.
///   3. Adds an inotify watch on kBaseFolder, then removes it immediately.
///   4. Reports CheckPointReached to the controller.
void RunWorkerProcess(std::size_t worker_index,
                      std::size_t cycles,
                      CheckPointControl& checkpoint_control);

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
