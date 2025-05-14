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
#ifndef SCORE_LIB_MEMORY_STRING_COMPARISON_ADAPTOR_H
#define SCORE_LIB_MEMORY_STRING_COMPARISON_ADAPTOR_H

#include "score/memory/string_literal.h"

#include <score/string_view.hpp>

#include <functional>
#include <string>
#include <string_view>
#include <variant>

namespace score::memory
{

/// @brief Adapter to make a std::string within a container comparable with an score::cpp::string_view
class StringComparisonAdaptor
{
  public:
    /// @brief Copy and Move constructors
    StringComparisonAdaptor(const StringComparisonAdaptor& str) = default;
    StringComparisonAdaptor& operator=(const StringComparisonAdaptor& str) = default;
    StringComparisonAdaptor(StringComparisonAdaptor&& str) noexcept = default;
    StringComparisonAdaptor& operator=(StringComparisonAdaptor&& str) noexcept = default;

    /// @brief Constructors for string
    // IMPLICIT CONVERSION JUSIFICAION
    // StringComparisonAdaptor is used as a key type for mw/lib/json's Object type. We want these keys to be implicitly
    // constructible from any string-like object. To have an interface that looks string-like:
    // json_bla["bla"] = value_bla;
    // instead of
    // json_bla[StringComparisonAdaptor("bla")] = json_bla;
    // we can not simply overload the operator[] for json::Object since it is a type alias around std::map and not our
    // own type.
    // NOLINTNEXTLINE(google-explicit-constructor): IMPLICIT CONVERSION JUSTIFICATION
    StringComparisonAdaptor(const std::string& str);
    StringComparisonAdaptor& operator=(const std::string& str);
    // NOLINTNEXTLINE(google-explicit-constructor): IMPLICIT CONVERSION JUSTIFICATION
    StringComparisonAdaptor(std::string&& str);
    StringComparisonAdaptor& operator=(std::string&& str);

    /// @brief Constructors for string view
    // NOLINTNEXTLINE(google-explicit-constructor): IMPLICIT CONVERSION JUSTIFICATION
    StringComparisonAdaptor(const score::cpp::string_view& score_future_cpp_str_view);
    StringComparisonAdaptor& operator=(const score::cpp::string_view& score_future_cpp_str_view);

    /// @brief Constructors for std::string view
    // NOLINTNEXTLINE(google-explicit-constructor): IMPLICIT CONVERSION JUSTIFICATION
    StringComparisonAdaptor(const std::string_view& str_view);
    StringComparisonAdaptor& operator=(const std::string_view& str_view);

    /// @brief Constructors for assignment of c-style strings
    /// This is mainly required for strings specified at compile time
    // NOLINTNEXTLINE(google-explicit-constructor): IMPLICIT CONVERSION JUSTIFICATION
    StringComparisonAdaptor(const score::StringLiteral& c_str);
    StringComparisonAdaptor& operator=(const score::StringLiteral& c_str);

    ~StringComparisonAdaptor() = default;

    /// @brief Gets a string view of the content
    /// @return The content as string_view
    score::cpp::string_view GetAsStringView() const noexcept;

  private:
    std::variant<std::string_view, std::string, score::StringLiteral> str_;
};

/// @brief Compares the underlying content of the string/string_view.
/// It is unimportant what type stores the content.
/// @param lhs The left-hand side value to compare with
/// @param rhs The right-hand side value to compare with
/// @return True if the content is equal
bool operator==(const StringComparisonAdaptor& lhs, const StringComparisonAdaptor& rhs) noexcept;

/// @brief Compares the underlying content of the string/string_view.
/// It is unimportant what type stores the content.
/// @param lhs The left-hand side value to compare with
/// @param rhs The right-hand side value to compare with
/// @return True if the content is equal
bool operator!=(const StringComparisonAdaptor& lhs, const StringComparisonAdaptor& rhs) noexcept;

bool operator<(const StringComparisonAdaptor& lhs, const StringComparisonAdaptor& rhs) noexcept;

}  // namespace score::memory

namespace std
{

template <>
// NOLINTNEXTLINE(score-struct-usage-compliance): STL defines a struct, not a class
class hash<score::memory::StringComparisonAdaptor>
{
  public:
    /// @brief Gets the hash for a StringComparisonAdaptor
    /// For the hash it is only important what content the string(-view) has
    /// @return The hash of the underlying content
    // NOL INT NEX TLINE(score-hash-noexcept): operator invokes non-noexcept calls
    size_t operator()(const score::memory::StringComparisonAdaptor& k) const noexcept
    {
        const auto as_string_view = k.GetAsStringView();
        return std::hash<score::cpp::string_view>{}(as_string_view);
    }
};

}  // namespace std

#endif  // SCORE_LIB_MEMORY_STRING_COMPARISON_ADAPTOR_H
