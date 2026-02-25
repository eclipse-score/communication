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
#include "score/mw/com/impl/bindings/lola/messaging/mw_log_logger.h"

#include "score/memory/shared/pointer_arithmetic_util.h"
#include "score/mw/log/logging.h"

#include <score/utility.hpp>

namespace score::mw::com::impl::lola
{

score::message_passing::LoggingCallback GetMwLogLogger()
{
    return [](score::message_passing::LogSeverity severity, score::message_passing::LogItems items) -> void {
        using LogStreamFactoryPtr = score::mw::log::LogStream (*)(std::string_view) noexcept;
        constexpr auto kLogLevels = score::cpp::to_underlying(score::message_passing::LogSeverity::kVerbose) + 1U;
        constexpr std::array<LogStreamFactoryPtr, kLogLevels> StreamFactories{{
            &score::mw::log::LogFatal,
            &score::mw::log::LogError,
            &score::mw::log::LogWarn,
            &score::mw::log::LogWarn,  // LogInfo - temporary replacement for Ticket-235378 debugging
            &score::mw::log::LogWarn,  // LogDebug - temporary replacement for Ticket-235378 debugging
            &score::mw::log::LogWarn   // LogVerbose - temporary replacement for Ticket-235378 debugging
        }};
        auto severity_num = score::cpp::to_underlying(severity);
        if (severity_num >= kLogLevels)
        {
            return;
        }
        score::mw::log::LogStream stream = (*StreamFactories.at(severity_num))("mp_2");
        for (auto& item : items)
        {
            std::visit(
                [&stream](auto&& arg) {
                    using T = std::decay_t<decltype(arg)>;
                    // Suppress "AUTOSAR C++14 A7-1-8", The rule states: "A non-type specifier shall be placed before
                    // a type specifier in a declaration.".
                    // Rationale: False positive: if constexpr is the required C++17 construct here.
                    // coverity[autosar_cpp14_a7_1_8_violation: FALSE]
                    if constexpr (std::is_same_v<T, const void*>)
                    {
                        stream << score::memory::shared::PointerToLogValue(arg);
                    }
                    else
                    {
                        stream << arg;
                    }
                },
                item);
        }
    };
}

}  // namespace score::mw::com::impl::lola
