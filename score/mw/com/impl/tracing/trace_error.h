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
#ifndef SCORE_MW_COM_IMPL_TRACING_CONFIGURATION_TRACE_CONFIG_ERROR_H
#define SCORE_MW_COM_IMPL_TRACING_CONFIGURATION_TRACE_CONFIG_ERROR_H

#include "score/result/result.h"

namespace score::mw::com::impl::tracing
{

/// \brief error codes, which can occur, when trying to parse a tracing filter config json and creating a
///        TracingFilterConfig from it.
enum class TraceErrorCode : score::result::ErrorCode
{
    JsonConfigParseError = 1,
    TraceErrorDisableAllTracePoints = 2,
    TraceErrorDisableTracePointInstance = 3
};

/// \brief See above explanation in TraceErrorCode
class TraceErrorDomain final : public score::result::ErrorDomain
{
  public:
    std::string_view MessageFor(const score::result::ErrorCode& code) const noexcept override
    {
        // Suppress "AUTOSAR C++14 A7-2-1", The rule states: "An expression with enum underlying type
        // shall only have values corresponding to the enumerators of the enumeration."
        // Suppressed for the following reason: Any other values do not correspond to
        // one of the enumerators of the enumeration will be handled with the default case.
        // coverity[autosar_cpp14_a7_2_1_violation]
        const auto error_code = static_cast<TraceErrorCode>(code);
        // Suppress "AUTOSAR C++14 M6-4-3" rule finding. This rule declares: "A switch statement shall be
        // a well-formed switch statement".
        // We don't need a break statement at the end of default case as we use return.
        // coverity[autosar_cpp14_m6_4_3_violation]
        switch (error_code)
        {
            // Suppress "AUTOSAR C++14 M6-4-5" rule finding. This rule declares: "An unconditional throw
            // or break statement shall terminate every non-empty switch-clause".
            // We don't need a break statement at the end of default case as we use return.
            // An error return is needed here to keep it robust for the future.
            // coverity[autosar_cpp14_m6_4_5_violation]
            case TraceErrorCode::JsonConfigParseError:
                return "json config parsing error";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case TraceErrorCode::TraceErrorDisableAllTracePoints:
                return "Tracing is completely disabled because of unrecoverable error";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case TraceErrorCode::TraceErrorDisableTracePointInstance:
                return "Tracing for the given trace-point instance is disabled because of unrecoverable error";
            // coverity[autosar_cpp14_m6_4_5_violation]
            default:
                return "unknown trace error";
        }
    }
};

// Suppress "AUTOSAR C++14 M3-2-3" rule finding. This rule states: "A type, object or function that is used in multiple
// translation units shall be declared in one and only one file.".
// This is false positive. Function is declared only once.
// coverity[autosar_cpp14_m3_2_3_violation : FALSE]
score::result::Error MakeError(const TraceErrorCode code, const std::string_view message = "");

}  // namespace score::mw::com::impl::tracing

#endif  // SCORE_MW_COM_IMPL_TRACING_CONFIGURATION_TRACE_CONFIG_ERROR_H
