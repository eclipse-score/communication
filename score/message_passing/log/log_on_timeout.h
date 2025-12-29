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
#ifndef SCORE_LIB_MESSAGE_PASSING_LOG_LOG_ON_TIMEOUT_H
#define SCORE_LIB_MESSAGE_PASSING_LOG_LOG_ON_TIMEOUT_H

#include "score/message_passing/log/log.h"

#include <chrono>
#include <type_traits>

namespace score::message_passing
{

template <LogSeverity severity, typename... Args>
class LogOnTimeoutBase
{
  public:
    // to avoid the lifetime problems with temporary objects, the arguments are copied (passed by value)
    // to avoid copying, use std::ref() or std::cref(), but pay attention to lifetime issues
    LogOnTimeoutBase(const LoggingCallback& logger, std::chrono::milliseconds timeout, Args... args)
        : logger_{logger}, timeout_{timeout}, capture_{std::make_tuple(args...)}
    {
        started_ = UnderlyingClock::now();
    }

    LogOnTimeoutBase(const LogOnTimeoutBase&) = delete;
    LogOnTimeoutBase(LogOnTimeoutBase&&) = delete;
    LogOnTimeoutBase& operator=(const LogOnTimeoutBase&) = delete;
    LogOnTimeoutBase& operator=(LogOnTimeoutBase&&) = delete;

    void release()
    {
        if (started_ != time_point{})
        {
            const time_point now = UnderlyingClock::now();
            if (now > started_ + timeout_)
            {
                std::apply(
                    [this, now](auto... tuple_args) {
                        const auto extra_ms = (now - (started_ + timeout_)).count();
                        // we explicitly and separately create std::string_view arguments
                        // to avoid "Decision couldn't be analyzed" coverage false positive in the last line

                        // Suppress "AUTOSAR C++14 A7-1-2" rule. The rule declares:
                        // The constexpr specifier shall be used for values that can be determined at compile time.
                        // True positive, but making them constexpr crashes the compiler.
                        // coverity[autosar_cpp14_a7_1_2_violation]
                        const std::string_view str1{"Time exceeded by "};
                        // coverity[autosar_cpp14_a7_1_2_violation]
                        const std::string_view str2{" ms for "};
                        Log<severity>(logger_, str1, extra_ms, str2, tuple_args...);
                    },
                    capture_);
            }
            started_ = time_point{};
        }
    }

    // A12-4-1 requirement (superfluous in our case, but easy to meet)
  protected:
    ~LogOnTimeoutBase()
    {
        release();
    }

  private:
// copied from score::os::HighResolutionSteadyClock, which is deprecated, but it would be weird to use mw::time in lib

// Suppress "AUTOSAR C++14 A16-0-1" rule findings. This rule stated: "The pre-processor shall only be used for
// unconditional and conditional file inclusion and include guards, and using the following directives: (1) #ifndef,
// #ifdef, (3) #if, (4) #if defined, (5) #elif, (6) #else, (7) #define, (8) #endif, (9) #include.".
// Rationale: Pre-processor commands are used to allow different implementations for linux and QNX to exist
// in the same file. It also prevents compiler errors in linux code when compiling for QNX and vice versa.
// coverity[autosar_cpp14_a16_0_1_violation]
#ifdef __QNX__
    using UnderlyingClock = std::chrono::high_resolution_clock;
// coverity[autosar_cpp14_a16_0_1_violation], see above rationale
#else
    using UnderlyingClock = std::chrono::steady_clock;
// coverity[autosar_cpp14_a16_0_1_violation], see above rationale
#endif

    using time_point = UnderlyingClock::time_point;

    const LoggingCallback& logger_;
    std::chrono::milliseconds timeout_;
    std::tuple<Args...> capture_;
    time_point started_;
};

template <typename... Args>
class LogWarnOnTimeout : public LogOnTimeoutBase<LogSeverity::kWarn, Args...>
{
  public:
    // funny way to avoid A14-1-6
    using LogOnTimeoutBase<LogSeverity::kWarn, Args...>::LogOnTimeoutBase;
};

// funny way to avoid A14-1-6
template <typename... Args>
LogWarnOnTimeout(const LoggingCallback& logger, std::chrono::milliseconds timeout, Args... args)
    -> LogWarnOnTimeout<Args...>;

}  // namespace score::message_passing

#endif  // SCORE_LIB_MESSAGE_PASSING_LOG_LOG_ON_TIMEOUT_H
