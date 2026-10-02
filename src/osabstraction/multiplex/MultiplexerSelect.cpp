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

#include "Multiplexer.h"
#include <errno.h>
#include <map>
#include <memory>
#include <cstring>
#include <thread>

namespace lgsomeip {
namespace osabstraction {

// If LINUX is not defined, the Select Multiplexer is executed.
Multiplexer::Multiplexer() {
    FD_ZERO(&master_fd_set_);
}

Multiplexer::~Multiplexer() {}

void Multiplexer::start() {
    running_ = true;
    thread_ = std::make_shared<std::thread>(&Multiplexer::run, this);
    // Assign a descriptive name to this worker thread.
    pthread_setname_np(thread_->native_handle(), "SomeipSelectMux");
}

void Multiplexer::stop() {
    if (running_.load()) {
        running_.store(false);
        // Wake the multiplexer thread.
        condition_.notify_one();
    }
}

void Multiplexer::join() {
    if (thread_ != nullptr && thread_->joinable()) {
        thread_->join();
    }
}

void Multiplexer::run() {
    struct timeval timeout;
    fd_set working_set;
    int select_result, descriptors_ready, first_file_descriptor, max_file_descriptor;

    while (running_.load()) {
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        // sleep thread when callback table is empty.
        std::map<int, std::function<void(bool)>> callbacks_snapshot;
        {
            std::unique_lock<std::mutex> lock(callbacks_mutex_);
            condition_.wait(lock, [this] { return !callbacks_.empty() || !running_.load(); });
            if (!running_.load()) {
                break;
            }
            callbacks_snapshot = callbacks_;
            memcpy(&working_set, &master_fd_set_, sizeof(master_fd_set_));
        }

        // process multiplexer I/O
        first_file_descriptor = callbacks_snapshot.begin()->first;
        max_file_descriptor = callbacks_snapshot.rbegin()->first + 1;

        select_result = select(max_file_descriptor, &working_set, NULL, NULL, &timeout);

        if (select_result < 0) {
            // TODO: create exception
            LGSOMEIP_LOG_ERROR << "Multiplexer::run / select failed / " << strerror(errno);
        } else if (select_result > 0) {
            descriptors_ready = select_result;
            for (int i = first_file_descriptor; i < max_file_descriptor && descriptors_ready > 0; i++) {
                if (FD_ISSET(i, &working_set)) {
                    auto callback = callbacks_snapshot.find(i);
                    if (callback != callbacks_snapshot.end() && callback->second != nullptr) {
                        callback->second(false);
                    } else {
                        LGSOMEIP_LOG_DEBUG << "Multiplexer::run / callbacks_[" << i << "] is nullptr";
                    }
                    descriptors_ready--;
                }
            }
        } else {
            std::function<void()> timeout_callback;
            {
                std::lock_guard<std::mutex> lock(callbacks_mutex_);
                timeout_callback = timeout_callback_;
            }
            if (timeout_callback != nullptr) {
                timeout_callback();
            }
        }
    } // End while()
}

void Multiplexer::unset(int file_descriptor) {
    if (file_descriptor == -1) {
        LGSOMEIP_LOG_WARN << "Multiplexer::unset / FD : " << file_descriptor;
        return;
    }

    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    FD_CLR(file_descriptor, &master_fd_set_);
    callbacks_.erase(file_descriptor);
}

void Multiplexer::set(int file_descriptor, std::function<void(bool)> callback) {
    if (file_descriptor == -1) {
        LGSOMEIP_LOG_ERROR << "Multiplexer::set EPOLL_CTL_ADD fail for socket / FD : " << file_descriptor;
        return;
    }

    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    callbacks_.insert(std::make_pair(file_descriptor, callback));
    FD_SET(file_descriptor, &master_fd_set_);

    LGSOMEIP_LOG_DEBUG << "Multiplexer::set / FD : " << file_descriptor << " is set";

    // thread wakeup
    if (callbacks_.size() == 1) {
        condition_.notify_one();
    }
}

void Multiplexer::set_timeout(std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    timeout_callback_ = callback;
}

} // namespace osabstraction
} // namespace lgsomeip
