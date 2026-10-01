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

#include <utils/time/TimeCheck.h>

using namespace lgsomeip;

TEST(TimeCheck, measures_non_negative_duration) {
    TimeCheck timer;

    timer.start();
    timer.end();

    EXPECT_GE(timer.get_duration(), 0.0);
}

TEST(TimeCheck, can_restart_measurement) {
    TimeCheck timer;

    timer.start();
    timer.end();
    const double first_duration = timer.get_duration();

    timer.start();
    timer.end();

    EXPECT_GE(timer.get_duration(), 0.0);
    EXPECT_GE(first_duration, 0.0);
}
