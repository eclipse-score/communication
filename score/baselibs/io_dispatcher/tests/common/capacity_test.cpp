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
 * @file capacity_test.cpp
 * @brief How many sources a reactor accepts, what Register() returns once it is full, and what Unregister() frees.
 *
 * @note The maximum reactor size is backend specific; every case here stays within a capacity both backends allow.
 */

#include "score/io_dispatcher/io_dispatcher_error.h"
#include "test_utils.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <limits>

using namespace io_dispatcher;
using namespace test_utils;

TEST(CapacityTest, AReactorWithNoSlotsRefusesEveryRegistration)
{
    const auto reactor = MakeReactor(0U);
    ASSERT_NE(reactor, nullptr) << "a reactor with no slots is still a reactor";

    SocketPair pair{};
    ASSERT_TRUE(pair.Open());

    const auto reg = reactor->Register(pair.ReadFd(), ReadEvent(), [](EventSourceHandle, EventTypes) noexcept {});
    ASSERT_FALSE(reg.has_value());
    EXPECT_EQ(reg.error(), IoDispatcherErrc::kResource);
}

// io_dispatcher_factory.h: MaxNumEventSources() is the largest count MakeIoDispatcher() accepts on this platform.
TEST(CapacityTest, MaxNumEventSourcesIsTheLargestReactorTheFactoryBuilds)
{
    const std::uint16_t largest{MaxNumEventSources()};
    EXPECT_TRUE(MakeIoDispatcher(largest).has_value()) << "the reported maximum was refused";

    if (largest < std::numeric_limits<std::uint16_t>::max())
    {
        const auto above = MakeIoDispatcher(static_cast<std::uint16_t>(largest + 1U));
        ASSERT_FALSE(above.has_value()) << "a reactor above the reported maximum was built";
        EXPECT_EQ(above.error(), IoDispatcherErrc::kResource);
    }
}

TEST(CapacityTest, AFreedSlotIsGivenToTheNextSource)
{
    const auto reactor = MakeReactor(1U);
    ASSERT_NE(reactor, nullptr);

    SocketPair first{};
    SocketPair second{};
    ASSERT_TRUE(first.Open());
    ASSERT_TRUE(second.Open());

    const auto held = reactor->Register(first.ReadFd(), ReadEvent(), [](EventSourceHandle, EventTypes) noexcept {});
    ASSERT_TRUE(held.has_value());
    ASSERT_FALSE(
        reactor->Register(second.ReadFd(), ReadEvent(), [](EventSourceHandle, EventTypes) noexcept {}).has_value());

    reactor->Unregister(held.value());

    EXPECT_TRUE(
        reactor->Register(second.ReadFd(), ReadEvent(), [](EventSourceHandle, EventTypes) noexcept {}).has_value())
        << "the slot the withdrawn source held was never handed on";
}

TEST(CapacityTest, TheLastFreeSlotIsTheLastRegistrationThatSucceeds)
{
    constexpr std::uint16_t kSlots{2U};
    const auto reactor = MakeReactor(kSlots);
    ASSERT_NE(reactor, nullptr);

    auto pool = MakePool(kSlots + 1U);
    if (pool.size() < (kSlots + 1U))
    {
        GTEST_SKIP() << "no descriptors for a pool of " << (kSlots + 1U) << " pairs";
    }

    for (std::size_t i{0U}; i < kSlots; ++i)
    {
        EXPECT_TRUE(
            reactor->Register(pool[i].ReadFd(), ReadEvent(), [](EventSourceHandle, EventTypes) noexcept {}).has_value())
            << "registration " << i << " was refused below the reactor's capacity";
    }

    const auto refused =
        reactor->Register(pool[kSlots].ReadFd(), ReadEvent(), [](EventSourceHandle, EventTypes) noexcept {});
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error(), IoDispatcherErrc::kResource);
}
