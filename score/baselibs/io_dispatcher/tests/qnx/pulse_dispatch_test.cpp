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
 * @file pulse_dispatch_test.cpp
 * @brief Where a pulse put on the reactor's channel ends up, and what it is carrying when it arrives.
 */

#include "pulse_test_utils.h"
#include "test_utils.h"

#include <gtest/gtest.h>

#include <sys/neutrino.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

using namespace io_dispatcher;
using namespace test_utils;
using namespace qnx_test_utils;

TEST(PulseDispatchTest, APulseReachesTheCallbackRegisteredForItsCode)
{
    const auto reactor = MakeReactor(4U);
    ASSERT_NE(reactor, nullptr);

    std::atomic_bool fired{false};
    std::atomic_bool pulse_only{false};
    std::atomic<PulseCode> code{0};
    const auto reg =
        AsImpl(*reactor).RegisterPulse(PulseEvent(kCode), [&](EventSourceHandle, const EventTypes events) noexcept {
            pulse_only = events.HasPulseEvent() && (!events.HasReadEvent()) && (!events.HasWriteEvent());
            code = events.GetPulseCode();
            fired = true;
        });
    ASSERT_TRUE(reg.has_value());

    ASSERT_TRUE(SendPulse(*reactor, kCode, kPayload));

    ASSERT_TRUE(PumpUntil(*reactor, fired, 4)) << "the pulse never reached its callback";
    EXPECT_TRUE(pulse_only.load()) << "a pulse arrived carrying descriptor readiness as well";
    EXPECT_EQ(code.load(), kCode);
}

TEST(PulseDispatchTest, APulseRoutesByCodeAndNotByValue)
{
    const auto reactor = MakeReactor(4U);
    ASSERT_NE(reactor, nullptr);

    std::atomic<std::uint32_t> wanted{0U};
    std::atomic<std::uint32_t> other{0U};
    ASSERT_TRUE(AsImpl(*reactor)
                    .RegisterPulse(PulseEvent(kCode),
                                   [&wanted](EventSourceHandle, EventTypes) noexcept {
                                       ++wanted;
                                   })
                    .has_value());
    ASSERT_TRUE(AsImpl(*reactor)
                    .RegisterPulse(PulseEvent(kOtherCode),
                                   [&other](EventSourceHandle, EventTypes) noexcept {
                                       ++other;
                                   })
                    .has_value());

    ASSERT_TRUE(SendPulse(*reactor, kCode, kPayload));
    ASSERT_TRUE(SendPulse(*reactor, kCode, kPayload + 1));
    ASSERT_TRUE(PumpPasses(*reactor, 2, std::chrono::milliseconds{100}));

    EXPECT_EQ(wanted.load(), 2U) << "two pulses differing only in value were not both routed";
    EXPECT_EQ(other.load(), 0U) << "a pulse reached a callback registered for another code";
}

TEST(PulseDispatchTest, EveryPulseOfABurstIsDelivered)
{
    constexpr std::uint32_t kBurst{5U};
    const auto reactor = MakeReactor(4U);
    ASSERT_NE(reactor, nullptr);

    std::atomic<std::uint32_t> delivered{0U};
    ASSERT_TRUE(AsImpl(*reactor)
                    .RegisterPulse(PulseEvent(kCode),
                                   [&delivered](EventSourceHandle, EventTypes) noexcept {
                                       ++delivered;
                                   })
                    .has_value());

    for (std::uint32_t i{0U}; i < kBurst; ++i)
    {
        ASSERT_TRUE(SendPulse(*reactor, kCode, static_cast<std::int32_t>(i)));
    }

    const auto deadline = Clock::now() + std::chrono::seconds{5};
    while ((delivered.load() < kBurst) && (Clock::now() < deadline))
    {
        score::cpp::ignore = reactor->HandleEvents(std::chrono::milliseconds{100});
    }

    EXPECT_EQ(delivered.load(), kBurst) << "a burst on one code was folded into fewer invocations";
}

TEST(PulseDispatchTest, APulseFromAnotherThreadIsRouted)
{
    const auto reactor = MakeReactor(4U);
    ASSERT_NE(reactor, nullptr);

    std::atomic_bool fired{false};
    ASSERT_TRUE(AsImpl(*reactor)
                    .RegisterPulse(PulseEvent(kCode),
                                   [&fired](EventSourceHandle, EventTypes) noexcept {
                                       fired = true;
                                   })
                    .has_value());

    std::thread sender([&reactor]() {
        std::this_thread::sleep_for(std::chrono::milliseconds{50});
        score::cpp::ignore = SendPulse(*reactor, kCode, kPayload);
    });
    const bool arrived{PumpUntil(*reactor, fired, 8)};
    sender.join();

    EXPECT_TRUE(arrived) << "a pulse sent from another thread was not routed";
}

TEST(PulseDispatchTest, ThePulseTargetCanBeReplaced)
{
    const auto reactor = MakeReactor(4U);
    ASSERT_NE(reactor, nullptr);

    std::atomic<std::uint32_t> first_target{0U};
    std::atomic_bool second_target{false};
    const auto reg =
        AsImpl(*reactor).RegisterPulse(PulseEvent(kCode), [&first_target](EventSourceHandle, EventTypes) noexcept {
            ++first_target;
        });
    ASSERT_TRUE(reg.has_value());

    ASSERT_TRUE(reactor
                    ->SetCallbackTarget(reg.value(),
                                        [&second_target](EventSourceHandle, EventTypes) noexcept {
                                            second_target = true;
                                        })
                    .has_value());

    ASSERT_TRUE(SendPulse(*reactor, kCode, kPayload));

    ASSERT_TRUE(PumpUntil(*reactor, second_target, 4));
    EXPECT_EQ(first_target.load(), 0U) << "the pulse target that was replaced still ran";
}

TEST(PulseDispatchTest, AnUnregisteredCodeReachesNothing)
{
    const auto reactor = MakeReactor(4U);
    ASSERT_NE(reactor, nullptr);

    std::atomic_bool fired{false};
    ASSERT_TRUE(AsImpl(*reactor)
                    .RegisterPulse(PulseEvent(kCode),
                                   [&fired](EventSourceHandle, EventTypes) noexcept {
                                       fired = true;
                                   })
                    .has_value());

    ASSERT_TRUE(SendPulse(*reactor, kOtherCode, kPayload));
    ASSERT_TRUE(PumpPasses(*reactor, 2, std::chrono::milliseconds{100}));

    EXPECT_FALSE(fired.load()) << "a code nobody registered was delivered anyway";
}

TEST(PulseDispatchTest, UnregisterStopsPulseDelivery)
{
    const auto reactor = MakeReactor(4U);
    ASSERT_NE(reactor, nullptr);

    std::atomic_bool fired{false};
    const auto reg =
        AsImpl(*reactor).RegisterPulse(PulseEvent(kCode), [&fired](EventSourceHandle, EventTypes) noexcept {
            fired = true;
        });
    ASSERT_TRUE(reg.has_value());
    reactor->Unregister(reg.value());

    ASSERT_TRUE(SendPulse(*reactor, kCode, kPayload));
    ASSERT_TRUE(PumpPasses(*reactor, 2, std::chrono::milliseconds{100}));

    EXPECT_FALSE(fired.load()) << "a withdrawn pulse registration kept receiving";
}

TEST(PulseDispatchTest, TheUnblockPulseIsNotSurfacedAsAnEvent)
{
    const auto reactor = MakeReactor(4U);
    ASSERT_NE(reactor, nullptr);

    std::atomic_bool fired{false};
    ASSERT_TRUE(AsImpl(*reactor)
                    .RegisterPulse(PulseEvent(kCode),
                                   [&fired](EventSourceHandle, EventTypes) noexcept {
                                       fired = true;
                                   })
                    .has_value());

    reactor->Unblock();
    ASSERT_TRUE(PumpPasses(*reactor, 2, std::chrono::milliseconds{100}));

    EXPECT_FALSE(fired.load()) << "the reactor's own wake-up pulse was handed to an application callback";
}

TEST(PulseDispatchTest, APulseAndAReadinessOutstandingTogetherAreBothDelivered)
{
    const auto reactor = MakeReactor(4U);
    ASSERT_NE(reactor, nullptr);

    SocketPair pair{};
    ASSERT_TRUE(pair.Open());

    std::atomic_bool pulse_fired{false};
    std::atomic_bool readiness_fired{false};
    ASSERT_TRUE(AsImpl(*reactor)
                    .RegisterPulse(PulseEvent(kCode),
                                   [&pulse_fired](EventSourceHandle, EventTypes) noexcept {
                                       pulse_fired = true;
                                   })
                    .has_value());
    ASSERT_TRUE(reactor
                    ->Register(pair.ReadFd(),
                               ReadEvent(),
                               [&readiness_fired](EventSourceHandle, EventTypes) noexcept {
                                   readiness_fired = true;
                               })
                    .has_value());

    PokeByte(pair.WriteFd());
    ASSERT_TRUE(SendPulse(*reactor, kCode, kPayload));

    const auto deadline = Clock::now() + std::chrono::seconds{5};
    while (((!pulse_fired.load()) || (!readiness_fired.load())) && (Clock::now() < deadline))
    {
        score::cpp::ignore = reactor->HandleEvents(std::chrono::milliseconds{100});
    }

    EXPECT_TRUE(pulse_fired.load()) << "a readiness outstanding at the same time swallowed the pulse";
    EXPECT_TRUE(readiness_fired.load()) << "a pulse outstanding at the same time swallowed the readiness";
}

TEST(PulseDispatchTest, AnIdlePulseRegistrationTimesOut)
{
    const auto reactor = MakeReactor(4U);
    ASSERT_NE(reactor, nullptr);

    ASSERT_TRUE(
        AsImpl(*reactor).RegisterPulse(PulseEvent(kCode), [](EventSourceHandle, EventTypes) noexcept {}).has_value());

    DrainWakeup(*reactor);
    const auto result = reactor->HandleEvents(std::chrono::milliseconds{100});

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), UnblockReason::kTimeout);
}
