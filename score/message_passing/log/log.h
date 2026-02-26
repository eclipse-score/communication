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
#ifndef SCORE_LIB_MESSAGE_PASSING_LOG_LOG_H
#define SCORE_LIB_MESSAGE_PASSING_LOG_LOG_H

#include "score/message_passing/log/logging_callback.h"

#include <type_traits>

namespace score::message_passing
{

inline std::string_view LogConvert(std::string_view t)
{
    return t;
}

template <class T, typename std::enable_if<std::is_unsigned_v<T>, bool>::type = true>
std::uint64_t LogConvert(const T& t)
{
    return static_cast<std::uint64_t>(t);
}

template <class T, typename std::enable_if<std::is_signed_v<T>, bool>::type = true>
std::int64_t LogConvert(const T& t)
{
    return static_cast<std::int64_t>(t);
}

template <class T, typename std::enable_if<!std::is_same_v<T, const char>, bool>::type = true>
const void* LogConvert(T* const p)
{
    return p;
}

inline const void* LogConvert(const std::nullptr_t p)
{
    return p;
}

template <class T>
auto LogConvert(std::reference_wrapper<T> r) -> decltype(LogConvert(r.get()))
{
    return LogConvert(r.get());
}

template <typename... Args>
auto LogConvertAll(Args&&... args) -> std::array<LogItem, sizeof...(Args)>
{
    return std::array<LogItem, sizeof...(Args)>{LogConvert(std::forward<Args>(args))...};
}

// The severity is a template argument to reserve a way to switch off selected severitites at compile time
template <LogSeverity severity, typename... Args>
void Log(const LoggingCallback& logger, Args&&... args)
{
    if (!logger.empty())
    {
        auto log_converted = LogConvertAll(std::forward<Args>(args)...);
        logger(severity, log_converted);
    }
}

template <typename... Args>
void LogFatal(const LoggingCallback& logger, Args&&... args)
{
    Log<LogSeverity::kFatal>(logger, std::forward<Args>(args)...);
}

template <typename... Args>
void LogError(const LoggingCallback& logger, Args&&... args)
{
    Log<LogSeverity::kError>(logger, std::forward<Args>(args)...);
}

template <typename... Args>
void LogWarn(const LoggingCallback& logger, Args&&... args)
{
    Log<LogSeverity::kWarn>(logger, std::forward<Args>(args)...);
}

template <typename... Args>
void LogInfo(const LoggingCallback& logger, Args&&... args)
{
    Log<LogSeverity::kInfo>(logger, std::forward<Args>(args)...);
}

template <typename... Args>
void LogDebug(const LoggingCallback& logger, Args&&... args)
{
    Log<LogSeverity::kDebug>(logger, std::forward<Args>(args)...);
}

template <typename... Args>
void LogVerbose(const LoggingCallback& logger, Args&&... args)
{
    Log<LogSeverity::kVerbose>(logger, std::forward<Args>(args)...);
}

}  // namespace score::message_passing

#endif  // SCORE_LIB_MESSAGE_PASSING_LOG_LOG_H
