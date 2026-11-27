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
 * @file registration_test.cpp
 * @brief Which sources Register() accepts, which it refuses, and what the registration it returns dispatches.
 *
 * @note EventSourceHandle is opaque: two live registrations differ, and nothing else about a handle is asserted here.
 */

#include "score/io_dispatcher/io_dispatcher_error.h"
#include "test_utils.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>

using namespace io_dispatcher;
using namespace test_utils;

TEST(RegistrationTest, ASourceIsAcceptedForEachEventMask)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    for (const EventTypes events : {ReadEvent(), WriteEvent(), ReadWriteEvent()})
    {
        SocketPair pair{};
        ASSERT_TRUE(pair.Open());

        const auto reg = reactor->Register(pair.ReadFd(), events, [](EventSourceHandle, EventTypes) noexcept {});
        ASSERT_TRUE(reg.has_value()) << "the mask read=" << events.HasReadEvent() << " write=" << events.HasWriteEvent()
                                     << " was refused";
        reactor->Unregister(reg.value());
    }
}

TEST(RegistrationTest, TwoLiveSourcesGetDistinctHandles)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    SocketPair first{};
    SocketPair second{};
    ASSERT_TRUE(first.Open());
    ASSERT_TRUE(second.Open());

    const auto one = reactor->Register(first.ReadFd(), ReadEvent(), [](EventSourceHandle, EventTypes) noexcept {});
    const auto other = reactor->Register(second.ReadFd(), ReadEvent(), [](EventSourceHandle, EventTypes) noexcept {});
    ASSERT_TRUE(one.has_value());
    ASSERT_TRUE(other.has_value());

    EXPECT_NE(one.value(), other.value()) << "two sources registered at once share a handle";
}

TEST(RegistrationTest, ASecondCallbackForTheSameSourceIsRefused)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    SocketPair pair{};
    ASSERT_TRUE(pair.Open());

    const auto first = reactor->Register(pair.ReadFd(), ReadEvent(), [](EventSourceHandle, EventTypes) noexcept {});
    ASSERT_TRUE(first.has_value());

    const auto second = reactor->Register(pair.ReadFd(), WriteEvent(), [](EventSourceHandle, EventTypes) noexcept {});
    ASSERT_FALSE(second.has_value()) << "one descriptor took two callbacks";
    EXPECT_EQ(second.error(), IoDispatcherErrc::kApiError);
}

TEST(RegistrationTest, ANegativeDescriptorIsRefusedAsAnInvalidHandle)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    for (const NativeHandle io_source : {kInvalidNativeHandle, NativeHandle{-2}})
    {
        const auto reg = reactor->Register(io_source, ReadEvent(), [](EventSourceHandle, EventTypes) noexcept {});
        ASSERT_FALSE(reg.has_value()) << "the descriptor " << io_source << " was accepted";
        EXPECT_EQ(reg.error(), IoDispatcherErrc::kInvalidHandle);
    }
}

TEST(RegistrationTest, AnEmptyCallbackIsRefusedAndTakesNoSlot)
{
    const auto reactor = MakeReactor(1U);
    ASSERT_NE(reactor, nullptr);

    SocketPair pair{};
    ASSERT_TRUE(pair.Open());

    const auto empty = reactor->Register(pair.ReadFd(), ReadEvent(), CallbackType{});
    ASSERT_FALSE(empty.has_value()) << "a registration without a callback was accepted";
    EXPECT_EQ(empty.error(), IoDispatcherErrc::kApiError);

    // The single slot is still free, so the refused registration left nothing behind.
    const auto reg = reactor->Register(pair.ReadFd(), ReadEvent(), [](EventSourceHandle, EventTypes) noexcept {});
    EXPECT_TRUE(reg.has_value()) << "the refused registration kept the descriptor or the slot";
}

TEST(RegistrationTest, ASourceRegistersAgainOnceItIsUnregistered)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    SocketPair pair{};
    ASSERT_TRUE(pair.Open());

    const auto first = reactor->Register(pair.ReadFd(), ReadEvent(), [](EventSourceHandle, EventTypes) noexcept {});
    ASSERT_TRUE(first.has_value());
    reactor->Unregister(first.value());

    std::atomic_bool fired{false};
    const auto second = reactor->Register(pair.ReadFd(), ReadEvent(), [&fired](EventSourceHandle, EventTypes) noexcept {
        fired = true;
    });
    ASSERT_TRUE(second.has_value());

    PokeByte(pair.WriteFd());
    EXPECT_TRUE(PumpUntil(*reactor, fired, 4))
        << "the descriptor registered again but its new registration never dispatched";
}

TEST(RegistrationTest, AnEmptyEventMaskRegistersAndNeverDispatches)
{
    const auto reactor = MakeReactor(8U);
    ASSERT_NE(reactor, nullptr);

    SocketPair pair{};
    ASSERT_TRUE(pair.Open());

    std::atomic_bool fired{false};
    const auto reg = reactor->Register(pair.ReadFd(), EmptyEvent(), [&fired](EventSourceHandle, EventTypes) noexcept {
        fired = true;
    });
    ASSERT_TRUE(reg.has_value()) << "a source nobody is interested in yet was refused";

    PokeByte(pair.WriteFd());
    DrainWakeup(*reactor);
    const auto result = reactor->HandleEvents(std::chrono::milliseconds{100});

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), UnblockReason::kTimeout);
    EXPECT_FALSE(fired.load()) << "an empty mask dispatched";
}
