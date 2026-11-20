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
 * @file reentrancy_test.cpp
 * @brief A running callback calling back into the reactor it is being dispatched by.
 *
 * Every call but HandleEvents() is legal there, so each case makes one from a callback and checks it took effect.
 * @warning A backend that cannot take one of these calls deadlocks the case rather than failing it.
 * The suite then hangs until the test timeout.
 */

#include "test_utils.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>

using namespace io_dispatcher;
using namespace test_utils;

TEST(ReentrancyTest, ACallbackMayRegisterAFurtherSource)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    SocketPair first{};
    SocketPair second{};
    ASSERT_TRUE(first.Open());
    ASSERT_TRUE(second.Open());

    const int first_read_fd{first.ReadFd()};
    const int second_read_fd{second.ReadFd()};
    std::atomic_bool second_registered{false};
    std::atomic_bool second_fired{false};

    ASSERT_TRUE(reactor
                    ->Register(first_read_fd,
                               ReadEvent(),
                               [&](EventSourceHandle, EventTypes) noexcept {
                                   DrainByte(first_read_fd);
                                   second_registered =
                                       reactor
                                           ->Register(second_read_fd,
                                                      ReadEvent(),
                                                      [&second_fired](EventSourceHandle, EventTypes) noexcept {
                                                          second_fired = true;
                                                      })
                                           .has_value();
                               })
                    .has_value());

    PokeByte(first.WriteFd());
    ASSERT_TRUE(PumpUntil(*reactor, second_registered, 4)) << "the first callback never ran";

    PokeByte(second.WriteFd());
    EXPECT_TRUE(PumpUntil(*reactor, second_fired, 4)) << "the source a callback registered never dispatched";
}

TEST(ReentrancyTest, ACallbackMayChangeItsOwnMask)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    SocketPair pair{};
    ASSERT_TRUE(pair.Open());

    std::atomic<std::uint32_t> invocations{0U};
    std::atomic_bool paused{false};

    // The datum stays unread, so only the emptied mask can stop the source being reported again.
    ASSERT_TRUE(reactor
                    ->Register(pair.ReadFd(),
                               ReadEvent(),
                               [&](const EventSourceHandle handle, EventTypes) noexcept {
                                   ++invocations;
                                   paused = reactor->SetMonitoredEvents(handle, EmptyEvent()).has_value();
                               })
                    .has_value());

    PokeByte(pair.WriteFd());
    ASSERT_TRUE(PumpUntil(*reactor, paused, 4)) << "the callback could not empty its own mask";

    ASSERT_TRUE(PumpPasses(*reactor, 3, std::chrono::milliseconds{100}));
    EXPECT_EQ(invocations.load(), 1U) << "the mask a callback emptied did not take effect";
}

TEST(ReentrancyTest, ACallbackMayUnregisterItself)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    SocketPair pair{};
    ASSERT_TRUE(pair.Open());

    std::atomic<std::uint32_t> invocations{0U};
    std::atomic_bool withdrawn{false};

    ASSERT_TRUE(reactor
                    ->Register(pair.ReadFd(),
                               ReadEvent(),
                               [&](const EventSourceHandle handle, EventTypes) noexcept {
                                   ++invocations;
                                   reactor->Unregister(handle);
                                   withdrawn = true;
                               })
                    .has_value());

    PokeByte(pair.WriteFd());
    ASSERT_TRUE(PumpUntil(*reactor, withdrawn, 4)) << "the callback never ran";

    ASSERT_TRUE(PumpPasses(*reactor, 3, std::chrono::milliseconds{100}));
    EXPECT_EQ(invocations.load(), 1U) << "a callback that withdrew itself kept being dispatched";
}

TEST(ReentrancyTest, ACallbackMayUnregisterASibling)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    SocketPair driver{};
    SocketPair sibling{};
    ASSERT_TRUE(driver.Open());
    ASSERT_TRUE(sibling.Open());

    const int driver_read_fd{driver.ReadFd()};
    std::atomic_bool sibling_fired{false};
    const auto sibling_reg =
        reactor->Register(sibling.ReadFd(), ReadEvent(), [&sibling_fired](EventSourceHandle, EventTypes) noexcept {
            sibling_fired = true;
        });
    ASSERT_TRUE(sibling_reg.has_value());

    std::atomic_bool withdrawn{false};
    ASSERT_TRUE(reactor
                    ->Register(driver_read_fd,
                               ReadEvent(),
                               [&](EventSourceHandle, EventTypes) noexcept {
                                   DrainByte(driver_read_fd);
                                   reactor->Unregister(sibling_reg.value());
                                   withdrawn = true;
                               })
                    .has_value());

    PokeByte(sibling.WriteFd());
    PokeByte(driver.WriteFd());
    ASSERT_TRUE(PumpUntil(*reactor, withdrawn, 4)) << "the driving callback never ran";

    sibling_fired = false;
    ASSERT_TRUE(PumpPasses(*reactor, 3, std::chrono::milliseconds{100}));
    EXPECT_FALSE(sibling_fired.load()) << "the sibling a callback withdrew kept being dispatched";
}
