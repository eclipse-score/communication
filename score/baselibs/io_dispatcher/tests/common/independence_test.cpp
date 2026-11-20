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
 * @file independence_test.cpp
 * @brief Reactors in one process share nothing: each has its own wake-up channel, sources and handles.
 *
 * The wake-up channel is the self-pipe on Linux and the private channel with its pulse connection on QNX.
 * A channel shared or crossed between two reactors shows up as an Unblock() that wakes the wrong one.
 */

#include "score/io_dispatcher/io_dispatcher_error.h"
#include "test_utils.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

using namespace io_dispatcher;
using namespace test_utils;

namespace
{

/**
 * @brief Length of a wait that only a wake-up meant for its own reactor may cut short.
 */
constexpr std::chrono::milliseconds kQuietWait{400};

/**
 * @brief How long the waker sleeps, so the other reactor is blocked in its wait before the Unblock().
 */
constexpr std::chrono::milliseconds kWakeUpDelay{50};

/**
 * @brief Scheduling slack allowed on a measured wait.
 */
constexpr std::chrono::milliseconds kSlack{30};

/**
 * @brief Wait used when probing several reactors one after the other.
 */
constexpr std::chrono::milliseconds kProbeWait{10};

/**
 * @brief Blocks `sleeper` in a wait, unblocks `waker`, and checks that only `waker` returns early.
 * @pre Both reactors have no wake-up pending.
 */
void ExpectUnblockWakesOnly(IoDispatcher& waker, IoDispatcher& sleeper)
{
    std::atomic<bool> sleeper_ok{false};
    UnblockReason sleeper_reason{UnblockReason::kEventsHandledOrUnblock};
    Clock::duration sleeper_waited{};
    std::thread sleeper_thread{[&sleeper, &sleeper_ok, &sleeper_reason, &sleeper_waited]() {
        const auto start = Clock::now();
        const auto result = sleeper.HandleEvents(kQuietWait);
        sleeper_waited = Clock::now() - start;
        if (result.has_value())
        {
            sleeper_reason = result.value();
            sleeper_ok.store(true);
        }
    }};

    std::this_thread::sleep_for(kWakeUpDelay);
    waker.Unblock();
    const auto start = Clock::now();
    const auto woken = waker.HandleEvents(kQuietWait);
    const auto waker_waited = Clock::now() - start;
    sleeper_thread.join();

    ASSERT_TRUE(woken.has_value());
    EXPECT_EQ(woken.value(), UnblockReason::kEventsHandledOrUnblock);
    EXPECT_LT(waker_waited, kQuietWait - kSlack);
    ASSERT_TRUE(sleeper_ok.load());
    EXPECT_EQ(sleeper_reason, UnblockReason::kTimeout);
    EXPECT_GE(sleeper_waited, kQuietWait - kSlack);
}

}  // namespace

TEST(IndependenceTest, AnUnblockWakesOnlyTheReactorItIsCalledOn)
{
    const auto first = MakeReactor(4U);
    const auto second = MakeReactor(4U);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    DrainWakeup(*first);
    DrainWakeup(*second);

    ExpectUnblockWakesOnly(*first, *second);
    ExpectUnblockWakesOnly(*second, *first);
}

TEST(IndependenceTest, EightReactorsCreatedAtOnceEachGetAWakeUpChannelOfTheirOwn)
{
    constexpr std::size_t kReactors{static_cast<std::size_t>(kMaxDispatcherTag) + 1U};
    std::vector<std::unique_ptr<IoDispatcher>> reactors(kReactors);
    {
        // Created from parallel threads, so channel creation in one cannot lean on another having finished.
        std::vector<std::thread> creators{};
        for (std::size_t index{0U}; index < kReactors; ++index)
        {
            creators.emplace_back([&reactors, index]() {
                reactors[index] = MakeReactor(4U, static_cast<DispatcherTag>(index));
            });
        }

        for (auto& creator : creators)
        {
            creator.join();
        }
    }

    for (const auto& reactor : reactors)
    {
        ASSERT_NE(reactor, nullptr);
        DrainWakeup(*reactor);
    }

    for (std::size_t woken{0U}; woken < kReactors; ++woken)
    {
        reactors[woken]->Unblock();
        for (std::size_t other{0U}; other < kReactors; ++other)
        {
            if (other == woken)
            {
                continue;
            }

            const auto idle = reactors[other]->HandleEvents(kProbeWait);
            ASSERT_TRUE(idle.has_value());
            EXPECT_EQ(idle.value(), UnblockReason::kTimeout) << "reactor " << other << " woken by " << woken;
        }

        const auto unblocked = reactors[woken]->HandleEvents(kQuietWait);
        ASSERT_TRUE(unblocked.has_value());
        EXPECT_EQ(unblocked.value(), UnblockReason::kEventsHandledOrUnblock) << "reactor " << woken;
    }
}

TEST(IndependenceTest, EachReactorDispatchesOnlyTheSourcesRegisteredOnIt)
{
    const auto first = MakeReactor(4U);
    const auto second = MakeReactor(4U);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    SocketPair first_pair{};
    SocketPair second_pair{};
    ASSERT_TRUE(first_pair.Open());
    ASSERT_TRUE(second_pair.Open());

    std::atomic_bool first_called{false};
    std::atomic_bool second_called{false};
    // Each callback drains its byte, so a source stays readable only until its own reactor dispatches it.
    const auto first_handle =
        first->Register(first_pair.ReadFd(), ReadEvent(), [&first_called, &first_pair](auto, auto) {
            DrainByte(first_pair.ReadFd());
            first_called.store(true);
        });
    const auto second_handle =
        second->Register(second_pair.ReadFd(), ReadEvent(), [&second_called, &second_pair](auto, auto) {
            DrainByte(second_pair.ReadFd());
            second_called.store(true);
        });
    ASSERT_TRUE(first_handle.has_value());
    ASSERT_TRUE(second_handle.has_value());

    PokeByte(first_pair.WriteFd());
    ASSERT_TRUE(PumpPasses(*second, 2, kProbeWait));
    EXPECT_FALSE(first_called.load());
    EXPECT_FALSE(second_called.load());
    EXPECT_TRUE(PumpUntil(*first, first_called, 10));
    EXPECT_FALSE(second_called.load());

    first_called.store(false);
    PokeByte(second_pair.WriteFd());
    ASSERT_TRUE(PumpPasses(*first, 2, kProbeWait));
    EXPECT_FALSE(first_called.load());
    EXPECT_FALSE(second_called.load());
    EXPECT_TRUE(PumpUntil(*second, second_called, 10));
    EXPECT_FALSE(first_called.load());

    first->Unregister(first_handle.value());
    second->Unregister(second_handle.value());
}

TEST(IndependenceTest, DestroyingOneReactorLeavesAnotherBlockedInItsWaitWorking)
{
    auto doomed = MakeReactor(4U);
    const auto survivor = MakeReactor(4U);
    ASSERT_NE(doomed, nullptr);
    ASSERT_NE(survivor, nullptr);
    SocketPair pair{};
    ASSERT_TRUE(pair.Open());

    std::atomic_bool called{false};
    const auto handle = survivor->Register(pair.ReadFd(), ReadEvent(), [&called](auto, auto) {
        called.store(true);
    });
    ASSERT_TRUE(handle.has_value());
    DrainWakeup(*survivor);

    std::atomic<bool> pumped{false};
    std::thread waiter{[&survivor, &called, &pumped]() {
        pumped.store(PumpUntil(*survivor, called, 10));
    }};
    std::this_thread::sleep_for(kWakeUpDelay);
    // Closes the doomed reactor's wake-up channel while the survivor is blocked on its own.
    doomed.reset();
    PokeByte(pair.WriteFd());
    waiter.join();
    EXPECT_TRUE(pumped.load());
    DrainByte(pair.ReadFd());

    std::thread blocked{[&survivor]() {
        const auto result = survivor->HandleEvents(std::chrono::seconds{5});
        EXPECT_TRUE(result.has_value());
    }};
    std::this_thread::sleep_for(kWakeUpDelay);
    const auto start = Clock::now();
    survivor->Unblock();
    blocked.join();
    EXPECT_LT(Clock::now() - start, std::chrono::seconds{1});

    survivor->Unregister(handle.value());
}

TEST(IndependenceTest, AReactorCreatedAfterAnotherIsDestroyedGetsAWakeUpChannelOfItsOwn)
{
    const auto survivor = MakeReactor(4U);
    ASSERT_NE(survivor, nullptr);
    {
        const auto doomed = MakeReactor(4U);
        ASSERT_NE(doomed, nullptr);
    }

    // The released descriptors or channel ids are the first ones the new reactor can be handed.
    const auto successor = MakeReactor(4U);
    ASSERT_NE(successor, nullptr);
    DrainWakeup(*survivor);
    DrainWakeup(*successor);

    ExpectUnblockWakesOnly(*successor, *survivor);
    ExpectUnblockWakesOnly(*survivor, *successor);
}

TEST(IndependenceTest, AHandleOfOneReactorIsRefusedByAnotherAndLeavesItsRegistrationAlone)
{
    const auto owner = MakeReactor(4U, 1U);
    const auto stranger = MakeReactor(4U, 2U);
    ASSERT_NE(owner, nullptr);
    ASSERT_NE(stranger, nullptr);
    SocketPair owner_pair{};
    SocketPair stranger_pair{};
    ASSERT_TRUE(owner_pair.Open());
    ASSERT_TRUE(stranger_pair.Open());

    std::atomic_bool owner_called{false};
    std::atomic_bool stranger_called{false};
    std::atomic_bool injected_called{false};
    const auto owner_handle = owner->Register(owner_pair.ReadFd(), ReadEvent(), [&owner_called](auto, auto) {
        owner_called.store(true);
    });
    // On Linux the stranger's only registration sits in the same slot as the owner's, so only the tag tells them apart.
    const auto stranger_handle =
        stranger->Register(stranger_pair.ReadFd(), ReadEvent(), [&stranger_called](auto, auto) {
            stranger_called.store(true);
        });
    ASSERT_TRUE(owner_handle.has_value());
    ASSERT_TRUE(stranger_handle.has_value());

    const auto monitored = stranger->SetMonitoredEvents(owner_handle.value(), EmptyEvent());
    ASSERT_FALSE(monitored.has_value());
    EXPECT_EQ(monitored.error(), IoDispatcherErrc::kApiError);
    const auto targeted = stranger->SetCallbackTarget(owner_handle.value(), [&injected_called](auto, auto) {
        injected_called.store(true);
    });
    ASSERT_FALSE(targeted.has_value());
    EXPECT_EQ(targeted.error(), IoDispatcherErrc::kApiError);
    stranger->Unregister(owner_handle.value());

    PokeByte(owner_pair.WriteFd());
    EXPECT_TRUE(PumpUntil(*owner, owner_called, 10)) << "the owner's own callback no longer runs";
    DrainByte(owner_pair.ReadFd());
    // A callback the stranger took for its own slot would run in place of the stranger's.
    PokeByte(stranger_pair.WriteFd());
    EXPECT_TRUE(PumpUntil(*stranger, stranger_called, 10)) << "the stranger's own callback was replaced";
    DrainByte(stranger_pair.ReadFd());
    EXPECT_FALSE(injected_called.load()) << "a callback set through a foreign handle ran";
    EXPECT_TRUE(stranger->SetMonitoredEvents(stranger_handle.value(), ReadEvent()).has_value());

    owner->Unregister(owner_handle.value());
    stranger->Unregister(stranger_handle.value());
}
