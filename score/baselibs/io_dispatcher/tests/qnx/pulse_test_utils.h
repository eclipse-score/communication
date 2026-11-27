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
#ifndef IO_DISPATCHER_TESTS_QNX_PULSE_TEST_UTILS_H
#define IO_DISPATCHER_TESTS_QNX_PULSE_TEST_UTILS_H

/**
 * @file pulse_test_utils.h
 * @brief Reaching the pulse API and putting a pulse on a reactor's channel.
 *
 * Pulses are a QNX event source, so RegisterPulse() and GetConnectionId() sit on IoDispatcherImpl rather than on
 * the interface.
 */

#include "score/io_dispatcher/io_dispatcher.h"
#include "score/io_dispatcher/io_dispatcher_types.h"
#include "score/io_dispatcher/qnx/io_dispatcher_impl.h"

#include <sys/neutrino.h>

#include <cstdint>

namespace qnx_test_utils
{

/**
 * @brief Two codes from the application range, clear of _PULSE_CODE_MINAVAIL (Unblock) and SI_NOTIFY (readiness).
 */
constexpr io_dispatcher::PulseCode kCode{static_cast<io_dispatcher::PulseCode>(_PULSE_CODE_MINAVAIL + 1)};
constexpr io_dispatcher::PulseCode kOtherCode{static_cast<io_dispatcher::PulseCode>(_PULSE_CODE_MINAVAIL + 2)};

/**
 * @brief A pulse value the dispatcher never reads.
 */
constexpr std::int32_t kPayload{0x5A5A};

/**
 * @brief Priority argument that makes a sent pulse inherit the sender's priority.
 */
constexpr std::int32_t kSenderPriority{-1};

/**
 * @brief The dispatcher behind a reactor, for the pulse API the interface does not carry.
 * @pre The QNX backend is the only implementation of IoDispatcher; the downcast is unchecked.
 */
inline io_dispatcher::IoDispatcherImpl& AsImpl(io_dispatcher::IoDispatcher& reactor) noexcept
{
    return static_cast<io_dispatcher::IoDispatcherImpl&>(reactor);
}

inline io_dispatcher::EventTypes PulseEvent(const io_dispatcher::PulseCode code) noexcept
{
    return io_dispatcher::EventTypes{}.SetPulseEvent(true).SetPulseCode(code);
}

/**
 * @brief Sends one pulse to the reactor's channel, the way a producer outside the dispatcher would.
 */
inline bool SendPulse(io_dispatcher::IoDispatcher& reactor,
                      const io_dispatcher::PulseCode code,
                      const std::int32_t value) noexcept
{
    return ::MsgSendPulse(AsImpl(reactor).GetConnectionId(), kSenderPriority, static_cast<int>(code), value) != -1;
}

}  // namespace qnx_test_utils

#endif  // IO_DISPATCHER_TESTS_QNX_PULSE_TEST_UTILS_H
