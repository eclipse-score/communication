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
 * @file monitored_events_test.cpp
 * @brief Changing a live registration: which events it is watched for, and which callback receives them.
 */

#include "score/io_dispatcher/io_dispatcher_error.h"
#include "test_utils.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <ostream>

using namespace io_dispatcher;
using namespace test_utils;

namespace
{

/**
 * @brief What the callback last saw, so a test can tell an absent invocation from a wrong one.
 */
struct Reported
{
    std::atomic_bool fired{false};
    std::atomic_bool read{false};
    std::atomic_bool write{false};
};

CallbackType Recorder(Reported& reported) noexcept
{
    return [&reported](EventSourceHandle, const EventTypes events) noexcept {
        reported.read = events.HasReadEvent();
        reported.write = events.HasWriteEvent();
        reported.fired = true;
    };
}

/**
 * @brief One of the two calls that change a live registration; the handle cases run for both.
 */
struct Mutator
{
    const char* name;
    score::Result<void> (*apply)(IoDispatcher&, EventSourceHandle);
};

void PrintTo(const Mutator& mutator, std::ostream* stream)
{
    *stream << mutator.name;
}

const Mutator kSetMonitoredEvents{"SetMonitoredEvents", [](IoDispatcher& reactor, const EventSourceHandle handle) {
                                      return reactor.SetMonitoredEvents(handle, ReadEvent());
                                  }};

const Mutator kSetCallbackTarget{"SetCallbackTarget", [](IoDispatcher& reactor, const EventSourceHandle handle) {
                                     return reactor.SetCallbackTarget(handle,
                                                                      [](EventSourceHandle, EventTypes) noexcept {});
                                 }};

/**
 * @brief Whether `passes` waits of 100 ms each all time out without a callback.
 * @note poll() reports a hang-up or an error even for a descriptor asked for no events, so a backend that keeps an
 *       emptied source in its set wakes at once on every pass.
 */
::testing::AssertionResult EveryWaitTimesOut(IoDispatcher& reactor, const Reported& reported, const int passes)
{
    for (int pass{0}; pass < passes; ++pass)
    {
        const auto result = reactor.HandleEvents(std::chrono::milliseconds{100});
        if (!result.has_value())
        {
            return ::testing::AssertionFailure() << "pass " << pass << " failed";
        }

        if (result.value() != UnblockReason::kTimeout)
        {
            return ::testing::AssertionFailure() << "pass " << pass << " ended early";
        }
    }

    if (reported.fired.load())
    {
        return ::testing::AssertionFailure() << "a source watched for no events was dispatched";
    }

    return ::testing::AssertionSuccess();
}

}  // namespace

TEST(MonitoredEventsTest, SetReplacesTheWholeMask)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    SocketPair pair{};
    ASSERT_TRUE(pair.Open());

    Reported reported{};
    const auto reg = reactor->Register(pair.ReadFd(), ReadEvent(), Recorder(reported));
    ASSERT_TRUE(reg.has_value());

    PokeByte(pair.WriteFd());
    ASSERT_TRUE(reactor->SetMonitoredEvents(reg.value(), WriteEvent()).has_value());

    ASSERT_TRUE(PumpUntil(*reactor, reported.fired, 4));
    EXPECT_TRUE(reported.write.load());
    EXPECT_FALSE(reported.read.load()) << "read survived a mask that names write alone";
}

TEST(MonitoredEventsTest, AnEmptiedMaskStopsDispatch)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    SocketPair pair{};
    ASSERT_TRUE(pair.Open());

    Reported reported{};
    const auto reg = reactor->Register(pair.ReadFd(), ReadEvent(), Recorder(reported));
    ASSERT_TRUE(reg.has_value());
    ASSERT_TRUE(reactor->SetMonitoredEvents(reg.value(), EmptyEvent()).has_value());

    PokeByte(pair.WriteFd());
    DrainWakeup(*reactor);
    const auto result = reactor->HandleEvents(std::chrono::milliseconds{100});

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), UnblockReason::kTimeout);
    EXPECT_FALSE(reported.fired.load()) << "an emptied mask still dispatched";
}

TEST(MonitoredEventsTest, AnEmptiedMaskKeepsAPeersCloseFromEndingTheWait)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);
    SocketPair pair{};
    ASSERT_TRUE(pair.Open());
    Reported reported{};
    const auto reg = reactor->Register(pair.ReadFd(), ReadEvent(), Recorder(reported));
    ASSERT_TRUE(reg.has_value());
    ASSERT_TRUE(reactor->SetMonitoredEvents(reg.value(), EmptyEvent()).has_value());

    pair.WriteEnd().Close();
    DrainWakeup(*reactor);

    EXPECT_TRUE(EveryWaitTimesOut(*reactor, reported, 3));
}

TEST(MonitoredEventsTest, AnEmptiedMaskKeepsAPeersResetFromEndingTheWait)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);
    sockaddr_in address{};
    UniqueFd listener{MakeTcpListener(address)};
    ASSERT_TRUE(listener.IsValid());
    UniqueFd client{::socket(AF_INET, SOCK_STREAM, 0)};
    ASSERT_TRUE(client.IsValid());
    ASSERT_EQ(::connect(client.Get(), reinterpret_cast<const sockaddr*>(&address), sizeof(address)), 0);
    UniqueFd accepted{::accept(listener.Get(), nullptr, nullptr)};
    ASSERT_TRUE(accepted.IsValid());
    Reported reported{};
    const auto reg = reactor->Register(accepted.Get(), ReadEvent(), Recorder(reported));
    ASSERT_TRUE(reg.has_value());
    ASSERT_TRUE(reactor->SetMonitoredEvents(reg.value(), EmptyEvent()).has_value());

    // SO_LINGER 0 makes the close send a reset, which poll() reports as an error and a hang-up.
    linger reset{};
    reset.l_onoff = 1;
    reset.l_linger = 0;
    ASSERT_EQ(::setsockopt(client.Get(), SOL_SOCKET, SO_LINGER, &reset, sizeof(reset)), 0);
    client.Close();
    DrainWakeup(*reactor);

    EXPECT_TRUE(EveryWaitTimesOut(*reactor, reported, 3));
}

TEST(MonitoredEventsTest, PausedReadHandsOverPendingDataWhenInterestReturns)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    SocketPair pair{};
    ASSERT_TRUE(pair.Open());

    Reported reported{};
    const auto reg = reactor->Register(pair.ReadFd(), ReadEvent(), Recorder(reported));
    ASSERT_TRUE(reg.has_value());
    ASSERT_TRUE(reactor->SetMonitoredEvents(reg.value(), EmptyEvent()).has_value());

    PokeByte(pair.WriteFd());
    DrainWakeup(*reactor);
    ASSERT_TRUE(reactor->HandleEvents(std::chrono::milliseconds{100}).has_value());
    ASSERT_FALSE(reported.fired.load()) << "the pause did not hold the datum back";

    ASSERT_TRUE(reactor->SetMonitoredEvents(reg.value(), ReadEvent()).has_value());

    EXPECT_TRUE(PumpUntil(*reactor, reported.fired, 4)) << "the datum held back over the pause was lost";
    EXPECT_TRUE(reported.read.load());
}

TEST(MonitoredEventsTest, MonitoringReadStartsDispatchOnAnEmptyRegistration)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    SocketPair pair{};
    ASSERT_TRUE(pair.Open());

    Reported reported{};
    const auto reg = reactor->Register(pair.ReadFd(), EmptyEvent(), Recorder(reported));
    ASSERT_TRUE(reg.has_value());

    PokeByte(pair.WriteFd());
    ASSERT_TRUE(reactor->SetMonitoredEvents(reg.value(), ReadEvent()).has_value());

    EXPECT_TRUE(PumpUntil(*reactor, reported.fired, 4));
    EXPECT_TRUE(reported.read.load());
}

TEST(MonitoredEventsTest, SetCallbackTargetRedirectsDispatch)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    SocketPair pair{};
    ASSERT_TRUE(pair.Open());

    std::atomic<std::uint32_t> first_target{0U};
    std::atomic_bool second_target{false};
    const auto reg =
        reactor->Register(pair.ReadFd(), ReadEvent(), [&first_target](EventSourceHandle, EventTypes) noexcept {
            ++first_target;
        });
    ASSERT_TRUE(reg.has_value());

    ASSERT_TRUE(reactor
                    ->SetCallbackTarget(reg.value(),
                                        [&second_target](EventSourceHandle, EventTypes) noexcept {
                                            second_target = true;
                                        })
                    .has_value());

    PokeByte(pair.WriteFd());
    ASSERT_TRUE(PumpUntil(*reactor, second_target, 4));
    EXPECT_EQ(first_target.load(), 0U) << "the target that was replaced still ran";
}

class MutatorHandleTest : public ::testing::TestWithParam<Mutator>
{
};

TEST_P(MutatorHandleTest, AnUnregisteredHandleIsRefused)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    SocketPair pair{};
    ASSERT_TRUE(pair.Open());

    const auto reg = reactor->Register(pair.ReadFd(), ReadEvent(), [](EventSourceHandle, EventTypes) noexcept {});
    ASSERT_TRUE(reg.has_value());
    reactor->Unregister(reg.value());

    const auto result = GetParam().apply(*reactor, reg.value());
    ASSERT_FALSE(result.has_value()) << "a handle nothing is registered under was accepted";
    EXPECT_EQ(result.error(), IoDispatcherErrc::kApiError);
}

TEST_P(MutatorHandleTest, TheInvalidHandleIsRefused)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    const auto result = GetParam().apply(*reactor, kInvalidEventSourceHandle);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), IoDispatcherErrc::kApiError);
}

INSTANTIATE_TEST_SUITE_P(EveryMutatorOfARegistration,
                         MutatorHandleTest,
                         ::testing::Values(kSetMonitoredEvents, kSetCallbackTarget),
                         [](const ::testing::TestParamInfo<Mutator>& param_info) {
                             return param_info.param.name;
                         });
