/********************************************************************************
 * Copyright (C) 2017-2026 LG Electronics Inc.
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ********************************************************************************/

#include <atomic>
#include <gtest/gtest.h>
#include <stdexcept>
#include <vector>

#include <utils/thread/ThreadPool.h>

using namespace lgsomeip;

TEST(ThreadPool, returns_job_result) {
    ThreadPool pool(2);

    auto result = pool.enqueue_job([](int first, int second) { return first + second; }, 2, 3);

    EXPECT_EQ(result.get(), 5);
}

TEST(ThreadPool, propagates_job_exception_through_future) {
    ThreadPool pool(1);

    auto result = pool.enqueue_job([]() -> int { throw std::runtime_error("job failure"); });

    EXPECT_THROW(result.get(), std::runtime_error);
}

TEST(ThreadPool, drains_queued_jobs_before_destruction) {
    std::atomic<int> completed_jobs{0};
    std::vector<std::future<void>> results;

    {
        ThreadPool pool(2);
        for (int index = 0; index < 8; ++index) {
            results.push_back(pool.enqueue_job([&completed_jobs] { ++completed_jobs; }));
        }

        for (auto& result : results) {
            result.get();
        }
    }

    EXPECT_EQ(completed_jobs.load(), 8);
}
