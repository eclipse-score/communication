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
#include "score/mw/com/impl/configuration/quality_type.h"

#include <exception>

namespace score::mw::com::impl
{

namespace
{

constexpr auto kInvalidString = "kInvalid";
constexpr auto kAsilQmString = "kASIL_QM";
constexpr auto kAsilBString = "kASIL_B";

}  // namespace

std::string ToString(QualityType quality_type) noexcept
{
    // Suppress "AUTOSAR C++14 M6-4-5" and "AUTOSAR C++14 M6-4-3", The rule states: An unconditional throw or break
    // statement shall terminate every nonempty switch-clause." and "A switch statement shall be a well-formed switch
    // statement.", respectively.The `return` and std::terminate() statements in this case clause unconditionally exits
    // the function, making an additional `break` statement redundant.
    // coverity[autosar_cpp14_m6_4_3_violation] See above
    switch (quality_type)
    {
        // coverity[autosar_cpp14_m6_4_5_violation] Return will terminate this switch clause
        case QualityType::kInvalid:
            return kInvalidString;
        // coverity[autosar_cpp14_m6_4_5_violation] Return will terminate this switch clause
        case QualityType::kASIL_QM:
            return kAsilQmString;
        // coverity[autosar_cpp14_m6_4_5_violation] Return will terminate this switch clause
        case QualityType::kASIL_B:
            return kAsilBString;
        // coverity[autosar_cpp14_m6_4_5_violation] Return will terminate this switch clause
        default:
            std::terminate();
    }
}

QualityType FromString(std::string quality_type) noexcept
{
    if (quality_type == kInvalidString)
    {
        return QualityType::kInvalid;
    }
    else if (quality_type == kAsilQmString)
    {
        return QualityType::kASIL_QM;
    }
    else if (quality_type == kAsilBString)
    {
        return QualityType::kASIL_B;
    }
    else
    {
        std::terminate();
    }
}

auto areCompatible(const QualityType& lhs, const QualityType& rhs) -> bool
{
    return lhs == rhs;
}

std::ostream& operator<<(std::ostream& ostream_out, const QualityType& type)
{
    switch (type)
    {
        case QualityType::kInvalid:
            ostream_out << "Invalid";
            break;
        case QualityType::kASIL_QM:
            ostream_out << "QM";
            break;
        case QualityType::kASIL_B:
            ostream_out << 'B';
            break;
        default:
            ostream_out << "(unknown)";
            break;
    }

    return ostream_out;
}

}  // namespace score::mw::com::impl
