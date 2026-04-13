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

#include "score/mw/com/test/common_test_resources/timeout_supervisor.h"

#include "score/concurrency/timed_executor/delayed_task.h"

namespace score::mw::com::test
{

TimeoutSupervisor::~TimeoutSupervisor()
{
    const std::scoped_lock lock{mutex_};
    abort_source_.request_stop();
    executor_.Shutdown();
}

void TimeoutSupervisor::StartSupervision(std::chrono::milliseconds timeout_milliseconds,
                                         score::cpp::callback<void(void)> timeout_callback)
{
    const std::scoped_lock lock{mutex_};
    auto abort_token = abort_source_.get_token();
    auto task = score::concurrency::DelayedTaskFactory::Make<std::chrono::steady_clock>(
        score::cpp::pmr::new_delete_resource(),
        std::chrono::steady_clock::now() + timeout_milliseconds,
        [abort_token, timeout_callback = std::move(timeout_callback)](const auto& stop_token, auto) -> void {
            if (stop_token.stop_requested() || abort_token.stop_requested())
            {
                return;
            }

            timeout_callback();
        });
    executor_.Post(std::move(task));
}

void TimeoutSupervisor::StopSupervision()
{
    const std::scoped_lock lock{mutex_};
    abort_source_.request_stop();
    abort_source_ = score::cpp::stop_source{};
}

}  // namespace score::mw::com::test
