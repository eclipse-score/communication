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

#include "ThreadPool.h"

#include <utils/log/formatLog.h>
#include <utils/log/logger.h>

namespace lgsomeip {

ThreadPool::ThreadPool(std::size_t thread_count) : stop_requested_(false), thread_count_(thread_count) {
    LGSOMEIP_LOG_DEBUG << "ThreadPool::ThreadPool / Number of threads : " << this->thread_count_;

    worker_threads_.reserve(thread_count_);
    for (std::size_t index = 0; index < thread_count_; ++index) {
        worker_threads_.emplace_back([this]() {
            pthread_setname_np(pthread_self(), "SomeipMsgCB");
            this->run_thread();
        });
    }
}

ThreadPool::~ThreadPool() {
    stop_requested_ = true;
    job_condition_.notify_all();

    for (auto& worker_thread : worker_threads_) {
        worker_thread.join();
    }
}

void ThreadPool::run_thread() {
    while (true) {
        std::unique_lock<std::mutex> lock(job_mutex_);
        job_condition_.wait(lock, [this]() { return !this->jobs_.empty() || stop_requested_; });
        if (stop_requested_ && this->jobs_.empty()) {
            return;
        }

        LGSOMEIP_LOG_DEBUG << "ThreadPool::run_thread / Pop the job from queue and run it";

        std::function<void()> front_job = std::move(jobs_.front());
        jobs_.pop();
        lock.unlock();

        front_job();
    }
}

} // namespace lgsomeip
