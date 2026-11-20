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
 * @file backend_test.cpp
 * @brief Behavior the poll backend has and the QNX one does not, so it cannot be asserted from the shared suite.
 */

#include "score/io_dispatcher/io_dispatcher_error.h"
#include "score/io_dispatcher/io_dispatcher_factory.h"
#include "test_utils.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>

using namespace io_dispatcher;
using namespace test_utils;

namespace
{

/**
 * @brief The most sources the handle encoding leaves room for; one value is reserved for the unblock.
 */
constexpr std::uint16_t kSlotLimit{8191U};

}  // namespace

TEST(LinuxBackendTest, TheSlotLimitItselfIsAcceptedAndOneMoreIsNot)
{
    EXPECT_EQ(MaxNumEventSources(), kSlotLimit) << "the reported maximum is not the slot limit";
    EXPECT_TRUE(MakeIoDispatcher(kSlotLimit).has_value()) << "the ceiling itself was refused";

    const auto above = MakeIoDispatcher(kSlotLimit + 1U);
    ASSERT_FALSE(above.has_value()) << "a reactor was built past what its handles can name";
    EXPECT_EQ(above.error(), IoDispatcherErrc::kResource);
}

TEST(LinuxBackendTest, TheTargetCannotBeReplacedWhileItIsRunning)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    SocketPair pair{};
    ASSERT_TRUE(pair.Open());

    const int read_fd{pair.ReadFd()};
    std::atomic_bool attempted{false};
    std::atomic_bool refused{false};
    ASSERT_TRUE(reactor
                    ->Register(read_fd,
                               ReadEvent(),
                               [&](const EventSourceHandle handle, EventTypes) noexcept {
                                   DrainByte(read_fd);
                                   const auto result = reactor->SetCallbackTarget(
                                       handle, [](EventSourceHandle, EventTypes) noexcept {});
                                   refused = (!result.has_value()) && (result.error() == IoDispatcherErrc::kApiError);
                                   attempted = true;
                               })
                    .has_value());

    PokeByte(pair.WriteFd());
    ASSERT_TRUE(PumpUntil(*reactor, attempted, 4)) << "the callback never ran";

    EXPECT_TRUE(refused.load()) << "a callback replaced the target it was running out of";
}

TEST(LinuxBackendTest, ASlotItsOwnCallbackFreedIsNotHandedOnUntilTheCallbackReturns)
{
    const auto reactor = MakeReactor(1U);
    ASSERT_NE(reactor, nullptr);

    SocketPair first{};
    SocketPair second{};
    ASSERT_TRUE(first.Open());
    ASSERT_TRUE(second.Open());

    const int first_read_fd{first.ReadFd()};
    const int second_read_fd{second.ReadFd()};
    std::atomic_bool attempted{false};
    std::atomic_bool refused{false};
    ASSERT_TRUE(reactor
                    ->Register(first_read_fd,
                               ReadEvent(),
                               [&](const EventSourceHandle handle, EventTypes) noexcept {
                                   DrainByte(first_read_fd);
                                   reactor->Unregister(handle);
                                   // The reactor's only slot is free, but the callback that held it is still running.
                                   const auto result = reactor->Register(
                                       second_read_fd, ReadEvent(), [](EventSourceHandle, EventTypes) noexcept {});
                                   refused = (!result.has_value()) && (result.error() == IoDispatcherErrc::kResource);
                                   attempted = true;
                               })
                    .has_value());

    PokeByte(first.WriteFd());
    ASSERT_TRUE(PumpUntil(*reactor, attempted, 4)) << "the callback never ran";

    EXPECT_TRUE(refused.load()) << "a new callback was moved over one that was still running";

    EXPECT_TRUE(
        reactor->Register(second_read_fd, ReadEvent(), [](EventSourceHandle, EventTypes) noexcept {}).has_value())
        << "the slot stayed refused after its callback returned";
}

TEST(LinuxBackendTest, AHangUpKeepsBeingReported)
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

    pair.WriteEnd().Close();
    ASSERT_TRUE(PumpPasses(*reactor, 3, std::chrono::milliseconds{100}));

    EXPECT_GE(invocations.load(), 2U) << "a hang-up was reported once and then forgotten";
}

TEST(LinuxBackendTest, AHangUpReachesAWatcherThatAskedOnlyForWrite)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    SocketPair pair{};
    ASSERT_TRUE(pair.Open());

    std::atomic_bool fired{false};
    std::atomic_bool got_write{false};
    ASSERT_TRUE(reactor
                    ->Register(pair.ReadFd(),
                               WriteEvent(),
                               [&](EventSourceHandle, const EventTypes events) noexcept {
                                   got_write = events.HasWriteEvent();
                                   fired = true;
                               })
                    .has_value());

    pair.WriteEnd().Close();

    ASSERT_TRUE(PumpUntil(*reactor, fired, 4)) << "the hang-up never reached the write watcher";
    EXPECT_TRUE(got_write.load());
}

TEST(LinuxBackendTest, ADescriptorClosedBehindTheReactorDoesNotDispatch)
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

    // The registration is left behind on purpose: poll reports the slot as invalid rather than as ready.
    pair.ReadEnd().Close();
    ASSERT_TRUE(PumpPasses(*reactor, 3, std::chrono::milliseconds{50}));

    EXPECT_FALSE(fired.load()) << "a descriptor that is no longer open was dispatched on";
}
