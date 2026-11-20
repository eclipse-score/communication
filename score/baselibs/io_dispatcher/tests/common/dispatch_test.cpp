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
 * @file dispatch_test.cpp
 * @brief What one HandleEvents() pass reports to a callback: which sources, which event types, and how often.
 *
 * @note A listener, an accepted channel and a datagram socket become ready for different reasons, so each has a case.
 */

#include "test_utils.h"

#include <gtest/gtest.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

using namespace io_dispatcher;
using namespace test_utils;

TEST(DispatchTest, EveryReadySourceIsServedInOnePass)
{
    constexpr std::size_t kSources{3U};
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    auto pool = MakePool(kSources);
    if (pool.size() < kSources)
    {
        GTEST_SKIP() << "no descriptors for a pool of " << kSources << " pairs";
    }

    std::atomic<std::uint32_t> served{0U};
    for (auto& pair : pool)
    {
        ASSERT_TRUE(reactor
                        ->Register(pair.ReadFd(),
                                   ReadEvent(),
                                   [&served](EventSourceHandle, EventTypes) noexcept {
                                       ++served;
                                   })
                        .has_value());
    }

    // The pokes come after the wake-up is consumed, so the pass that counts is the first one to see them.
    DrainWakeup(*reactor);
    for (auto& pair : pool)
    {
        PokeByte(pair.WriteFd());
    }
    ASSERT_TRUE(reactor->HandleEvents(std::chrono::milliseconds{300}).has_value());

    EXPECT_EQ(served.load(), kSources) << "one pass left a ready source unserved";
}

TEST(DispatchTest, ACallbackIsToldItsOwnHandle)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    SocketPair pair{};
    ASSERT_TRUE(pair.Open());

    std::atomic_bool fired{false};
    std::atomic<EventSourceHandle> reported{kInvalidEventSourceHandle};
    const auto reg =
        reactor->Register(pair.ReadFd(), ReadEvent(), [&](const EventSourceHandle handle, EventTypes) noexcept {
            reported = handle;
            fired = true;
        });
    ASSERT_TRUE(reg.has_value());

    PokeByte(pair.WriteFd());
    ASSERT_TRUE(PumpUntil(*reactor, fired, 4));

    EXPECT_EQ(reported.load(), reg.value()) << "the callback was told a handle its registration never had";
}

TEST(DispatchTest, AReadableSourceIsNotReportedWritableAndFiresOnce)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    SocketPair pair{};
    ASSERT_TRUE(pair.Open());

    std::atomic_bool fired{false};
    std::atomic_bool got_read{false};
    std::atomic_bool got_write{false};
    std::atomic<std::uint32_t> invocations{0U};
    ASSERT_TRUE(reactor
                    ->Register(pair.ReadFd(),
                               ReadEvent(),
                               [&](EventSourceHandle, const EventTypes events) noexcept {
                                   got_read = events.HasReadEvent();
                                   got_write = events.HasWriteEvent();
                                   ++invocations;
                                   fired = true;
                               })
                    .has_value());

    PokeByte(pair.WriteFd());
    ASSERT_TRUE(PumpUntil(*reactor, fired, 4));

    EXPECT_TRUE(got_read.load());
    EXPECT_FALSE(got_write.load()) << "a readable socket was reported writable although only read was monitored";
    EXPECT_EQ(invocations.load(), 1U) << "one datum reported as more than one readiness";
}

TEST(DispatchTest, ReadAndWriteArriveInOneInvocation)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    SocketPair pair{};
    ASSERT_TRUE(pair.Open());

    std::atomic_bool fired{false};
    std::atomic_bool both{false};
    ASSERT_TRUE(reactor
                    ->Register(pair.ReadFd(),
                               ReadWriteEvent(),
                               [&](EventSourceHandle, const EventTypes events) noexcept {
                                   both = events.HasReadEvent() && events.HasWriteEvent();
                                   fired = true;
                               })
                    .has_value());

    PokeByte(pair.WriteFd());
    ASSERT_TRUE(PumpUntil(*reactor, fired, 4));

    EXPECT_TRUE(both.load()) << "a socket that was both readable and writable reported only one of the two";
}

TEST(DispatchTest, DataNobodyReadIsReportedAgain)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    SocketPair pair{};
    ASSERT_TRUE(pair.Open());

    std::atomic<std::uint32_t> invocations{0U};
    ASSERT_TRUE(reactor
                    ->Register(pair.ReadFd(),
                               ReadEvent(),
                               [&invocations](EventSourceHandle, EventTypes) noexcept {
                                   ++invocations;
                               })
                    .has_value());

    PokeByte(pair.WriteFd());
    ASSERT_TRUE(PumpPasses(*reactor, 4, std::chrono::milliseconds{200}));

    EXPECT_GE(invocations.load(), 2U) << "a datum the callback never read stopped being reported";
}

TEST(DispatchTest, DataTheCallbackReadIsNotReportedAgain)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    SocketPair pair{};
    ASSERT_TRUE(pair.Open());

    const int read_fd{pair.ReadFd()};
    std::atomic<std::uint32_t> invocations{0U};
    ASSERT_TRUE(reactor
                    ->Register(read_fd,
                               ReadEvent(),
                               [&invocations, read_fd](EventSourceHandle, EventTypes) noexcept {
                                   DrainByte(read_fd);
                                   ++invocations;
                               })
                    .has_value());

    PokeByte(pair.WriteFd());
    ASSERT_TRUE(PumpPasses(*reactor, 4, std::chrono::milliseconds{200}));

    EXPECT_EQ(invocations.load(), 1U) << "a drained source kept being reported";
}

TEST(DispatchTest, APeerThatClosedIsReportedToAReadWatcher)
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

    pair.WriteEnd().Close();

    EXPECT_TRUE(PumpUntil(*reactor, fired, 4)) << "the close of the peer never reached the read watcher";
}

TEST(DispatchTest, ATcpListenerIsReadableOnAnIncomingConnection)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    sockaddr_in bound{};
    UniqueFd listener{MakeTcpListener(bound)};
    ASSERT_TRUE(listener.IsValid());

    std::atomic_bool fired{false};
    ASSERT_TRUE(reactor
                    ->Register(listener.Get(),
                               ReadEvent(),
                               [&fired](EventSourceHandle, EventTypes) noexcept {
                                   fired = true;
                               })
                    .has_value());

    UniqueFd client{::socket(AF_INET, SOCK_STREAM, 0)};
    ASSERT_TRUE(client.IsValid());
    ASSERT_EQ(::connect(client.Get(), reinterpret_cast<sockaddr*>(&bound), sizeof(bound)), 0);

    EXPECT_TRUE(PumpUntil(*reactor, fired, 4)) << "the acceptor never became readable";
}

TEST(DispatchTest, AnAcceptedChannelIsReadableOnIncomingData)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    sockaddr_in bound{};
    UniqueFd listener{MakeTcpListener(bound)};
    ASSERT_TRUE(listener.IsValid());

    UniqueFd client{::socket(AF_INET, SOCK_STREAM, 0)};
    ASSERT_TRUE(client.IsValid());
    ASSERT_EQ(::connect(client.Get(), reinterpret_cast<sockaddr*>(&bound), sizeof(bound)), 0);

    UniqueFd channel{::accept(listener.Get(), nullptr, nullptr)};
    ASSERT_TRUE(channel.IsValid());

    std::atomic_bool fired{false};
    ASSERT_TRUE(reactor
                    ->Register(channel.Get(),
                               ReadEvent(),
                               [&fired](EventSourceHandle, EventTypes) noexcept {
                                   fired = true;
                               })
                    .has_value());

    PokeByte(client.Get());

    EXPECT_TRUE(PumpUntil(*reactor, fired, 4)) << "the accepted channel never became readable";
}

TEST(DispatchTest, AUdpSocketIsReadableOnADatagram)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    sockaddr_in bound{};
    UniqueFd receiver{MakeUdpSocket(bound)};
    ASSERT_TRUE(receiver.IsValid());
    UniqueFd sender{::socket(AF_INET, SOCK_DGRAM, 0)};
    ASSERT_TRUE(sender.IsValid());

    std::atomic_bool fired{false};
    std::atomic_bool got_write{false};
    ASSERT_TRUE(reactor
                    ->Register(receiver.Get(),
                               ReadEvent(),
                               [&](EventSourceHandle, const EventTypes events) noexcept {
                                   got_write = events.HasWriteEvent();
                                   fired = true;
                               })
                    .has_value());

    const std::uint8_t datagram{0x5AU};
    ASSERT_EQ(
        ::sendto(sender.Get(), &datagram, sizeof(datagram), 0, reinterpret_cast<sockaddr*>(&bound), sizeof(bound)),
        static_cast<ssize_t>(sizeof(datagram)));

    ASSERT_TRUE(PumpUntil(*reactor, fired, 4)) << "the datagram never reached the receiver's callback";
    EXPECT_FALSE(got_write.load()) << "a readable datagram socket was reported writable too";
}

TEST(DispatchTest, TwoSourcesDemultiplexToTheirOwnHandles)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    SocketPair first{};
    SocketPair second{};
    ASSERT_TRUE(first.Open());
    ASSERT_TRUE(second.Open());

    std::atomic<EventSourceHandle> reported{kInvalidEventSourceHandle};
    std::atomic_bool fired{false};
    const auto shared_callback = [&](const EventSourceHandle handle, EventTypes) noexcept {
        reported = handle;
        fired = true;
    };

    const auto one = reactor->Register(first.ReadFd(), ReadEvent(), shared_callback);
    const auto other = reactor->Register(second.ReadFd(), ReadEvent(), shared_callback);
    ASSERT_TRUE(one.has_value());
    ASSERT_TRUE(other.has_value());

    PokeByte(second.WriteFd());
    ASSERT_TRUE(PumpUntil(*reactor, fired, 4));

    EXPECT_EQ(reported.load(), other.value()) << "the shared callback was told the idle source's handle";
}
