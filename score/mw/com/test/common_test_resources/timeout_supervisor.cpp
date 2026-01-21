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

#include <sys/types.h>
#include <chrono>
#include <iostream>
#include <mutex>

namespace score::mw::com::test
{

TimeoutSupervisor::TimeoutSupervisor()
    : timeout_{}, shutdown_{false}, supervision_thread_{&TimeoutSupervisor::Supervision, this}
{
}

TimeoutSupervisor::~TimeoutSupervisor()
{
    {
        std::lock_guard<std::mutex> lock_guard{mutex_};
        shutdown_ = true;
    }

    cond_var_.notify_one();
    supervision_thread_.join();
}

void TimeoutSupervisor::StartSupervision(std::chrono::milliseconds timeout_milliseconds,
                                         score::cpp::callback<void(void)> timeout_callback)
{
    {
        std::lock_guard<std::mutex> lock_guard{mutex_};
        timeout_callback_ = std::move(timeout_callback);
        if (timeout_.has_value())
        {
            std::cerr << "Error: Calling StartSupervision() although it is currently running. Stop it first!"
                      << std::endl;
        }
        timeout_ = timeout_milliseconds;
    }
    cond_var_.notify_one();
}

void TimeoutSupervisor::StopSupervision()
{
    {
        std::lock_guard<std::mutex> lock_guard{mutex_};
        timeout_.reset();
    }

    cond_var_.notify_one();
}

void TimeoutSupervisor::Supervision()
{
    while (!shutdown_)
    {
        std::unique_lock lk(mutex_);
        if (!timeout_.has_value())
        {
            cond_var_.wait(lk, [this]() {
                return (shutdown_ || timeout_.has_value());
            });
        }
        else
        {
            auto status = cond_var_.wait_for(lk, std::chrono::milliseconds{timeout_.value()});
            lk.unlock();
            if (status == std::cv_status::timeout)
            {
                if (!timeout_callback_.empty())
                {
                    timeout_callback_();
                }
                else
                {
                    std::cerr << "TimeoutSupervisor: Error - Empty timeout_callback_ in TimeoutSupervisor!"
                              << std::endl;
                }
            }
        }
    }
}

}  // namespace score::mw::com::test
