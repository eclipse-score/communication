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
 * @file thread_safety_test.cpp
 * @brief Calling the reactor from a thread other than the one driving HandleEvents().
 *
 * A mutator must not wait for a blocked wait, nor for an unrelated callback that is still running.
 * Each case parks the reactor in one of those two states and times the mutator against a budget.
 */

#include "test_utils.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <thread>

using namespace io_dispatcher;
using namespace test_utils;

namespace
{

/**
 * @brief What a mutator gets to finish in, generously over what an uncontended call costs.
 */
constexpr std::chrono::milliseconds kCallBudget{2000};

/**
 * @brief What the pump gets to enter its wait in before a mutator is timed against it.
 */
constexpr std::chrono::milliseconds kEnterWaitDelay{100};

/**
 * @brief What the driving callback gets to reach its hold in.
 */
constexpr std::chrono::seconds kSetupBudget{5};

/**
 * @brief How many sources the churn case keeps registered.
 *
 * Every round changes each source's mask, so the wait that follows a round has this many pending changes to apply.
 */
constexpr std::uint16_t kChurnedSources{32U};

/**
 * @brief How many times the churn case sweeps every source.
 */
constexpr int kChurnRounds{100};

/**
 * @brief Flags for a callback that stays in the reactor until released, so a mutator arrives mid-dispatch.
 */
struct Hold
{
    std::atomic_bool entered{false};
    std::atomic_bool released{false};
};

CallbackType Parking(Hold& hold) noexcept
{
    return [&hold](EventSourceHandle, EventTypes) noexcept {
        hold.entered = true;
        while (!hold.released.load())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
    };
}

}  // namespace

/**
 * @brief A registration must not wait out a wait the reactor is blocked in.
 * @note The delay before the call makes the blocked state likely rather than certain.
 */
TEST(ThreadSafetyTest, ASourceIsRegisteredWhileAWaitIsBlocked)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    SocketPair pair{};
    ASSERT_TRUE(pair.Open());

    ReactorThread pump{*reactor, std::chrono::seconds{5}};
    std::this_thread::sleep_for(kEnterWaitDelay);

    const auto start = Clock::now();
    const auto reg = reactor->Register(pair.ReadFd(), ReadEvent(), [](EventSourceHandle, EventTypes) noexcept {});
    const auto elapsed = Clock::now() - start;
    pump.Stop();

    EXPECT_TRUE(reg.has_value());
    EXPECT_LT(elapsed, kCallBudget) << "the registration waited out the blocked wait";
}

TEST(ThreadSafetyTest, AMaskIsChangedWhileACallbackRuns)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    SocketPair pair{};
    ASSERT_TRUE(pair.Open());

    Hold hold{};
    const auto reg = reactor->Register(pair.ReadFd(), ReadEvent(), Parking(hold));
    ASSERT_TRUE(reg.has_value());

    ReactorThread pump{*reactor};
    PokeByte(pair.WriteFd());
    ASSERT_TRUE(WaitFor(
        [&hold]() {
            return hold.entered.load();
        },
        Clock::now() + kSetupBudget))
        << "the callback never reached its hold";

    const auto start = Clock::now();
    const auto result = reactor->SetMonitoredEvents(reg.value(), EmptyEvent());
    const auto elapsed = Clock::now() - start;

    hold.released = true;
    pump.Stop();

    EXPECT_TRUE(result.has_value());
    EXPECT_LT(elapsed, kCallBudget) << "the mask change waited for the callback it was aimed at";
}

TEST(ThreadSafetyTest, ASourceIsRegisteredWhileACallbackRuns)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    SocketPair holding{};
    SocketPair arriving{};
    ASSERT_TRUE(holding.Open());
    ASSERT_TRUE(arriving.Open());

    Hold hold{};
    ASSERT_TRUE(reactor->Register(holding.ReadFd(), ReadEvent(), Parking(hold)).has_value());

    ReactorThread pump{*reactor};
    PokeByte(holding.WriteFd());
    ASSERT_TRUE(WaitFor(
        [&hold]() {
            return hold.entered.load();
        },
        Clock::now() + kSetupBudget))
        << "the callback never reached its hold";

    const auto start = Clock::now();
    const auto reg = reactor->Register(arriving.ReadFd(), ReadEvent(), [](EventSourceHandle, EventTypes) noexcept {});
    const auto elapsed = Clock::now() - start;

    hold.released = true;
    pump.Stop();

    EXPECT_TRUE(reg.has_value());
    EXPECT_LT(elapsed, kCallBudget) << "the registration waited for an unrelated callback";
}

TEST(ThreadSafetyTest, ASiblingIsUnregisteredWhileACallbackRuns)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    SocketPair holding{};
    SocketPair sibling{};
    ASSERT_TRUE(holding.Open());
    ASSERT_TRUE(sibling.Open());

    Hold hold{};
    ASSERT_TRUE(reactor->Register(holding.ReadFd(), ReadEvent(), Parking(hold)).has_value());
    const auto sibling_reg =
        reactor->Register(sibling.ReadFd(), ReadEvent(), [](EventSourceHandle, EventTypes) noexcept {});
    ASSERT_TRUE(sibling_reg.has_value());

    ReactorThread pump{*reactor};
    PokeByte(holding.WriteFd());
    ASSERT_TRUE(WaitFor(
        [&hold]() {
            return hold.entered.load();
        },
        Clock::now() + kSetupBudget))
        << "the callback never reached its hold";

    const auto start = Clock::now();
    reactor->Unregister(sibling_reg.value());
    const auto elapsed = Clock::now() - start;

    hold.released = true;
    pump.Stop();

    EXPECT_LT(elapsed, kCallBudget) << "the withdrawal waited for an unrelated callback";
}

/**
 * @brief A mutator called just before the reactor enters its wait is not held for the whole timeout.
 *
 * The case fills the stretch between the reactor's last look at pending mutators and its blocking receive with
 * sources to arm, and samples it often.
 * @note Probabilistic: it makes the interleaving likely rather than certain.
 */
TEST(ThreadSafetyTest, RepeatedMaskChangesNeverWaitOutABlockedWait)
{
    const auto reactor = MakeReactor(kChurnedSources);
    ASSERT_NE(reactor, nullptr);

    std::array<SocketPair, kChurnedSources> pairs{};
    std::array<EventSourceHandle, kChurnedSources> handles{};
    for (std::size_t i{0U}; i < pairs.size(); ++i)
    {
        ASSERT_TRUE(pairs[i].Open());
        const auto reg =
            reactor->Register(pairs[i].ReadFd(), ReadEvent(), [](EventSourceHandle, EventTypes) noexcept {});
        ASSERT_TRUE(reg.has_value()) << "source " << i;
        handles[i] = reg.value();
    }

    ReactorThread pump{*reactor, std::chrono::seconds{5}};

    Clock::duration worst_sweep{};
    for (int round{0}; round < kChurnRounds; ++round)
    {
        const auto events = ((round % 2) == 0) ? EmptyEvent() : ReadEvent();

        const auto start = Clock::now();
        for (const auto handle : handles)
        {
            ASSERT_TRUE(reactor->SetMonitoredEvents(handle, events).has_value()) << "round " << round;
        }
        worst_sweep = std::max(worst_sweep, Clock::now() - start);
    }
    pump.Stop();

    EXPECT_LT(worst_sweep, kCallBudget) << "a mask change waited out the wait the reactor was entering";
}
