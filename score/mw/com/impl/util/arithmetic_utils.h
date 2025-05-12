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
#ifndef SCORE_MW_COM_IMPL_UTIL_ARITHMETIC_UTILS_H
#define SCORE_MW_COM_IMPL_UTIL_ARITHMETIC_UTILS_H

#include <limits>

namespace score::mw::com::impl
{

template <typename T, T lhs, T rhs>
constexpr void static_assert_addition_does_not_overflow()
{
    static_assert(lhs <= std::numeric_limits<T>::max() - rhs);
}

template <typename T, T lhs, T rhs>
constexpr void static_assert_multiplication_does_not_overflow()
{
    static_assert(lhs <= std::numeric_limits<T>::max() / rhs);
}

template <typename T, T lhs, T rhs>
constexpr T add_without_overflow()
{
    // Suppress "AUTOSAR C++14 M0-1-9" rule violations. The rule states "There shall be no dead code".
    // Rationale: False positive: This is not dead code. The function static_assert_addition_does_not_overflow is
    // evaluated at compile time.
    // coverity[autosar_cpp14_m0_1_9_violation : FALSE]
    static_assert_addition_does_not_overflow<T, lhs, rhs>();
    return lhs + rhs;
}

template <typename T, T lhs, T rhs>
constexpr T multiply_without_overflow()
{
    // Suppress "AUTOSAR C++14 M0-1-9" rule violations. The rule states "There shall be no dead code".
    // Rationale: False positive: This is not dead code. The function static_assert_multiplication_does_not_overflow is
    // evaluated at compile time.
    // coverity[autosar_cpp14_m0_1_9_violation : FALSE]
    static_assert_multiplication_does_not_overflow<T, lhs, rhs>();
    return lhs * rhs;
}

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_UTIL_ARITHMETIC_UTILS_H
