/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/
#include "score/mw/com/impl/e2e/e2e_reception_timeout_monitor.h"

#include <gtest/gtest.h>

#include <chrono>

namespace score::mw::com::impl::e2e
{
namespace
{

using std::chrono_literals::operator""ms;
using std::chrono_literals::operator""h;

constexpr std::chrono::milliseconds kTimeout{10ms};
constexpr ReceptionTimeoutMonitor::Clock::time_point kStart{ReceptionTimeoutMonitor::Clock::time_point{} + 1h};

TEST(E2eReceptionTimeoutMonitorTest, ZeroTimeoutDisablesTheMonitor)
{
    // Given a monitor with a reception timeout of 0 which has seen activity
    ReceptionTimeoutMonitor monitor{std::chrono::milliseconds{0}};
    monitor.OnActivity(kStart);

    // When consuming missed periods much later
    const auto missed = monitor.ConsumeMissedPeriods(kStart + 1h);

    // Then the monitor is disabled and reports no missed period
    EXPECT_FALSE(monitor.IsEnabled());
    EXPECT_EQ(missed, 0U);
}

TEST(E2eReceptionTimeoutMonitorTest, NoMissedPeriodsBeforeFirstActivity)
{
    // Given an enabled monitor which has not seen any activity
    ReceptionTimeoutMonitor monitor{kTimeout};

    // When consuming missed periods much later
    const auto missed = monitor.ConsumeMissedPeriods(kStart + 1h);

    // Then no missed period is reported, as the monitor is not armed yet
    EXPECT_TRUE(monitor.IsEnabled());
    EXPECT_EQ(missed, 0U);
}

TEST(E2eReceptionTimeoutMonitorTest, NoMissedPeriodBeforeTimeoutElapsed)
{
    // Given a monitor which has seen activity
    ReceptionTimeoutMonitor monitor{kTimeout};
    monitor.OnActivity(kStart);

    // When consuming missed periods just before the timeout elapsed
    const auto missed = monitor.ConsumeMissedPeriods(kStart + kTimeout - std::chrono::nanoseconds{1});

    // Then no missed period is reported
    EXPECT_EQ(missed, 0U);
}

TEST(E2eReceptionTimeoutMonitorTest, OneMissedPeriodWhenTimeoutElapsed)
{
    // Given a monitor which has seen activity
    ReceptionTimeoutMonitor monitor{kTimeout};
    monitor.OnActivity(kStart);

    // When consuming missed periods exactly when the timeout elapsed
    const auto missed = monitor.ConsumeMissedPeriods(kStart + kTimeout);

    // Then one missed period is reported
    EXPECT_EQ(missed, 1U);
}

TEST(E2eReceptionTimeoutMonitorTest, PeriodsAreReportedOnlyOnceAndPartialPeriodsCarryOver)
{
    // Given a monitor which has seen activity
    ReceptionTimeoutMonitor monitor{kTimeout};
    monitor.OnActivity(kStart);

    // When consuming missed periods after 3.5 timeouts
    // Then 3 missed periods are reported
    EXPECT_EQ(monitor.ConsumeMissedPeriods(kStart + 35ms), 3U);

    // When consuming again at the same time
    // Then nothing new is reported
    EXPECT_EQ(monitor.ConsumeMissedPeriods(kStart + 35ms), 0U);

    // When consuming after 4 timeouts
    // Then the half period left over from before completes one more missed period
    EXPECT_EQ(monitor.ConsumeMissedPeriods(kStart + 40ms), 1U);
}

TEST(E2eReceptionTimeoutMonitorTest, ActivityRestartsThePeriod)
{
    // Given a monitor which has seen activity and then activity again shortly before the timeout elapsed
    ReceptionTimeoutMonitor monitor{kTimeout};
    monitor.OnActivity(kStart);
    monitor.OnActivity(kStart + 9ms);

    // When consuming missed periods one timeout after the first activity
    // Then no missed period is reported
    EXPECT_EQ(monitor.ConsumeMissedPeriods(kStart + kTimeout), 0U);

    // When consuming missed periods one timeout after the second activity
    // Then one missed period is reported
    EXPECT_EQ(monitor.ConsumeMissedPeriods(kStart + 9ms + kTimeout), 1U);
}

TEST(E2eReceptionTimeoutMonitorTest, MissedPeriodsAreCappedAtMaximumWindowSize)
{
    // Given a monitor which has seen activity
    ReceptionTimeoutMonitor monitor{kTimeout};
    monitor.OnActivity(kStart);

    // When consuming missed periods much later
    const auto missed = monitor.ConsumeMissedPeriods(kStart + 1h);

    // Then the result is capped at the maximum window size
    EXPECT_EQ(missed, kMaxHealthWindowSize);
}

TEST(E2eReceptionTimeoutMonitorTest, TimeBeforeLastActivityReportsNoMissedPeriod)
{
    // Given a monitor which has seen activity
    ReceptionTimeoutMonitor monitor{kTimeout};
    monitor.OnActivity(kStart);

    // When consuming missed periods for a time before that activity
    const auto missed = monitor.ConsumeMissedPeriods(kStart - 1h);

    // Then no missed period is reported
    EXPECT_EQ(missed, 0U);
}

TEST(E2eReceptionTimeoutMonitorTest, ResetDisarmsUntilNextActivity)
{
    // Given a monitor which has seen activity and was reset
    ReceptionTimeoutMonitor monitor{kTimeout};
    monitor.OnActivity(kStart);
    monitor.Reset();

    // When consuming missed periods much later
    // Then no missed period is reported
    EXPECT_EQ(monitor.ConsumeMissedPeriods(kStart + 1h), 0U);

    // When activity is seen again and the timeout elapses
    monitor.OnActivity(kStart + 1h);

    // Then missed periods are counted from the new activity
    EXPECT_EQ(monitor.ConsumeMissedPeriods(kStart + 1h + kTimeout), 1U);
}

}  // namespace
}  // namespace score::mw::com::impl::e2e
