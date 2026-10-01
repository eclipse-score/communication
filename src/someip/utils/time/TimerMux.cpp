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

#include <chrono>
#include <functional>
#include <typeinfo>

#include "TimerMux.h"

#include <socket/LocalAddress.h>
#include <socket/UDPSocket.h>

namespace lgsomeip {

TimerMux::TimerMux(std::string name) {
    // TODO : Make Unique File Name;
    file_name_ += name;

    // Timer init
    timer_init();

    // TODO : Make & Open Socket File;
    std::shared_ptr<lgsomeip::osabstraction::Address> address =
        std::make_shared<lgsomeip::osabstraction::LocalAddress>();
    address->set_reliable(false);
    address->set_file_path(file_name_.c_str());

    socket_ = std::make_shared<lgsomeip::osabstraction::UDPSocket>(address);
    socket_->bind();
}

TimerMux::~TimerMux() {
    // Thread Stop & Join
    timer_state_ = false;
    condition_.notify_one();
    try {
        thread_->join();
    } catch (...) {
        // do nothing
    }

    // Multiplexer stop Listen
    stop_listen();
}

void TimerMux::set_timer(std::uint32_t interval_milliseconds, bool periodic) {
    if (socket_ != 0) {
        periodic_ = periodic;
        timer_interval_ = interval_milliseconds;
        condition_.notify_one();
    } else {
        // TODO : Error Report
    }
}

void TimerMux::cancel_timer() {
    timer_interval_ = 0;
    periodic_ = false;
}

std::shared_ptr<lgsomeip::osabstraction::Multiplexer> TimerMux::get_multiplexer() const {
    return multiplexer_;
}

void TimerMux::start_listen(std::shared_ptr<lgsomeip::osabstraction::Multiplexer> multiplexer) {
    if (multiplexer != nullptr) {
        multiplexer_ = multiplexer;
        multiplexer_->set(socket_->get_socket_fd(), std::bind(&TimerMux::callback, this));
    } else {
        // TODO : throw Exception
    }
}

void TimerMux::stop_listen() {
    if (multiplexer_ != nullptr) {
        multiplexer_->unset(socket_->get_socket_fd());
        multiplexer_ = nullptr;
    } else {
        // TODO : throw Exception
    }
}

void TimerMux::register_handler(std::function<void(void)> handler) {
    handler_ = handler;
}

void TimerMux::unregister_handler() {
    handler_ = nullptr;
}

void TimerMux::callback() {
    char receive_buffer[10];
    socket_->receive(receive_buffer, sizeof(receive_buffer), nullptr); // UDP receive function call
    if (handler_ != nullptr) {
        handler_();
        if (periodic_) {
            set_timer(timer_interval_, periodic_);
        }
    }
}

void TimerMux::timer_init() {
    timer_state_ = true;
    thread_ = std::make_shared<std::thread>(&TimerMux::run_timer, this);
    // For assigning thread name
    pthread_setname_np(thread_->native_handle(), "SomeipTimer");
}

void TimerMux::run_timer() {
    const char timer_message[] = "on";
    std::chrono::steady_clock::time_point next(std::chrono::steady_clock::now());

    while (1) {
        std::unique_lock<std::mutex> lock(thread_mutex_);
        condition_.wait(lock, [&] { return timer_interval_ > 0 || !timer_state_; });
        if (!timer_state_)
            break;

        //        std::chrono::duration<int, std::milli> msec(timer_interval_);
        //        std::this_thread::sleep_for(msec);
        std::chrono::steady_clock::duration duration(std::chrono::milliseconds{timer_interval_});
        next += duration;
        std::this_thread::sleep_until(next);

        socket_->send(timer_message, sizeof(timer_message), socket_->get_src_address());
        if (periodic_ == false) {
            cancel_timer();
        }
    }
}

} // namespace lgsomeip
