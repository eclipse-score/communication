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

/**
 * @file wait_control_test.cpp
 * @brief What ends a wait and what HandleEvents() reports for it: the timeout, an Unblock(), or a signal.
 */

#include "test_utils.h"

#include <gtest/gtest.h>

#include <csignal>

#include <pthread.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

using namespace io_dispatcher;
using namespace test_utils;

namespace
{

/**
 * @brief A SIGUSR1 handler without SA_RESTART, so the wait underneath it is interrupted rather than resumed.
 * @note Restores the previous disposition, so one case's signal does not reach the next.
 */
class InterruptingHandler final
{
  public:
    InterruptingHandler() noexcept
    {
        struct sigaction action{};
        action.sa_handler = [](int) noexcept {};
        sigemptyset(&action.sa_mask);
        action.sa_flags = 0;
        score::cpp::ignore = ::sigaction(SIGUSR1, &action, &previous_);
    }

    ~InterruptingHandler() noexcept
    {
        score::cpp::ignore = ::sigaction(SIGUSR1, &previous_, nullptr);
    }

    InterruptingHandler(const InterruptingHandler&) = delete;
    InterruptingHandler& operator=(const InterruptingHandler&) = delete;
    InterruptingHandler(InterruptingHandler&&) = delete;
    InterruptingHandler& operator=(InterruptingHandler&&) = delete;

  private:
    struct sigaction previous_{};
};

/**
 * @brief How long a waker thread sleeps before releasing the wait under test.
 */
constexpr std::chrono::milliseconds kWakeUpDelay{200};

/**
 * @brief Scheduling slack allowed on a measured wait.
 */
constexpr std::chrono::milliseconds kSlack{30};

/**
 * @brief Millisecond counts no backend can hand to its wait unchanged.
 *
 * Each breaks a different conversion: milliseconds::max() and 18'446'744'073'710 overflow a nanosecond product,
 * the second of them wrapping it to 448'384 ns, and 4'294'967'296 truncates to zero when narrowed to 32 bits.
 */
constexpr std::chrono::milliseconds::rep kTimeoutsTooLargeToConvert[]{
    std::chrono::milliseconds::max().count(),
    18'446'744'073'710,
    4'294'967'296,
};

}  // namespace

TEST(WaitControlTest, AZeroTimeoutReturnsAtOnce)
{
    const auto reactor = MakeReactor(4U);
    ASSERT_NE(reactor, nullptr);

    const auto start = Clock::now();
    const auto result = reactor->HandleEvents(std::chrono::milliseconds{0});
    const auto elapsed = Clock::now() - start;

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), UnblockReason::kTimeout);
    EXPECT_LT(elapsed, std::chrono::milliseconds{500}) << "a zero timeout waited";
}

TEST(WaitControlTest, ANegativeTimeoutIsClampedToNoWaitAtAll)
{
    const auto reactor = MakeReactor(4U);
    ASSERT_NE(reactor, nullptr);

    const auto start = Clock::now();
    const auto result = reactor->HandleEvents(std::chrono::milliseconds{-5});
    const auto elapsed = Clock::now() - start;

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), UnblockReason::kTimeout);
    EXPECT_LT(elapsed, std::chrono::milliseconds{500}) << "a negative timeout was waited on rather than clamped";
}

class LongWaitTest : public ::testing::TestWithParam<std::chrono::milliseconds::rep>
{
};

TEST_P(LongWaitTest, ATimeoutTooLargeToConvertIsClampedRatherThanWrapped)
{
    const std::chrono::milliseconds requested{GetParam()};

    const auto reactor = MakeReactor(4U);
    ASSERT_NE(reactor, nullptr);

    std::thread waker([&reactor]() {
        std::this_thread::sleep_for(kWakeUpDelay);
        reactor->Unblock();
    });

    const auto start = Clock::now();
    const auto result = reactor->HandleEvents(requested);
    const auto elapsed = Clock::now() - start;
    waker.join();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), UnblockReason::kEventsHandledOrUnblock)
        << "a wait of " << requested.count() << " ms expired on its own";
    EXPECT_GE(elapsed, kWakeUpDelay - kSlack) << "the wait ended far short of the value it was given";
    EXPECT_LT(elapsed, std::chrono::seconds{5}) << "the long wait was never released";
}

INSTANTIATE_TEST_SUITE_P(EveryTimeoutTooLargeToConvert,
                         LongWaitTest,
                         ::testing::ValuesIn(kTimeoutsTooLargeToConvert),
                         [](const ::testing::TestParamInfo<std::chrono::milliseconds::rep>& param_info) {
                             return "Ms" + std::to_string(param_info.param);
                         });

TEST(WaitControlTest, AnUnblockBeforeTheWaitIsRemembered)
{
    const auto reactor = MakeReactor(4U);
    ASSERT_NE(reactor, nullptr);

    reactor->Unblock();

    const auto start = Clock::now();
    const auto result = reactor->HandleEvents(std::chrono::seconds{5});
    const auto elapsed = Clock::now() - start;

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), UnblockReason::kEventsHandledOrUnblock);
    EXPECT_LT(elapsed, std::chrono::seconds{2}) << "an unblock raised before the wait was lost";
}

TEST(WaitControlTest, RepeatedUnblocksCostOneWakeUp)
{
    const auto reactor = MakeReactor(4U);
    ASSERT_NE(reactor, nullptr);

    for (int i = 0; i < 50; ++i)
    {
        reactor->Unblock();
    }

    const auto first = reactor->HandleEvents(std::chrono::milliseconds{100});
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first.value(), UnblockReason::kEventsHandledOrUnblock);

    const auto second = reactor->HandleEvents(std::chrono::milliseconds{100});
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second.value(), UnblockReason::kTimeout) << "a burst of unblocks is still being paid off";
}

TEST(WaitControlTest, AnUnblockRaisedByACallbackEndsTheFollowingWait)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    SocketPair pair{};
    ASSERT_TRUE(pair.Open());

    const int read_fd{pair.ReadFd()};
    std::atomic_bool fired{false};
    ASSERT_TRUE(reactor
                    ->Register(read_fd,
                               ReadEvent(),
                               [&reactor, &fired, read_fd](EventSourceHandle, EventTypes) noexcept {
                                   DrainByte(read_fd);
                                   reactor->Unblock();
                                   fired = true;
                               })
                    .has_value());

    PokeByte(pair.WriteFd());
    ASSERT_TRUE(PumpUntil(*reactor, fired, 4));

    const auto start = Clock::now();
    const auto result = reactor->HandleEvents(std::chrono::seconds{5});
    const auto elapsed = Clock::now() - start;

    ASSERT_TRUE(result.has_value());
    EXPECT_LT(elapsed, std::chrono::seconds{2}) << "the unblock a callback raised never reached the following wait";
}

TEST(WaitControlTest, AReadySourceIsServedEvenWithAnUnblockPending)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    SocketPair pair{};
    ASSERT_TRUE(pair.Open());

    std::atomic_bool fired{false};
    ASSERT_TRUE(reactor
                    ->Register(pair.ReadFd(),
                               ReadEvent(),
                               [&fired](EventSourceHandle, EventTypes) noexcept {
                                   fired = true;
                               })
                    .has_value());

    PokeByte(pair.WriteFd());
    reactor->Unblock();

    EXPECT_TRUE(PumpUntil(*reactor, fired, 4)) << "a pending unblock swallowed a readiness";
}

TEST(WaitControlTest, TheTimeoutFollowsTheRequestedExpiry)
{
    constexpr std::chrono::milliseconds kRequested{150};
    const auto reactor = MakeReactor(4U);
    ASSERT_NE(reactor, nullptr);

    const auto start = Clock::now();
    const auto result = reactor->HandleEvents(kRequested);
    const auto elapsed = Clock::now() - start;

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), UnblockReason::kTimeout);
    EXPECT_GE(elapsed, kRequested - kSlack) << "the wait returned well before its expiry";
    EXPECT_LT(elapsed, std::chrono::seconds{2}) << "the wait ran far past its expiry";
}

TEST(WaitControlTest, AnInterruptedWaitIsReportedAsASignal)
{
    const InterruptingHandler handler{};

    const auto reactor = MakeReactor(4U);
    ASSERT_NE(reactor, nullptr);

    // Nothing can observe the wait being entered, so the signal repeats.
    // One that arrives too early is not the only one.
    const pthread_t waiter{::pthread_self()};
    std::atomic_bool waiting{true};
    std::thread interrupter([waiter, &waiting]() {
        while (waiting.load())
        {
            score::cpp::ignore = ::pthread_kill(waiter, SIGUSR1);
            std::this_thread::sleep_for(std::chrono::milliseconds{20});
        }
    });

    const auto result = reactor->HandleEvents(std::chrono::seconds{5});
    waiting.store(false);
    interrupter.join();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), UnblockReason::kSignal) << "an interrupted wait was reported as something else";
}
