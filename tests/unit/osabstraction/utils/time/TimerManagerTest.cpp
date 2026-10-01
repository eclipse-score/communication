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

#include <gtest/gtest.h>

#include <utils/time/TimerManager.h>

using namespace lgsomeip;

TEST(TimerManager, fires_one_shot_timer_once) {
    auto& manager = TimerManager::get();
    int callback_count = 0;
    const auto timer_id = manager.start_one_shot_timer(std::chrono::milliseconds(0),
                                                       [&callback_count](TimerManager::TimerId) { ++callback_count; });

    manager.iterate_loop();
    EXPECT_EQ(callback_count, 1);
    manager.kill_timer(timer_id);
}

TEST(TimerManager, repeating_timer_can_be_killed) {
    auto& manager = TimerManager::get();
    int callback_count = 0;
    const auto timer_id = manager.start_timer(std::chrono::milliseconds(0),
                                              [&callback_count](TimerManager::TimerId) { ++callback_count; });

    manager.iterate_loop();
    ASSERT_EQ(callback_count, 1);
    manager.kill_timer(timer_id);
    manager.iterate_loop();

    EXPECT_EQ(callback_count, 1);
}
