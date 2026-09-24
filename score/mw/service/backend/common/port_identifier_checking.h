/*********************************************************************************
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
 *******************************************************************************/

#ifndef SCORE_MW_SERVICE_BACKEND_COMMON_PORT_IDENTIFIER_CHECKING_H
#define SCORE_MW_SERVICE_BACKEND_COMMON_PORT_IDENTIFIER_CHECKING_H

#include <string>
#include <type_traits>

namespace score::mw::service::backend::common
{
namespace details
{

template <typename T, typename = void>
// coverity[autosar_cpp14_a11_0_2_violation] struct inherits from std::false_type to implement custom type trait
struct CanInvokeGet : std::false_type
{
};

template <typename T>
// coverity[autosar_cpp14_a11_0_2_violation] struct inherits from std::true_type to implement custom type trait
struct CanInvokeGet<T, std::void_t<decltype(T::Get())>> : std::true_type
{
};

}  // namespace details

template <typename T>
// Rationale: False-positive, this function is used in this file multiple_immediate_instantiation_strategy.h.
// coverity[autosar_cpp14_a0_1_3_violation : FALSE] see above justification
constexpr bool IsValidPortIdentifierType() noexcept
{
    // Rationale: This is a false positive because "if constexpr" is a valid statement since C++17.
    // coverity[autosar_cpp14_a7_1_8_violation] see above justification
    // coverity[autosar_cpp14_m6_4_1_violation] false-positive, if-statement is followed by compound statement
    if constexpr (details::CanInvokeGet<T>::value)
    {
        return std::is_convertible_v<decltype(T::Get()), std::string>;
    }
    else
    {
        return false;
    }
}

}  // namespace score::mw::service::backend::common

#endif  // SCORE_MW_SERVICE_BACKEND_COMMON_PORT_IDENTIFIER_CHECKING_H
