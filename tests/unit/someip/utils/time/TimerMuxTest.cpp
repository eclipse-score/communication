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
#include <gtest/gtest.h>
#include <memory>

#include <endpoint/EndpointConstant.h>
#include <multiplex/Multiplexer.h>
#include <utils/time/TimerMux.h>

using namespace lgsomeip;
using namespace lgsomeip::osabstraction;

static int timer_expired = 0;
void on_timer() {
    timer_expired++;
}

TEST(TimerMux, OneShotTest) {
    timer_expired = 0;
    std::shared_ptr<Multiplexer> mux = std::make_shared<Multiplexer>();
    mux->start();

    TimerMux timer("Test");
    timer.register_handler(std::bind(on_timer));
    timer.start_listen(mux);
    timer.set_timer(1000, false);

    std::chrono::duration<int, std::milli> msec1(200);
    std::this_thread::sleep_for(msec1);

    ASSERT_EQ(0, timer_expired);

    std::chrono::duration<int, std::milli> msec2(1000);
    std::this_thread::sleep_for(msec2);
    ASSERT_EQ(1, timer_expired);

    mux->stop();
}

TEST(TimerMux, PeriodicTimerTest) {
    timer_expired = 0;
    std::shared_ptr<Multiplexer> mux = std::make_shared<Multiplexer>();
    mux->start();

    TimerMux timer("Test");
    timer.register_handler(std::bind(on_timer));
    timer.start_listen(mux);
    timer.set_timer(500, true);

    std::chrono::duration<int, std::milli> msec1(3000);
    std::this_thread::sleep_for(msec1);

    timer.cancel_timer();
    ASSERT_EQ(true, timer_expired > 4);

    mux->stop();
}
