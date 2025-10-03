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
#include "score/memory/shared/pointer_arithmetic_util.h"

#include "score/mw/log/log_types.h"
#include "score/mw/log/logging.h"

#include <score/assert.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <limits>

namespace score::memory::shared
{

namespace detail
{

std::uintptr_t AddOffsetToPointerAsInteger(const std::uintptr_t pointer_as_integer, const std::size_t offset)
{
    static_assert(std::numeric_limits<std::size_t>::max() <= std::numeric_limits<std::uintptr_t>::max(),
                  "If max value of size_t is larger than uintptr_t, then subtracting offset from the max value of "
                  "uintptr_t could underflow.");

    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(pointer_as_integer <= (std::numeric_limits<std::uintptr_t>::max() - offset),
                           "Could not add offset to pointer. Result would lead to overflow of std::uintptr_t");
    return pointer_as_integer + offset;
}

std::uintptr_t SubtractOffsetFromPointerAsInteger(const std::uintptr_t pointer_as_integer, const std::size_t offset)
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(pointer_as_integer >= offset,
                           "Could not subtract offset from pointer. Result would lead to underflow of std::size_t");
    return pointer_as_integer - offset;
}

}  // namespace detail

std::uintptr_t CastPointerToInteger(const void* const pointer) noexcept
{
    // Suppress "AUTOSAR C++14 A5-2-4" rule finding: reinterpret_cast shall not be used.
    // Rationale : According to https://timsong-cpp.github.io/cppwp/n4659/expr.reinterpret.cast#4, casting a pointer to
    // an integral type large enough to hold it is implementation-defined. We rely on sufficient testing to ensure that
    // the implementation defined behaviour performs as expected (i.e. that adding an offset to the integer pointer
    // value results in the expected final address).
    // Suppress "AUTOSAR C++14 M5-2-9" rule finding: "A cast shall not convert a pointer type to an integral type.".
    // Rationale: Same as rationale for autosar_cpp14_a5_2_4_violation.
    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast): see rationale above.
    // coverity[autosar_cpp14_a5_2_4_violation]
    // coverity[autosar_cpp14_m5_2_9_violation]
    return reinterpret_cast<std::uintptr_t>(pointer);
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
}

template <typename PointerType>
auto CastIntegerToPointer(std::uintptr_t integer) noexcept -> PointerType
{
    // Suppress "AUTOSAR C++14 A5-2-4" rule finding: reinterpret_cast shall not be used.
    // Rationale : According to https://timsong-cpp.github.io/cppwp/n4659/expr.reinterpret.cast#5, casting an integer to
    // a pointer is implementation-defined. We rely on sufficient testing to ensure that the implementation defined
    // behaviour performs as expected (i.e. the address of the resulting pointer is the same as the integer value).
    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast): see rationale above.
    // coverity[autosar_cpp14_a5_2_4_violation]
    return reinterpret_cast<PointerType>(integer);
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
}
template void* CastIntegerToPointer<void*>(std::uintptr_t) noexcept;
template const void* CastIntegerToPointer<const void*>(std::uintptr_t) noexcept;
template std::byte* CastIntegerToPointer<std::byte*>(std::uintptr_t) noexcept;

score::mw::log::LogHex64 PointerToLogValue(const void* const pointer)
{
    // Suppress "AUTOSAR C++14 A5-2-4" rule finding: "reinterpret_cast shall not be used.".
    // Rationale : Accepted for conversion to log output
    // Suppress "AUTOSAR C++14 M5-2-9" rule finding: "A cast shall not convert a pointer type to an integral type.".
    // Rationale : As per C++11 standard (ISO/IEC 14882:2011) std::uintptr_t is an unsigned integer type capable of
    // holding a pointer to void.
    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast): see rationale above.
    // coverity[autosar_cpp14_a5_2_4_violation]
    // coverity[autosar_cpp14_m5_2_9_violation]
    return score::mw::log::LogHex64{reinterpret_cast<std::uintptr_t>(pointer)};
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
}

std::ptrdiff_t SubtractPointersBytes(const void* const first, const void* const second) noexcept
{
    static_assert(sizeof(std::ptrdiff_t) == sizeof(std::uintptr_t), "");

    const auto first_address_as_integer = CastPointerToInteger(first);
    const auto second_address_as_integer = CastPointerToInteger(second);

    constexpr auto ptr_diff_max = std::numeric_limits<std::ptrdiff_t>::max();

    if (first_address_as_integer > second_address_as_integer)
    {
        const auto result_as_integer =
            detail::SubtractOffsetFromPointerAsInteger(first_address_as_integer, second_address_as_integer);
        if (result_as_integer > static_cast<std::uintptr_t>(ptr_diff_max))
        {
            score::mw::log::LogFatal("shm")
                << "Could not subtract " << second_address_as_integer << "from" << first_address_as_integer
                << ". Result does not fit into std::ptrdiff_t. Terminating.";
            std::terminate();
        }
        return static_cast<std::ptrdiff_t>(result_as_integer);
    }

    // Calculate the absolute value of the subtraction by reversing the order (since we know that the second value is >=
    // the first).
    const auto absolute_value_result_as_integer =
        detail::SubtractOffsetFromPointerAsInteger(second_address_as_integer, first_address_as_integer);

    // Since we need to cast positive_result_as_integer to a std::ptrdiff_t, we have to handle the special case in which
    // the actual result is equal to ptr_diff_min. In such a case, the absolute value of the result will be ptr_diff_max
    // + 1U which cannot fit inside a std::ptrdiff_t. So we directly return ptr_diff_min in such a case.
    if (absolute_value_result_as_integer == static_cast<std::uintptr_t>(ptr_diff_max) + 1U)
    {
        return std::numeric_limits<std::ptrdiff_t>::min();
    }
    if (absolute_value_result_as_integer > static_cast<std::uintptr_t>(ptr_diff_max))
    {
        score::mw::log::LogFatal("shm") << "Could not subtract " << second_address_as_integer << "from"
                                      << first_address_as_integer
                                      << ". Result does not fit into std::ptrdiff_t. Terminating.";
        std::terminate();
    }
    return -1 * static_cast<std::ptrdiff_t>(absolute_value_result_as_integer);
}

}  // namespace score::memory::shared
