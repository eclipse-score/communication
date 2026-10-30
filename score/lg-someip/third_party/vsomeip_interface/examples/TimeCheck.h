// Copyright (C) 2017-2026 LG Electronics Inc.
// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined(__UTILS_TIME_TIME_CHECK_H__)
#define __UTILS_TIME_TIME_CHECK_H__

#include <chrono>
#include <ctime>
#include <iostream>

class TimeCheck {
public:
    void start()
    {
        st = std::chrono::steady_clock::now();
    }

    void end()
    {
        en = std::chrono::steady_clock::now();
    }

    double getDuration()
    {
        std::chrono::duration<double> diff = en-st;
        return diff.count(); // std::chrono::duration_cast<std::chrono::microseconds>(en - st);
    }

private:
    std::chrono::time_point<std::chrono::steady_clock> st;
    std::chrono::time_point<std::chrono::steady_clock> en;
};

#endif // !defined(__UTILS_TIME_TIME_CHECK_H__)
