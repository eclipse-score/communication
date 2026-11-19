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
 * @file lifetime_test.cpp
 * @brief What Unregister() and the reactor's destruction do to the descriptors and to the remaining sources.
 *
 * @note The reactor never owns a descriptor a caller handed it, so neither Unregister nor destruction closes one.
 */

#include "test_utils.h"

#include <gtest/gtest.h>

#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>

using namespace io_dispatcher;
using namespace test_utils;

namespace
{

/**
 * @brief Registers itself with a callback that captures only `this`.
 * @note Destroy() leaves the storage allocated and sets a flag in place of the free, so a callback that outlives it
 * shows as a failed expectation instead of a crash.
 */
class SelfRegisteredOwner final
{
  public:
    SelfRegisteredOwner(IoDispatcher& reactor, const NativeHandle fd) : reactor_{reactor}
    {
        const auto registered = reactor_.Register(fd, ReadEvent(), [this](EventSourceHandle, EventTypes) noexcept {
            OnEvent();
        });
        EXPECT_TRUE(registered.has_value());
        handle_ = registered.value();
    }

    /**
     * @brief Unregisters, then marks the members as gone.
     */
    void Destroy() noexcept
    {
        reactor_.Unregister(handle_);
        destroyed_.store(true);
    }

    std::atomic_bool entered{false};
    std::atomic_bool released{false};
    std::atomic_bool used_after_destroy{false};

  private:
    void OnEvent() noexcept
    {
        entered.store(true);
        while (!released.load())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
        if (destroyed_.load())
        {
            used_after_destroy.store(true);
        }
    }

    IoDispatcher& reactor_;
    EventSourceHandle handle_{kInvalidEventSourceHandle};
    std::atomic_bool destroyed_{false};
};

}  // namespace

TEST(LifetimeTest, UnregisterLeavesTheDescriptorOpen)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    SocketPair pair{};
    ASSERT_TRUE(pair.Open());

    const auto reg = reactor->Register(pair.ReadFd(), ReadEvent(), [](EventSourceHandle, EventTypes) noexcept {});
    ASSERT_TRUE(reg.has_value());
    reactor->Unregister(reg.value());

    PokeByte(pair.WriteFd());
    std::uint8_t byte{0};
    EXPECT_EQ(::read(pair.ReadFd(), &byte, sizeof(byte)), static_cast<ssize_t>(sizeof(byte)))
        << "the descriptor stopped working once its registration went";
}

TEST(LifetimeTest, UnregisteringTwiceIsHarmless)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    SocketPair pair{};
    ASSERT_TRUE(pair.Open());

    const auto reg = reactor->Register(pair.ReadFd(), ReadEvent(), [](EventSourceHandle, EventTypes) noexcept {});
    ASSERT_TRUE(reg.has_value());

    reactor->Unregister(reg.value());
    reactor->Unregister(reg.value());

    const auto result = reactor->HandleEvents(std::chrono::milliseconds{0});
    EXPECT_TRUE(result.has_value()) << "the reactor was left unusable by the second withdrawal";
}

TEST(LifetimeTest, UnregisteringAHandleNamingNoSourceIsHarmless)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    reactor->Unregister(kInvalidEventSourceHandle);

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
    EXPECT_TRUE(PumpUntil(*reactor, fired, 4)) << "the reactor was left unusable by a withdrawal that named nothing";
}

TEST(LifetimeTest, AnUnregisteredSourceIsNoLongerDispatchedOn)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    SocketPair pair{};
    ASSERT_TRUE(pair.Open());

    std::atomic_bool fired{false};
    const auto reg = reactor->Register(pair.ReadFd(), ReadEvent(), [&fired](EventSourceHandle, EventTypes) noexcept {
        fired = true;
    });
    ASSERT_TRUE(reg.has_value());
    reactor->Unregister(reg.value());

    PokeByte(pair.WriteFd());
    DrainWakeup(*reactor);
    const auto result = reactor->HandleEvents(std::chrono::milliseconds{100});

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), UnblockReason::kTimeout);
    EXPECT_FALSE(fired.load()) << "a withdrawn source was dispatched on";
}

TEST(LifetimeTest, UnregisteringOneSourceLeavesItsSiblingDispatching)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    SocketPair withdrawn{};
    SocketPair kept{};
    ASSERT_TRUE(withdrawn.Open());
    ASSERT_TRUE(kept.Open());

    std::atomic_bool withdrawn_fired{false};
    std::atomic_bool kept_fired{false};
    const auto going =
        reactor->Register(withdrawn.ReadFd(), ReadEvent(), [&withdrawn_fired](EventSourceHandle, EventTypes) noexcept {
            withdrawn_fired = true;
        });
    ASSERT_TRUE(going.has_value());
    ASSERT_TRUE(reactor
                    ->Register(kept.ReadFd(),
                               ReadEvent(),
                               [&kept_fired](EventSourceHandle, EventTypes) noexcept {
                                   kept_fired = true;
                               })
                    .has_value());

    reactor->Unregister(going.value());

    PokeByte(withdrawn.WriteFd());
    PokeByte(kept.WriteFd());

    EXPECT_TRUE(PumpUntil(*reactor, kept_fired, 4)) << "withdrawing one source silenced its sibling";
    EXPECT_FALSE(withdrawn_fired.load());
}

TEST(LifetimeTest, DestroyingTheReactorLeavesRegisteredDescriptorsOpen)
{
    SocketPair pair{};
    ASSERT_TRUE(pair.Open());

    {
        const auto reactor = MakeReactor(8U);
        ASSERT_NE(reactor, nullptr);
        ASSERT_TRUE(
            reactor->Register(pair.ReadFd(), ReadEvent(), [](EventSourceHandle, EventTypes) noexcept {}).has_value());
    }

    PokeByte(pair.WriteFd());
    std::uint8_t byte{0};
    EXPECT_EQ(::read(pair.ReadFd(), &byte, sizeof(byte)), static_cast<ssize_t>(sizeof(byte)))
        << "the reactor closed a descriptor it never owned";
}

TEST(LifetimeTest, UnregisterFromAnotherThreadWaitsForTheRunningCallbackOfThatSource)
{
    const auto reactor = MakeReactor(4U);
    ASSERT_NE(reactor, nullptr);
    SocketPair pair{};
    ASSERT_TRUE(pair.Open());
    SelfRegisteredOwner owner{*reactor, pair.ReadFd()};

    ReactorThread pump{*reactor};
    PokeByte(pair.WriteFd());
    ASSERT_TRUE(WaitFor(
        [&owner]() {
            return owner.entered.load();
        },
        Clock::now() + std::chrono::seconds{5}))
        << "the callback never reached its hold";

    // Released from a third thread, so an Unregister() that waits for the callback still returns.
    std::thread releaser{[&owner]() {
        std::this_thread::sleep_for(std::chrono::milliseconds{200});
        owner.released.store(true);
    }};
    owner.Destroy();
    releaser.join();
    pump.Stop();

    EXPECT_FALSE(owner.used_after_destroy.load()) << "Unregister() returned while the callback of its source still ran";
}

TEST(LifetimeTest, UnregisterReleasesTheCallbackOfAnIdleSource)
{
    const auto reactor = MakeReactor(4U);
    ASSERT_NE(reactor, nullptr);
    SocketPair pair{};
    ASSERT_TRUE(pair.Open());
    auto capture = std::make_shared<int>(0);
    const std::weak_ptr<int> observer{capture};
    const auto reg = reactor->Register(pair.ReadFd(), ReadEvent(), [capture](EventSourceHandle, EventTypes) noexcept {
        ++*capture;
    });
    ASSERT_TRUE(reg.has_value());
    capture.reset();

    reactor->Unregister(reg.value());

    EXPECT_TRUE(observer.expired()) << "Unregister() kept the captures of the callback alive";
}

TEST(LifetimeTest, UnregisterFromAnotherThreadReleasesTheRunningCallbackBeforeReturning)
{
    const auto reactor = MakeReactor(4U);
    ASSERT_NE(reactor, nullptr);
    SocketPair pair{};
    ASSERT_TRUE(pair.Open());
    std::atomic_bool entered{false};
    std::atomic_bool released{false};
    auto capture = std::make_shared<int>(0);
    const std::weak_ptr<int> observer{capture};
    const auto reg = reactor->Register(
        pair.ReadFd(), ReadEvent(), [capture, &entered, &released](EventSourceHandle, EventTypes) noexcept {
            entered.store(true);
            while (!released.load())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds{1});
            }
        });
    ASSERT_TRUE(reg.has_value());
    capture.reset();

    ReactorThread pump{*reactor};
    PokeByte(pair.WriteFd());
    ASSERT_TRUE(WaitFor(
        [&entered]() {
            return entered.load();
        },
        Clock::now() + std::chrono::seconds{5}))
        << "the callback never reached its hold";
    std::thread releaser{[&released]() {
        std::this_thread::sleep_for(std::chrono::milliseconds{500});
        released.store(true);
    }};
    const bool still_held{!released.load()};
    reactor->Unregister(reg.value());
    const bool expired_on_return{observer.expired()};
    releaser.join();
    pump.Stop();

    EXPECT_TRUE(still_held) << "the callback returned before Unregister(), so the running path went untested";
    EXPECT_TRUE(expired_on_return) << "Unregister() returned with the captures of the callback still alive";
}

TEST(LifetimeTest, ACallbackUnregisteringItsOwnSourceIsReleasedOnceItReturns)
{
    const auto reactor = MakeReactor(4U);
    ASSERT_NE(reactor, nullptr);
    SocketPair pair{};
    ASSERT_TRUE(pair.Open());
    EventSourceHandle handle{kInvalidEventSourceHandle};
    std::atomic_bool fired{false};
    std::atomic_bool alive_after_unregister{false};
    auto capture = std::make_shared<int>(0);
    const std::weak_ptr<int> observer{capture};
    const auto reg = reactor->Register(pair.ReadFd(),
                                       ReadEvent(),
                                       [capture, &reactor, &handle, &observer, &fired, &alive_after_unregister](
                                           EventSourceHandle, EventTypes) noexcept {
                                           reactor->Unregister(handle);
                                           alive_after_unregister.store(!observer.expired());
                                           fired.store(true);
                                       });
    ASSERT_TRUE(reg.has_value());
    handle = reg.value();
    capture.reset();

    PokeByte(pair.WriteFd());
    ASSERT_TRUE(PumpUntil(*reactor, fired, 4));

    EXPECT_TRUE(alive_after_unregister.load()) << "the callback was destroyed while it still ran";
    EXPECT_TRUE(observer.expired()) << "the callback outlived its return after unregistering its own source";
}

// io_dispatcher.h, Unregister(): once it returns, the dispatcher no longer waits on the source.
// The pump waits 5 s at a time, so a close that raced a blocked wait would keep the port bound for the whole test.
TEST(LifetimeTest, ASocketClosedRightAfterUnregisterFromAnotherThreadReleasesItsPort)
{
    constexpr int kAttempts{20};
    const auto reactor = MakeReactor(4U);
    ASSERT_NE(reactor, nullptr);
    ReactorThread pump{*reactor, std::chrono::seconds{5}};

    for (int attempt{0}; attempt < kAttempts; ++attempt)
    {
        sockaddr_in bound{};
        UniqueFd socket = MakeUdpSocket(bound);
        ASSERT_TRUE(socket.IsValid());
        const auto handle = reactor->Register(socket.Get(), ReadEvent(), [](EventSourceHandle, EventTypes) noexcept {});
        ASSERT_TRUE(handle.has_value());
        // Gives the pump time to rebuild its set with the socket and block in the wait.
        std::this_thread::sleep_for(std::chrono::milliseconds{20});

        reactor->Unregister(handle.value());
        socket.Close();

        UniqueFd rebound{::socket(AF_INET, SOCK_DGRAM, 0)};
        ASSERT_TRUE(rebound.IsValid());
        EXPECT_EQ(::bind(rebound.Get(), reinterpret_cast<const sockaddr*>(&bound), sizeof(bound)), 0)
            << "attempt " << attempt << ": the closed socket still holds its port";
    }

    pump.Stop();
}
