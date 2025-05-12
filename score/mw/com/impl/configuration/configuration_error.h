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
#ifndef SCORE_MW_COM_IMPL_CONFIGURATION_CONFIGURATION_ERROR_H
#define SCORE_MW_COM_IMPL_CONFIGURATION_CONFIGURATION_ERROR_H

#include "score/result/result.h"

namespace score::mw::com::impl
{

/// \brief error codes, which can occur, when trying to create an InstanceIdentifier from a string representation
/// \details These error codes and the corresponding error domain are a preparation for a later/upcoming implementation
///          of static score::Result<InstanceIdentifier> InstanceIdentifier::Create(score_string_view serializedFormat).
///          Right now, it isn't used from core functionality.
enum class configuration_errc : score::result::ErrorCode
{
    serialization_deploymentinformation_invalid = 0,
    serialization_no_shmbindinginformation = 1,
    serialization_shmbindinginformation_invalid = 2,
    serialization_someipbindinginformation_invalid = 3,
    serialization_no_someipbindinginformation = 4,
};

/// \brief See above explanation in configuration_errc
class ConfigurationErrorDomain final : public score::result::ErrorDomain
{

  public:
    /* Gcc compiler bug leads to a compiler warning if override is not added, even if final keyword is there. */
    // \todo Gcc compiler bug leads to a compiler warning if override is not added, even if final keyword is there
    // (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=78010). When bug is fixed, remove the override keyword from the
    // MessageFor function signature and the coverity suppression.
    std::string_view MessageFor(const score::result::ErrorCode& code) const noexcept override final
    // coverity[autosar_cpp14_a10_3_1_violation]
    {
        // Suppress "AUTOSAR C++14 M6-4-5" and "AUTOSAR C++14 M6-4-3", The rule states: An unconditional throw or break
        // statement shall terminate every nonempty switch-clause." and
        // "A switch statement shall be a well-formed switch statement.", respectively.
        // These findings are false positives. The `return` statements in the case clauses of this switch statement
        // unconditionally exits the function, making an additional `break` statement redundant.
        // coverity[autosar_cpp14_m6_4_3_violation]
        switch (code)
        {
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(configuration_errc::serialization_deploymentinformation_invalid):
                return "serialization of <DeploymentInformation> is invalid";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(configuration_errc::serialization_no_shmbindinginformation):
                return "no serialization of <LoLaShmBindingInfo>";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(configuration_errc::serialization_shmbindinginformation_invalid):
                return "serialization of <LoLaShmBindingInfo> is invalid";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(configuration_errc::serialization_no_someipbindinginformation):
                return "no serialization of <SomeIpBindingInfo>";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(
                configuration_errc::serialization_someipbindinginformation_invalid):
                return "serialization of <SomeIpBindingInfo> is invalid";
            // coverity[autosar_cpp14_m6_4_5_violation]
            default:
                return "unknown configuration error";
        }
    }
};

score::result::Error MakeError(const configuration_errc code, const std::string_view message = "");

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_CONFIGURATION_CONFIGURATION_ERROR_H
