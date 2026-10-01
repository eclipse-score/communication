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

#ifndef LG_SOMEIP_UTILS_TIME_TIME_CHECK_H
#define LG_SOMEIP_UTILS_TIME_TIME_CHECK_H

#include <chrono>
#include <ctime>
#include <iostream>

namespace lgsomeip {

class TimeCheck {
public:
    void start() {
        start_time_ = std::chrono::steady_clock::now();
    }

    void end() {
        end_time_ = std::chrono::steady_clock::now();
    }

    double get_duration() {
        std::chrono::duration<double> diff = end_time_ - start_time_;
        return diff.count();
    }

private:
    std::chrono::time_point<std::chrono::steady_clock> start_time_;
    std::chrono::time_point<std::chrono::steady_clock> end_time_;
};

} // namespace lgsomeip

#endif // LG_SOMEIP_UTILS_TIME_TIME_CHECK_H
