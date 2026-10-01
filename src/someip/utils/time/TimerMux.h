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

#ifndef LG_SOMEIP_UTILS_TIME_TIMER_MUX_H
#define LG_SOMEIP_UTILS_TIME_TIMER_MUX_H

#include <cstdint>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <string>
#include <thread>

#include <endpoint/Endpoint.h>
#include <endpoint/EndpointConstant.h>
#include <multiplex/Multiplexer.h>

#include <socket/LocalAddress.h>
#include <socket/UDPSocket.h>

namespace lgsomeip {

class TimerMux {
public:
    TimerMux(std::string name);
    virtual ~TimerMux();

    void set_timer(std::uint32_t interval_milliseconds, bool periodic = false);
    void cancel_timer();

    std::shared_ptr<lgsomeip::osabstraction::Multiplexer> get_multiplexer() const;
    void start_listen(std::shared_ptr<lgsomeip::osabstraction::Multiplexer> multiplexer = nullptr);
    void stop_listen();

    // Set Callback
    void register_handler(std::function<void(void)> handler);
    void unregister_handler();

protected:
    void timer_init();
    void callback();

private:
    std::shared_ptr<lgsomeip::osabstraction::Multiplexer> multiplexer_ = nullptr;
    std::atomic<bool> timer_state_{true};
    std::function<void(void)> handler_{nullptr};
    std::string file_name_{SOMEIP_SOCKET_PATH};
    std::shared_ptr<lgsomeip::osabstraction::UDPSocket> socket_{nullptr};
    std::uint32_t timer_interval_{0};
    bool periodic_ = false;

private:
    void run_timer();
    std::shared_ptr<std::thread> thread_{nullptr};
    std::condition_variable condition_;
    std::mutex thread_mutex_;
};

} // namespace lgsomeip

#endif // LG_SOMEIP_UTILS_TIME_TIMER_MUX_H
