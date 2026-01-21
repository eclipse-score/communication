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

#ifndef SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_TIMEOUT_SUPERVISOR_H
#define SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_TIMEOUT_SUPERVISOR_H

#include <score/callback.hpp>
#include <score/optional.hpp>

#include <sys/types.h>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace score::mw::com::test
{

/// \brief Manages supervision of timeouts. I.e. you can start a timeout supervision of N milliseconds. If it times out
///        before the supervision is stopped again, a user provided callback will be called.
/// \details This helper class is needed as score::os::InterprocessNotification (more exact the
///          InterprocessConditionalVariable it encapsulates) doesn't support a wait with timeout!
class TimeoutSupervisor
{
  public:
    TimeoutSupervisor();
    ~TimeoutSupervisor();

    void StartSupervision(std::chrono::milliseconds timeout, score::cpp::callback<void(void)> timeout_callback);
    void StopSupervision();

  private:
    void Supervision();

    std::mutex mutex_;
    std::condition_variable cond_var_;
    score::cpp::optional<std::chrono::milliseconds> timeout_;
    bool shutdown_;
    score::cpp::callback<void(void)> timeout_callback_{};
    std::thread supervision_thread_;
};

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_TIMEOUT_SUPERVISOR_H
