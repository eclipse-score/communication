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
#ifndef SCORE_MW_COM_IMPL_TRACING_CONFIGURATION_HASH_HELPER_UTILITY_H
#define SCORE_MW_COM_IMPL_TRACING_CONFIGURATION_HASH_HELPER_UTILITY_H

#include <iterator>
#include <limits>
#include <type_traits>
#include <utility>

namespace score::mw::com::impl::tracing::configuration
{

template <typename InputIt, typename T, typename std::enable_if_t<std::is_unsigned<T>::value>* = nullptr>
auto Accumulate(InputIt first, InputIt last, const T init_value) noexcept -> std::pair<T, bool>
{
    static_assert(
        std::is_base_of<std::input_iterator_tag, typename std::iterator_traits<InputIt>::iterator_category>::value,
        "InputIt must be an input iterator");
    T result{init_value};
    bool overflow_error{false};
    // Suppress "AUTOSAR C++14 M5-0-15" rule finding.
    // This rule states:"indexing shall be the only form of pointer arithmetic.".
    // Rationale: Tolerated due to containers providing pointer-like iterators.
    // The Coverity tool considers these iterators as raw pointers.
    // coverity[autosar_cpp14_m5_0_15_violation]
    for (; first != last; ++first)
    {
        overflow_error = (std::numeric_limits<T>::max() - result) < (*first);
        if (overflow_error)
        {
            break;
        }
        // Suppress "AUTOSAR C++14 A4-7-1" rule finding. This rule states: "An integer expression shall not lead to
        // data loss.". The check above ensures that "result" will not exceed its maximum value for size_t type.
        // coverity[autosar_cpp14_a4_7_1_violation]
        result += *first;
    }
    return {result, overflow_error};
}

}  // namespace score::mw::com::impl::tracing::configuration

#endif  // SCORE_MW_COM_IMPL_TRACING_CONFIGURATION_HASH_HELPER_UTILITY_H
