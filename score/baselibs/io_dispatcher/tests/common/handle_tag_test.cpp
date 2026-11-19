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
 * @file handle_tag_test.cpp
 * @brief The DispatcherTag a reactor stamps into its handles, and that a tagged handle still names its registration.
 *
 * @note A handle the backend rebuilds on dispatch without the tag misses its registration, so each case that matters
 *       for routing drives a real event through the reactor.
 */

#include "score/io_dispatcher/io_dispatcher_error.h"
#include "test_utils.h"

#if defined(__QNX__)
#include "qnx/pulse_test_utils.h"
#endif

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <utility>

using namespace io_dispatcher;
using namespace test_utils;

namespace
{

// Tag 7 sets all three tag bits, so a mask that drops one of them shows up there first.
constexpr DispatcherTag kTagsUnderTest[]{1U, 5U, kMaxDispatcherTag};

/**
 * @brief What a callback was last told, so a test can compare the handle it saw with the one Register() returned.
 */
struct Seen
{
    std::atomic_bool fired{false};
    std::atomic<std::uint32_t> count{0U};
    std::atomic<EventSourceHandle> handle{kInvalidEventSourceHandle};
};

CallbackType Recorder(Seen& seen) noexcept
{
    return [&seen](const EventSourceHandle handle, EventTypes) noexcept {
        seen.handle = handle;
        ++seen.count;
        seen.fired = true;
    };
}

/**
 * @brief Records like Recorder() and reads the byte that made `fd` readable, so it fires once per poke.
 */
CallbackType DrainingRecorder(Seen& seen, const int fd) noexcept
{
    return [&seen, fd](const EventSourceHandle handle, EventTypes) noexcept {
        DrainByte(fd);
        seen.handle = handle;
        ++seen.count;
        seen.fired = true;
    };
}

}  // namespace

// io_dispatcher_types.h: bits 48..50 of every handle carry the issuing dispatcher's DispatcherTag.
TEST(HandleTagTest, EveryTagUpToTheMaximumIsCarriedByTheHandlesItsReactorIssues)
{
    for (DispatcherTag tag{0U}; tag <= kMaxDispatcherTag; ++tag)
    {
        SCOPED_TRACE(static_cast<int>(tag));
        const auto reactor = MakeReactor(4U, tag);
        ASSERT_NE(reactor, nullptr) << "a tag within range was refused";

        SocketPair first{};
        SocketPair second{};
        ASSERT_TRUE(first.Open());
        ASSERT_TRUE(second.Open());

        const auto one = reactor->Register(first.ReadFd(), ReadEvent(), [](EventSourceHandle, EventTypes) noexcept {});
        const auto other =
            reactor->Register(second.ReadFd(), ReadEvent(), [](EventSourceHandle, EventTypes) noexcept {});
        ASSERT_TRUE(one.has_value());
        ASSERT_TRUE(other.has_value());

        EXPECT_EQ(DispatcherTagOf(one.value()), tag);
        EXPECT_EQ(DispatcherTagOf(other.value()), tag);
        EXPECT_NE(one.value(), other.value()) << "the tag overwrote the part that tells registrations apart";
        reactor->Unregister(one.value());
        reactor->Unregister(other.value());
    }
}

// io_dispatcher_factory.h: the tag defaults to 0.
TEST(HandleTagTest, TheUntaggedFactoryIssuesTagZero)
{
    const auto reactor = MakeReactor(4U);
    ASSERT_NE(reactor, nullptr);

    SocketPair pair{};
    ASSERT_TRUE(pair.Open());

    const auto reg = reactor->Register(pair.ReadFd(), ReadEvent(), [](EventSourceHandle, EventTypes) noexcept {});
    ASSERT_TRUE(reg.has_value());

    EXPECT_EQ(DispatcherTagOf(reg.value()), 0U);
    reactor->Unregister(reg.value());
}

// io_dispatcher_factory.h: a tag above kMaxDispatcherTag returns kApiError.
TEST(HandleTagTest, ATagAboveTheMaximumIsRefusedWithApiError)
{
    for (const DispatcherTag tag : {static_cast<DispatcherTag>(kMaxDispatcherTag + 1U), DispatcherTag{255U}})
    {
        SCOPED_TRACE(static_cast<int>(tag));
        const auto reactor = MakeIoDispatcher(4U, tag);

        ASSERT_FALSE(reactor.has_value()) << "a tag that does not fit bits 48..50 was taken";
        EXPECT_EQ(reactor.error(), IoDispatcherErrc::kApiError);
    }
}

// io_dispatcher_types.h: the tag is part of the handle, so the handle a callback receives is the one Register()
// returned.
TEST(HandleTagTest, ACallbackOnAReadableSourceIsToldTheTaggedHandleRegisterReturned)
{
    for (const DispatcherTag tag : kTagsUnderTest)
    {
        SCOPED_TRACE(static_cast<int>(tag));
        const auto reactor = MakeReactor(4U, tag);
        ASSERT_NE(reactor, nullptr);

        SocketPair pair{};
        ASSERT_TRUE(pair.Open());

        Seen seen{};
        const auto reg = reactor->Register(pair.ReadFd(), ReadEvent(), DrainingRecorder(seen, pair.ReadFd()));
        ASSERT_TRUE(reg.has_value());

        PokeByte(pair.WriteFd());
        ASSERT_TRUE(PumpUntil(*reactor, seen.fired, 4)) << "a readable source of a tagged reactor was never dispatched";

        EXPECT_EQ(seen.handle.load(), reg.value()) << "the callback was told a handle Register() never returned";
        EXPECT_EQ(DispatcherTagOf(seen.handle.load()), tag);
        reactor->Unregister(reg.value());
    }
}

// io_dispatcher.h: SetMonitoredEvents() takes the handle Register() returned, tag included.
TEST(HandleTagTest, SetMonitoredEventsAcceptsATaggedHandle)
{
    for (const DispatcherTag tag : kTagsUnderTest)
    {
        SCOPED_TRACE(static_cast<int>(tag));
        const auto reactor = MakeReactor(4U, tag);
        ASSERT_NE(reactor, nullptr);

        SocketPair pair{};
        ASSERT_TRUE(pair.Open());

        Seen seen{};
        const auto reg = reactor->Register(pair.ReadFd(), EmptyEvent(), DrainingRecorder(seen, pair.ReadFd()));
        ASSERT_TRUE(reg.has_value());

        const auto result = reactor->SetMonitoredEvents(reg.value(), ReadEvent());
        ASSERT_TRUE(result.has_value()) << "the tagged handle was not recognised";

        PokeByte(pair.WriteFd());
        EXPECT_TRUE(PumpUntil(*reactor, seen.fired, 4)) << "the new mask did not reach the registration";
        reactor->Unregister(reg.value());
    }
}

// io_dispatcher.h: SetCallbackTarget() takes the handle Register() returned, tag included.
TEST(HandleTagTest, SetCallbackTargetAcceptsATaggedHandle)
{
    for (const DispatcherTag tag : kTagsUnderTest)
    {
        SCOPED_TRACE(static_cast<int>(tag));
        const auto reactor = MakeReactor(4U, tag);
        ASSERT_NE(reactor, nullptr);

        SocketPair pair{};
        ASSERT_TRUE(pair.Open());

        Seen replaced{};
        Seen target{};
        const auto reg = reactor->Register(pair.ReadFd(), ReadEvent(), DrainingRecorder(replaced, pair.ReadFd()));
        ASSERT_TRUE(reg.has_value());

        const auto result = reactor->SetCallbackTarget(reg.value(), DrainingRecorder(target, pair.ReadFd()));
        ASSERT_TRUE(result.has_value()) << "the tagged handle was not recognised";

        PokeByte(pair.WriteFd());
        EXPECT_TRUE(PumpUntil(*reactor, target.fired, 4)) << "the new callback was never run";
        EXPECT_FALSE(replaced.fired.load()) << "the replaced callback still ran";
        reactor->Unregister(reg.value());
    }
}

// io_dispatcher.h: Unregister() with the tagged handle withdraws the registration.
TEST(HandleTagTest, UnregisterWithATaggedHandleStopsDispatch)
{
    for (const DispatcherTag tag : kTagsUnderTest)
    {
        SCOPED_TRACE(static_cast<int>(tag));
        const auto reactor = MakeReactor(4U, tag);
        ASSERT_NE(reactor, nullptr);

        SocketPair pair{};
        ASSERT_TRUE(pair.Open());

        Seen seen{};
        const auto reg = reactor->Register(pair.ReadFd(), ReadEvent(), Recorder(seen));
        ASSERT_TRUE(reg.has_value());

        reactor->Unregister(reg.value());

        PokeByte(pair.WriteFd());
        ASSERT_TRUE(PumpPasses(*reactor, 2, std::chrono::milliseconds{100}));
        EXPECT_FALSE(seen.fired.load()) << "the tagged handle did not withdraw its registration";
    }
}

// Error guessing: the tag takes three bits from the part that tells a reused slot's registrations apart, so a handle
// left over from the slot's previous registration must still be refused.
TEST(HandleTagTest, AStaleHandleOfAReusedSlotIsRefusedOnATaggedReactor)
{
    const auto reactor = MakeReactor(1U, kMaxDispatcherTag);
    ASSERT_NE(reactor, nullptr);

    SocketPair first{};
    SocketPair second{};
    ASSERT_TRUE(first.Open());
    ASSERT_TRUE(second.Open());

    const auto stale = reactor->Register(first.ReadFd(), ReadEvent(), [](EventSourceHandle, EventTypes) noexcept {});
    ASSERT_TRUE(stale.has_value());
    reactor->Unregister(stale.value());
    const auto live = reactor->Register(second.ReadFd(), ReadEvent(), [](EventSourceHandle, EventTypes) noexcept {});
    ASSERT_TRUE(live.has_value()) << "the only slot was not freed";

    EXPECT_NE(live.value(), stale.value());
    EXPECT_EQ(DispatcherTagOf(live.value()), kMaxDispatcherTag);
    const auto result = reactor->SetMonitoredEvents(stale.value(), WriteEvent());
    ASSERT_FALSE(result.has_value()) << "a handle of the slot's previous registration reached the new one";
    EXPECT_EQ(result.error(), IoDispatcherErrc::kApiError);
    reactor->Unregister(live.value());
}

#if defined(__QNX__)

// io_dispatcher_types.h, qnx/io_dispatcher_impl.h: pulse handles carry the tag too, and a pulse callback is told the
// handle RegisterPulse() returned.
TEST(HandleTagTest, APulseRegistrationCarriesTheTagAndItsCallbackIsToldItsHandle)
{
    for (const DispatcherTag tag : kTagsUnderTest)
    {
        SCOPED_TRACE(static_cast<int>(tag));
        const auto reactor = MakeReactor(4U, tag);
        ASSERT_NE(reactor, nullptr);

        Seen seen{};
        const auto reg = qnx_test_utils::AsImpl(*reactor).RegisterPulse(
            qnx_test_utils::PulseEvent(qnx_test_utils::kCode), Recorder(seen));
        ASSERT_TRUE(reg.has_value());
        EXPECT_EQ(DispatcherTagOf(reg.value()), tag);

        ASSERT_TRUE(qnx_test_utils::SendPulse(*reactor, qnx_test_utils::kCode, qnx_test_utils::kPayload));
        ASSERT_TRUE(PumpUntil(*reactor, seen.fired, 4)) << "a pulse to a tagged reactor never reached its callback";

        EXPECT_EQ(seen.handle.load(), reg.value())
            << "the pulse callback was told a handle RegisterPulse() never returned";
        reactor->Unregister(reg.value());
    }
}

#endif
