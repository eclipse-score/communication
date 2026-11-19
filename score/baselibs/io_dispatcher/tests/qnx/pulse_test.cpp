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
 * @file pulse_test.cpp
 * @brief Which pulse registrations RegisterPulse() takes, and what a pulse handle is and is not good for.
 *
 * Pulses are a QNX event source, so RegisterPulse() and GetConnectionId() sit on IoDispatcherImpl, not IoDispatcher.
 */

#include "pulse_test_utils.h"
#include "score/io_dispatcher/io_dispatcher_error.h"
#include "test_utils.h"

#include <gtest/gtest.h>

#include <sys/neutrino.h>

using namespace io_dispatcher;
using namespace test_utils;
using namespace qnx_test_utils;

TEST(PulseRegistrationTest, AMaskWithoutAPulseIsRefused)
{
    const auto reactor = MakeReactor(4U);
    ASSERT_NE(reactor, nullptr);

    const auto reg = AsImpl(*reactor).RegisterPulse(ReadEvent(), [](EventSourceHandle, EventTypes) noexcept {});

    ASSERT_FALSE(reg.has_value()) << "a mask that names no pulse was taken as a pulse registration";
    EXPECT_EQ(reg.error(), IoDispatcherErrc::kApiError);
}

TEST(PulseRegistrationTest, AnEmptyCallbackIsRefused)
{
    const auto reactor = MakeReactor(4U);
    ASSERT_NE(reactor, nullptr);

    const auto reg = AsImpl(*reactor).RegisterPulse(PulseEvent(kCode), CallbackType{});

    ASSERT_FALSE(reg.has_value()) << "a pulse registration without a callback was accepted";
    EXPECT_EQ(reg.error(), IoDispatcherErrc::kApiError);
}

TEST(PulseRegistrationTest, ADuplicateCodeIsRefused)
{
    const auto reactor = MakeReactor(4U);
    ASSERT_NE(reactor, nullptr);

    ASSERT_TRUE(
        AsImpl(*reactor).RegisterPulse(PulseEvent(kCode), [](EventSourceHandle, EventTypes) noexcept {}).has_value());

    const auto second =
        AsImpl(*reactor).RegisterPulse(PulseEvent(kCode), [](EventSourceHandle, EventTypes) noexcept {});

    ASSERT_FALSE(second.has_value()) << "one pulse code took two callbacks";
    EXPECT_EQ(second.error(), IoDispatcherErrc::kApiError);
}

TEST(PulseRegistrationTest, DistinctCodesGetDistinctHandles)
{
    const auto reactor = MakeReactor(4U);
    ASSERT_NE(reactor, nullptr);

    const auto one = AsImpl(*reactor).RegisterPulse(PulseEvent(kCode), [](EventSourceHandle, EventTypes) noexcept {});
    const auto other =
        AsImpl(*reactor).RegisterPulse(PulseEvent(kOtherCode), [](EventSourceHandle, EventTypes) noexcept {});
    ASSERT_TRUE(one.has_value());
    ASSERT_TRUE(other.has_value());

    EXPECT_NE(one.value(), other.value());
}

TEST(PulseRegistrationTest, TheCodeUnblockReservesIsRefused)
{
    const auto reactor = MakeReactor(4U);
    ASSERT_NE(reactor, nullptr);

    const auto reg = AsImpl(*reactor).RegisterPulse(PulseEvent(static_cast<PulseCode>(_PULSE_CODE_MINAVAIL)),
                                                    [](EventSourceHandle, EventTypes) noexcept {});

    ASSERT_FALSE(reg.has_value()) << "the code Unblock() sends was handed to an application callback";
    EXPECT_EQ(reg.error(), IoDispatcherErrc::kApiError);
}

TEST(PulseRegistrationTest, TheCodeReadinessNotificationsUseIsRefused)
{
    const auto reactor = MakeReactor(4U);
    ASSERT_NE(reactor, nullptr);

    const auto reg = AsImpl(*reactor).RegisterPulse(PulseEvent(static_cast<PulseCode>(SI_NOTIFY)),
                                                    [](EventSourceHandle, EventTypes) noexcept {});

    ASSERT_FALSE(reg.has_value()) << "the code descriptor readiness arrives with was handed to an application callback";
    EXPECT_EQ(reg.error(), IoDispatcherErrc::kApiError);
}

TEST(PulseRegistrationTest, TheHighestApplicationCodeIsAccepted)
{
    const auto reactor = MakeReactor(4U);
    ASSERT_NE(reactor, nullptr);

    EXPECT_TRUE(AsImpl(*reactor)
                    .RegisterPulse(PulseEvent(static_cast<PulseCode>(_PULSE_CODE_MAXAVAIL)),
                                   [](EventSourceHandle, EventTypes) noexcept {})
                    .has_value());
}

TEST(PulseRegistrationTest, AFullReactorRefusesAPulseRegistration)
{
    const auto reactor = MakeReactor(1U);
    ASSERT_NE(reactor, nullptr);

    ASSERT_TRUE(
        AsImpl(*reactor).RegisterPulse(PulseEvent(kCode), [](EventSourceHandle, EventTypes) noexcept {}).has_value());

    const auto refused =
        AsImpl(*reactor).RegisterPulse(PulseEvent(kOtherCode), [](EventSourceHandle, EventTypes) noexcept {});

    ASSERT_FALSE(refused.has_value()) << "a pulse registration slipped past the reactor's capacity";
    EXPECT_EQ(refused.error(), IoDispatcherErrc::kResource);
}

TEST(PulseRegistrationTest, APulseHandleHasNoMonitoredEvents)
{
    const auto reactor = MakeReactor(4U);
    ASSERT_NE(reactor, nullptr);

    const auto reg = AsImpl(*reactor).RegisterPulse(PulseEvent(kCode), [](EventSourceHandle, EventTypes) noexcept {});
    ASSERT_TRUE(reg.has_value());

    const auto result = reactor->SetMonitoredEvents(reg.value(), ReadEvent());

    ASSERT_FALSE(result.has_value()) << "a pulse registration accepted a descriptor's event mask";
    EXPECT_EQ(result.error(), IoDispatcherErrc::kApiError);
}
