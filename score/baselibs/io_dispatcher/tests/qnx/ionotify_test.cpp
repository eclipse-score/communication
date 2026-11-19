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
 * @file ionotify_test.cpp
 * @brief Descriptor readiness on QNX, where it is armed with ionotify() and delivered as a pulse.
 *
 * The descriptor has to fit the bits the pulse leaves for it.
 * A descriptor whose conditions already hold is reported without any pulse ever being sent.
 * A descriptor closed behind the reactor is dropped from it when arming reports EBADF.
 */

#include "score/io_dispatcher/io_dispatcher_error.h"
#include "test_utils.h"

#include <gtest/gtest.h>

#include <sys/iomgr.h>
#include <sys/neutrino.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <thread>

using namespace io_dispatcher;
using namespace test_utils;

TEST(IonotifyTest, ADescriptorTooLargeToCarryInAPulseIsRefused)
{
    const auto reactor = MakeReactor(4U);
    ASSERT_NE(reactor, nullptr);

    // The pulse carries the descriptor in the bits the manager leaves free, so a larger number cannot be delivered.
    const NativeHandle too_large{static_cast<NativeHandle>(_NOTIFY_DATA_MASK) + 1};

    const auto reg = reactor->Register(too_large, ReadEvent(), [](EventSourceHandle, EventTypes) noexcept {});

    ASSERT_FALSE(reg.has_value()) << "a descriptor whose readiness could never be delivered was accepted";
    EXPECT_EQ(reg.error(), IoDispatcherErrc::kInvalidHandle);
}

TEST(IonotifyTest, TheInvalidDescriptorIsRefused)
{
    const auto reactor = MakeReactor(4U);
    ASSERT_NE(reactor, nullptr);

    const auto reg =
        reactor->Register(kInvalidNativeHandle, ReadEvent(), [](EventSourceHandle, EventTypes) noexcept {});

    ASSERT_FALSE(reg.has_value()) << "the invalid descriptor was taken as a source";
    EXPECT_EQ(reg.error(), IoDispatcherErrc::kInvalidHandle);
}

TEST(IonotifyTest, ASourceThatIsAlreadyReadyIsReportedWithoutAPulse)
{
    const auto reactor = MakeReactor(4U);
    ASSERT_NE(reactor, nullptr);

    SocketPair pair{};
    ASSERT_TRUE(pair.Open());

    // The data is there before the source is armed, so arming is what has to notice it.
    PokeByte(pair.WriteFd());

    std::atomic_bool fired{false};
    ASSERT_TRUE(reactor
                    ->Register(pair.ReadFd(),
                               ReadEvent(),
                               [&fired](EventSourceHandle, EventTypes) noexcept {
                                   fired = true;
                               })
                    .has_value());

    EXPECT_TRUE(PumpUntil(*reactor, fired, 4)) << "a source that was ready before it was armed was never reported";
}

TEST(IonotifyTest, ADescriptorClosedBeforeItIsArmedIsUnregistered)
{
    // One slot, so the capacity the closed source gives back is what the second registration needs.
    const auto reactor = MakeReactor(1U);
    ASSERT_NE(reactor, nullptr);

    // Both pairs are opened before either descriptor is closed, so the replacement cannot land on a recycled number.
    SocketPair doomed{};
    ASSERT_TRUE(doomed.Open());
    SocketPair replacement{};
    ASSERT_TRUE(replacement.Open());

    ASSERT_TRUE(
        reactor->Register(doomed.ReadFd(), ReadEvent(), [](EventSourceHandle, EventTypes) noexcept {}).has_value());

    // Nothing has pumped yet, so the source is still waiting to be armed when it stops being a descriptor.
    doomed.ReadEnd().Close();
    ASSERT_TRUE(reactor->HandleEvents(std::chrono::milliseconds{0}).has_value());

    EXPECT_TRUE(
        reactor->Register(replacement.ReadFd(), ReadEvent(), [](EventSourceHandle, EventTypes) noexcept {}).has_value())
        << "the only slot stayed taken by a descriptor that is no longer open";
}

TEST(IonotifyTest, ADescriptorAboveTheDescriptorBitsOfThePulseIsRefused)
{
    const auto reactor = MakeReactor(4U);
    ASSERT_NE(reactor, nullptr);

    // The pulse keeps 20 bits for the descriptor and the rest of _NOTIFY_DATA_MASK for the registration generation.
    const NativeHandle too_large{NativeHandle{1} << 20};

    const auto reg = reactor->Register(too_large, ReadEvent(), [](EventSourceHandle, EventTypes) noexcept {});

    ASSERT_FALSE(reg.has_value()) << "a descriptor that does not fit the descriptor bits of the pulse was accepted";
    EXPECT_EQ(reg.error(), IoDispatcherErrc::kInvalidHandle);
}

TEST(IonotifyTest, APulseQueuedByAClosedSocketDoesNotWakeTheSocketReusingItsNumber)
{
    const auto reactor = MakeReactor(4U);
    ASSERT_NE(reactor, nullptr);

    SocketPair old_pair{};
    ASSERT_TRUE(old_pair.Open());
    const int reused_fd{old_pair.ReadFd()};

    const auto old_reg = reactor->Register(reused_fd, ReadEvent(), [](EventSourceHandle, EventTypes) noexcept {});
    ASSERT_TRUE(old_reg.has_value());
    // A wait with no data arms the descriptor, so the next byte queues a pulse instead of being polled.
    ASSERT_TRUE(PumpPasses(*reactor, 1, std::chrono::milliseconds{0}));

    PokeByte(old_pair.WriteFd());
    // The manager queues the pulse on the channel. Nothing receives it before the unregistration.
    std::this_thread::sleep_for(std::chrono::milliseconds{50});
    reactor->Unregister(old_reg.value());

    // A new socket on the same descriptor number, with no data, as a reconnect after a close produces it.
    SocketPair new_pair{};
    ASSERT_TRUE(new_pair.Open());
    old_pair.ReadEnd().Close();
    ASSERT_EQ(::dup2(new_pair.ReadFd(), reused_fd), reused_fd);
    UniqueFd reused_end{reused_fd};

    std::atomic_int new_calls{0};
    const auto new_reg =
        reactor->Register(reused_fd, ReadEvent(), [&new_calls](EventSourceHandle, EventTypes) noexcept {
            ++new_calls;
        });
    ASSERT_TRUE(new_reg.has_value());

    // The first wait arms the new registration and then receives the pulse the old socket queued.
    ASSERT_TRUE(PumpPasses(*reactor, 3, std::chrono::milliseconds{100}));
    EXPECT_EQ(new_calls.load(), 0) << "a pulse of the closed socket woke the socket that reuses its number";

    PokeByte(new_pair.WriteFd());
    for (int i = 0; (i < 4) && (new_calls.load() == 0); ++i)
    {
        ASSERT_TRUE(PumpPasses(*reactor, 1, std::chrono::milliseconds{300}));
    }
    EXPECT_EQ(new_calls.load(), 1) << "the new registration did not dispatch its own data";
    reactor->Unregister(new_reg.value());
}
