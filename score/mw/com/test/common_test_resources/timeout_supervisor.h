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

#include "score/concurrency/thread_pool.h"
#include "score/concurrency/timed_executor/concurrent_timed_executor.h"

#include <score/callback.hpp>
#include <score/stop_token.hpp>

#include <chrono>
#include <mutex>

namespace score::mw::com::test
{

/// \brief Manages supervision of timeouts. I.e. you can start a timeout supervision of N milliseconds. If it times out
///        before the supervision is stopped again, a user provided callback will be called.
/// \details This helper class is needed as score::os::InterprocessNotification (more exact the
///          InterprocessConditionalVariable it encapsulates) doesn't support a wait with timeout!
class TimeoutSupervisor
{
  public:
    TimeoutSupervisor() = default;
    TimeoutSupervisor(const TimeoutSupervisor&) = delete;
    TimeoutSupervisor(TimeoutSupervisor&&) = delete;
    TimeoutSupervisor& operator=(const TimeoutSupervisor&) = delete;
    TimeoutSupervisor& operator=(TimeoutSupervisor&&) = delete;
    ~TimeoutSupervisor();

    void StartSupervision(std::chrono::milliseconds timeout, score::cpp::callback<void(void)> timeout_callback);
    void StopSupervision();

  private:
    std::mutex mutex_;
    score::cpp::stop_source abort_source_{};
    score::concurrency::ConcurrentTimedExecutor<std::chrono::steady_clock> executor_{
        score::cpp::pmr::new_delete_resource(),
        score::cpp::pmr::make_unique<score::concurrency::ThreadPool>(score::cpp::pmr::new_delete_resource(), 1U)};
};

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_TIMEOUT_SUPERVISOR_H
