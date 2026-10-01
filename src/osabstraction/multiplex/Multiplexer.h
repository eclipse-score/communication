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

#ifndef LG_SOMEIP_OSABSTRACTION_MULTIPLEXER_MULTIPLEXER
#define LG_SOMEIP_OSABSTRACTION_MULTIPLEXER_MULTIPLEXER

#include <condition_variable>
#include <atomic>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <thread>

#include <utils/log/logger.h>

namespace lgsomeip {
namespace osabstraction {

#if defined(LINUX)
#include <sys/epoll.h>
#include <sys/eventfd.h>
#define LSAR_EPOLL_SIZE 1024
#elif defined(QNX)
#include <sys/select.h>
#endif // LINUX

class Multiplexer {
public:
    Multiplexer();
    virtual ~Multiplexer();

    void start();
    void stop();
    void join();

    void set(int file_descriptor, std::function<void(bool)> callback);
    void set_timeout(std::function<void()> callback);
    void unset(int file_descriptor);
    void run();

private:
#if defined(LINUX)
    struct epoll_event epoll_events_[LSAR_EPOLL_SIZE] = {
        0,
    };
    int epoll_fd_;
    int condition_fd_; // wakeup notify
#else
    fd_set master_fd_set_;
#endif // LINUX

    std::map<int, std::function<void(bool)>> callbacks_;
    std::mutex callbacks_mutex_;

    std::atomic_bool running_{true};

    std::shared_ptr<std::thread> thread_;
    std::condition_variable condition_;

    std::function<void()> timeout_callback_{nullptr};
};

} // namespace osabstraction
} // namespace lgsomeip

#endif // LG_SOMEIP_OSABSTRACTION_MULTIPLEXER_MULTIPLEXER
