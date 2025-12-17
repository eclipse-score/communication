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
#ifndef SCORE_LIB_MEMORY_SHARED_SHARED_MEMORY_ERROR_H
// coverity[autosar_cpp14_m16_0_2_violation] false-positive: global namespace
#define SCORE_LIB_MEMORY_SHARED_SHARED_MEMORY_ERROR_H

#include "score/result/result.h"

#include <string_view>

namespace score::memory::shared
{

/// \brief error codes, which can occur when dealing with functionality related to shared memory.
enum class SharedMemoryErrorCode : score::result::ErrorCode
{
    UnknownSharedMemoryIdentifier = 1,
};

/// \brief See above explanation in SharedMemoryErrorCode
class SharedMemoryErrorDomain final : public score::result::ErrorDomain
{
  public:
    /* Gcc compiler bug leads to a compiler warning if override is not added, even if final keyword is there. */
    /// \todo Gcc compiler bug leads to a compiler warning if override is not added, even if final keyword is there
    /// (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=78010). When bug is fixed, remove the override keyword from the
    /// MessageFor function signature and the autosar_cpp14_a10_3_1_violation coverity suppression.
    // Suppress "AUTOSAR C++14 A10-3-1" rule finding: Virtual function declaration shall contain exactly one of the
    // three specifiers: (1) virtual, (2) override, (3) final.
    // Rationale : See explanation above.
    // coverity[autosar_cpp14_a10_3_1_violation]
    std::string_view MessageFor(const score::result::ErrorCode& code) const noexcept override final
    // coverity[autosar_cpp14_a10_3_1_violation]
    {
        // Suppress "AUTOSAR C++14 M6-4-5" and "AUTOSAR C++14 M6-4-3", The rule states: An unconditional throw or break
        // statement shall terminate every nonempty switch-clause." and "A switch statement shall be a well-formed
        // switch statement.", respectively.
        // Rationale: The `return` statements in this case clause unconditionally exits the switch case, making an
        // additional `break` statement redundant.
        // coverity[autosar_cpp14_m6_4_3_violation] See above
        switch (code)
        {
                // coverity[autosar_cpp14_m6_4_5_violation] Return will terminate this switch clause
            case static_cast<score::result::ErrorCode>(SharedMemoryErrorCode::UnknownSharedMemoryIdentifier):
                return "Unknown shared memory identifier";
                // coverity[autosar_cpp14_m6_4_5_violation] Return will terminate this switch clause
            default:
                return "unknown shared memory error";
        }
    }
};

score::result::Error MakeError(const SharedMemoryErrorCode code, const std::string_view message = "");

}  // namespace score::memory::shared

#endif  // SCORE_LIB_MEMORY_SHARED_SHARED_MEMORY_ERROR_H
