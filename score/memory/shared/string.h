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
#ifndef SCORE_LIB_MEMORY_SHARED_STRING_H
#define SCORE_LIB_MEMORY_SHARED_STRING_H

#include "score/memory/shared/polymorphic_offset_ptr_allocator.h"

#include "score/language/safecpp/string_view/char_traits_wrapper.h"

#include <string>

namespace score::memory::shared
{

// TODO: Currently we cannot use the wrapper around std::basic_string in QNX due to a bug in LLVM which prevents using a
//  pointer type (i.e. OffsetPtr) in the allocator which is not trivially constructible. Therefore, BasicString cannot
//  currently be used when building for QNX. If a BasicString is instantiated when building for QNX, it will throw a
//  compiler error.

// LCOV_EXCL_START As described above, this wrapper cannot be used in QNX due to a bug. Therefore, we cannot test the
// code in QNX (only on host).
// LCOV_EXCL_BR_START See above

/// \brief We provide our custom version of a std::BasicString to ensure that it supports HEAP and SharedMemory usage
/// with our custom allocator.
template <typename CharT, typename Traits = typename safecpp::char_traits_wrapper<CharT>::traits_type>
using BasicString = std::basic_string<CharT, Traits, score::memory::shared::PolymorphicOffsetPtrAllocator<CharT>>;

// Provide custom operator==/operator!= implementations to be interoperable with std::string in this regard
template <typename CharT, typename Traits>
// A13-5-5: "Comparison operators shall be non-member functions with identical parameter types and noexcept."
// Rule concerns code uniformity, not functional safety; implementation safely ensures correct string comparison
// semantics (size equality + data comparison) while enabling necessary std::string interoperability.
// coverity[autosar_cpp14_a13_5_5_violation]
inline bool operator==(const BasicString<CharT, Traits>& lhs, const std::string& rhs) noexcept
{
    return ((lhs.size() == rhs.size()) && (lhs.compare(0U, lhs.size(), rhs.data()) == 0));
}

template <typename CharT, typename Traits>
// A13-5-5: "Comparison operators shall be non-member functions with identical parameter types and noexcept."
// Rule concerns code uniformity, not functional safety; implementation safely ensures correct string comparison
// semantics (size equality + data comparison) while enabling necessary std::string interoperability.
// coverity[autosar_cpp14_a13_5_5_violation]
inline bool operator!=(const BasicString<CharT, Traits>& lhs, const std::string& rhs) noexcept
{
    return !operator==(lhs, rhs);
}

template <typename CharT, typename Traits>
// A13-5-5: "Comparison operators shall be non-member functions with identical parameter types and noexcept."
// Rule concerns code uniformity, not functional safety; implementation safely ensures correct string comparison
// semantics (size equality + data comparison) while enabling necessary std::string interoperability.
// coverity[autosar_cpp14_a13_5_5_violation]
inline bool operator==(const std::string& lhs, const BasicString<CharT, Traits>& rhs) noexcept
{
    return ((lhs.size() == rhs.size()) && (lhs.compare(0U, lhs.size(), rhs.data()) == 0));
}

template <typename CharT, typename Traits>
// A13-5-5: "Comparison operators shall be non-member functions with identical parameter types and noexcept."
// Rule concerns code uniformity, not functional safety; implementation safely ensures correct string comparison
// semantics (size equality + data comparison) while enabling necessary std::string interoperability.
// coverity[autosar_cpp14_a13_5_5_violation]
inline bool operator!=(const std::string& lhs, const BasicString<CharT, Traits>& rhs) noexcept
{
    return !operator==(lhs, rhs);
}

/// \brief Type definition of a BasicString operating on char, akin to std::string
using String = BasicString<char>;

// LCOV_EXCL_BR_STOP
// LCOV_EXCL_STOP

}  // namespace score::memory::shared

#endif  // SCORE_LIB_MEMORY_SHARED_STRING_H
