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

#ifndef SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_SYNC_UTILS_H
#define SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_SYNC_UTILS_H

#include "score/concurrency/future/interruptible_promise.h"

#include <score/jthread.hpp>
#include <score/stop_token.hpp>

namespace score::mw::com::test
{

class SyncCoordinator
{
  public:
    SyncCoordinator(std::string file_name);
    ~SyncCoordinator() = default;
    void Signal() noexcept;
    static void CleanUp(const std::string& file_name) noexcept;
    score::concurrency::InterruptibleFuture<void> Wait(const score::cpp::stop_token& stop_token) noexcept;

  private:
    void CheckFileCreation(std::shared_ptr<score::concurrency::InterruptiblePromise<void>> promise,
                           const score::cpp::stop_token& stop_token) noexcept;
    std::string file_name_;
    score::cpp::jthread checkfile_thread_;
};

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_SYNC_UTILS_H
