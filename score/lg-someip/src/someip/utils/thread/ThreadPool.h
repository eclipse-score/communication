/********************************************************************************
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
 *
 ********************************************************************************/

#ifndef LG_SOMEIP_THREAD_POOL_H
#define LG_SOMEIP_THREAD_POOL_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <tuple>
#include <type_traits>
#include <vector>

#include <utils/log/formatLog.h>
#include <utils/log/logger.h>

namespace lgsomeip {
class ThreadPool {
public:
    ThreadPool(std::size_t thread_count);
    ~ThreadPool();

    template <class F, class... Args> std::future<std::invoke_result_t<F, Args...>> enqueue_job(F&& f, Args&&... args);

private:
    std::atomic<bool> stop_requested_;
    std::size_t thread_count_;
    std::vector<std::thread> worker_threads_;
    std::queue<std::function<void()>> jobs_;
    std::condition_variable job_condition_;
    std::mutex job_mutex_;

    void run_thread();
};

template <class F, class... Args>
std::future<std::invoke_result_t<F, Args...>> ThreadPool::enqueue_job(F&& f, Args&&... args) {
    if (stop_requested_) {
        throw std::runtime_error("ThreadPool::enqueue_job / Stop thread pool");
    }

    using return_type = std::invoke_result_t<F, Args...>;

    // The callable and the arguments are copied into the job and the callable is invoked with them as lvalues.
    auto job = std::make_shared<std::packaged_task<return_type()>>(
        [function = std::forward<F>(f), arguments = std::make_tuple(std::forward<Args>(args)...)]() mutable {
            return std::apply(function, arguments);
        });
    std::future<return_type> future_result = job->get_future();
    {
        LGSOMEIP_LOG_DEBUG << "ThreadPool::enqueue_job / Push the job to queue.";

        std::lock_guard<std::mutex> lock(job_mutex_);
        jobs_.push([job]() { (*job)(); });
    }
    job_condition_.notify_one();

    return future_result;
}

} // namespace lgsomeip

#endif // LG_SOMEIP_THREAD_POOL_H
