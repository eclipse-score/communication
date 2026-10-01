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
#include <sys/file.h>
#include <thread>
#include <unistd.h>

#include <utils/time/TimerManager.h>

namespace lgsomeip {
namespace osabstraction {

// If LINUX is defined, the Epoll Multiplexer is executed.
Multiplexer::Multiplexer() {
    constexpr char kLockPath[] = "/tmp/someip/multiplexer.lock";
    int lock_fd = ::open(kLockPath, O_RDONLY | O_CREAT, 0600);
    // Just try to do best-effort. If there are error, just try to create epoll
    if (lock_fd != -1) {
        ::flock(lock_fd, LOCK_EX);
    }
    epoll_fd_ = epoll_create(LSAR_EPOLL_SIZE);
    if (lock_fd != -1) {
        ::flock(lock_fd, LOCK_UN);
        ::close(lock_fd);
    }

    // wakeup condition fd
    condition_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (condition_fd_ < 0) {
        LGSOMEIP_LOG_WARN << "Multiplexer::Multiplexer() / Create eventfd fail.";
        condition_fd_ = -1;
    } else {
        LGSOMEIP_LOG_DEBUG
            << "Multiplexer::Multiplexer() / Create event FD to be used to stop the multiplex thread, FD: "
            << condition_fd_;
        set(condition_fd_, nullptr);
    }
}

Multiplexer::~Multiplexer() {
    stop();
    join();

    if (condition_fd_ != -1) {
        ::close(condition_fd_);
        condition_fd_ = -1;
    }
    if (epoll_fd_ != -1) {
        ::close(epoll_fd_);
        epoll_fd_ = -1;
    }
}

void Multiplexer::start() {
    running_ = true;
    thread_ = std::make_shared<std::thread>(&Multiplexer::run, this);
    // For assigning thread name
    pthread_setname_np(thread_->native_handle(), "SomeipEpollMux");
}

void Multiplexer::stop() {
    if (running_.load()) {
        running_.store(false);
        // Wakeup notify
        if (condition_fd_ != -1) {
            eventfd_write(condition_fd_, 1);
        }
    }
}

void Multiplexer::join() {
    if (thread_ != nullptr && thread_->joinable()) {
        thread_->join();
    }
}

void Multiplexer::run() {
    int event_count;
    auto get_callback = [this](int file_descriptor) {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        auto callback = callbacks_.find(file_descriptor);
        if (callback == callbacks_.end()) {
            return std::function<void(bool)>();
        }
        return callback->second;
    };

    while (running_.load()) {
        event_count = epoll_wait(epoll_fd_, epoll_events_, LSAR_EPOLL_SIZE, 1000);

        for (int index = 0; index < event_count; index++) {
#if defined(EPOLLRDHUP)
            if (epoll_events_[index].events & EPOLLRDHUP) {
                const int file_descriptor = epoll_events_[index].data.fd;
                LGSOMEIP_LOG_DEBUG << "Multiplexer::run / EPOLLRDHUP fd : " << file_descriptor;
                auto callback = get_callback(file_descriptor);
                if (callback != nullptr) {
                    callback(true); // disconnect
                }
                unset(file_descriptor);
            }
#else
            if (epoll_events_[index].events & EPOLLHUP) {
                const int file_descriptor = epoll_events_[index].data.fd;
                LGSOMEIP_LOG_DEBUG << "Multiplexer::run / EPOLLHUP fd : " << file_descriptor;
                auto callback = get_callback(file_descriptor);
                if (callback != nullptr) {
                    callback(true); // disconnect
                }
                unset(file_descriptor);
            }
#endif // EPOLLRDHUP
            else if (epoll_events_[index].events & EPOLLERR) {
                const int file_descriptor = epoll_events_[index].data.fd;
                LGSOMEIP_LOG_DEBUG << "Multiplexer::run / EPOLLERR fd : " << file_descriptor;
                auto callback = get_callback(file_descriptor);
                if (callback != nullptr) {
                    callback(true); // disconnect
                }
                unset(file_descriptor);
            } else {
                const int file_descriptor = epoll_events_[index].data.fd;
                auto callback = get_callback(file_descriptor);
                if ((epoll_events_[index].events & EPOLLIN) && callback != nullptr) {
                    callback(false);
                } else {
                    LGSOMEIP_LOG_DEBUG << "Multiplexer::run / callbacks_[" << file_descriptor << "] is nullptr";
                }
            }
        }

        // TimerManager singleton instance may be destroyed before stopping Multiplexer
        if (running_.load()) {
            TimerManager::get().iterate_loop();
        } else {
            // Thread stop
            break;
        }

        if (event_count == -1) {
            LGSOMEIP_LOG_DEBUG << "Multiplexer::run / epoll wait() error #" << errno;
            break;
        }
        if (event_count == 0) {
            std::function<void()> timeout_callback;
            {
                std::lock_guard<std::mutex> lock(callbacks_mutex_);
                timeout_callback = timeout_callback_;
            }
            if (timeout_callback != nullptr) {
                timeout_callback();
            }
            continue;
        }

    } // End while(running_)
}

void Multiplexer::unset(int file_descriptor) {
    if (file_descriptor == -1) {
        LGSOMEIP_LOG_WARN << "Multiplexer::unset / FD : " << file_descriptor;
        return;
    }

    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, file_descriptor, NULL);
    callbacks_.erase(file_descriptor);
}

void Multiplexer::set(int file_descriptor, std::function<void(bool)> callback) {
    if (file_descriptor == -1) {
        LGSOMEIP_LOG_ERROR << "Multiplexer::set EPOLL_CTL_ADD fail for socket / FD : " << file_descriptor;
        return;
    }
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLERR;
#if defined(EPOLLRDHUP)
    event.events |= EPOLLRDHUP;
#else
    // for old libraries
    event.events |= EPOLLHUP;
#endif // EPOLLRDHUP
    event.data.fd = file_descriptor;

    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, file_descriptor, &event) < 0) {
        LGSOMEIP_LOG_ERROR << "Multiplexer::set EPOLL_CTL_ADD fail for socket " << file_descriptor << "with errno #"
                           << errno;
        callbacks_.erase(file_descriptor);
        return;
    }
    callbacks_[file_descriptor] = callback;
}

void Multiplexer::set_timeout(std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    timeout_callback_ = callback;
}

} // namespace osabstraction
} // namespace lgsomeip
