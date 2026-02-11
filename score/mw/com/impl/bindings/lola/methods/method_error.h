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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHODS_METHOD_ERROR_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHODS_METHOD_ERROR_H

#include "score/result/result.h"

namespace score::mw::com::impl::lola
{

/// \brief Error used within the implementation of LoLa service methods.
///
/// Since we need to propagate errors between Proxy and Skeleton during method subscription / calls, we need a set of
/// error codes which can be sent via message passing to communicate errors that may have occurred on one side to the
/// other. Once mw/com has a well designed set of error codes, these codes may be moved to MethodErrc so that they can
/// be returned to the user. But for now, we keep these error codes out of MethodErrc to avoid a breaking change to the
/// public API.
enum class MethodErrc : score::result::ErrorCode
{
    kInvalid,
    kSkeletonAlreadyDestroyed,
    kUnexpectedMessage,
    kUnexpectedMessageSize,
    kMessagePassingError,
    kNotSubscribed,
    kNotOffered,
    kUnknownProxy,
    // Note. kNumEnumElements must ALWAYS be the last enum entry
    kNumEnumElements
};

class MethodErrorDomain final : public score::result::ErrorDomain
{
  public:
    /* Gcc compiler bug leads to a compiler warning if override is not added, even if final keyword is there. */
    /// \todo Gcc compiler bug leads to a compiler warning if override is not added, even if final keyword is there
    /// (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=78010). When bug is fixed, remove the override keyword from the
    /// MessageFor function signature and the AUTOSAR.MEMB.VIRTUAL.SPEC klocwork suppression.
    std::string_view MessageFor(const score::result::ErrorCode& code) const noexcept override final
    // Suppress "AUTOSAR C++14 A10-3-1" rule finding: Virtual function declaration shall contain exactly one of the
    // three specifiers: (1) virtual, (2) override, (3) final.
    // Rationale : See explanation above.
    // coverity[autosar_cpp14_a10_3_1_violation]
    {
        // Suppress "AUTOSAR C++14 M6-4-5" and "AUTOSAR C++14 M6-4-3", The rule states: "An unconditional throw or break
        // statement shall terminate every nonempty switch-clause." and "A switch statement shall be a well-formed
        // switch statement." respectively. There is return for every switch case, adding a break would be dead code.
        // coverity[autosar_cpp14_m6_4_3_violation]
        switch (code)
        {
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(MethodErrc::kSkeletonAlreadyDestroyed):
                return "Command failed since skeleton was already destroyed.";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(MethodErrc::kUnexpectedMessage):
                return "Message with an unexpected type was received.";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(MethodErrc::kUnexpectedMessageSize):
                return "Message with an unexpected size was received.";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(MethodErrc::kMessagePassingError):
                return "Message passing failed with an error.";
                // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(MethodErrc::kNotSubscribed):
                return "Method has not been successfully subscribed.";
                // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(MethodErrc::kNotOffered):
                return "Method has not been fully offered.";
                // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(MethodErrc::kUnknownProxy):
                return "Proxy is not allowed to access method.";
                // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(MethodErrc::kInvalid):
            case static_cast<score::result::ErrorCode>(MethodErrc::kNumEnumElements):
                SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(false,
                                       "kNumEnumElements/kInvalid are not valid states for the enum! They're just used "
                                       "for verifying the value of an enum during serialization / deserialization!");
            default:
                return "unknown future error";
        }
    }
};

score::result::Error MakeError(const MethodErrc code, const std::string_view message = "");

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHODS_METHOD_ERROR_H
