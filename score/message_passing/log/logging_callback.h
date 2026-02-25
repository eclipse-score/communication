/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
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
#ifndef SCORE_LIB_MESSAGE_PASSING_LOG_LOGGING_CALLBACK_H
#define SCORE_LIB_MESSAGE_PASSING_LOG_LOGGING_CALLBACK_H

#include <score/callback.hpp>
#include <score/span.hpp>

#include <string_view>
#include <variant>

namespace score::message_passing
{

enum class LogSeverity : std::uint8_t
{
    kFatal = 0x00,
    kError = 0x01,
    kWarn = 0x02,
    kInfo = 0x03,
    kDebug = 0x04,
    kVerbose = 0x05,
};

using LogItem = std::variant<std::string_view, std::int64_t, std::uint64_t, const void*>;
using LogItems = score::cpp::span<const LogItem>;
using LoggingCallback = score::cpp::callback<void(LogSeverity, LogItems)>;

LoggingCallback GetCerrLogger();

}  // namespace score::message_passing

#endif  // SCORE_LIB_MESSAGE_PASSING_LOG_LOGGING_CALLBACK_H
