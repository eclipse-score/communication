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
#include "score/memory/string_comparison_adaptor.h"

#include <score/overload.hpp>

#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace score::memory
{

StringComparisonAdaptor::StringComparisonAdaptor(const std::string& str) : str_{str} {}

StringComparisonAdaptor& StringComparisonAdaptor::operator=(const std::string& str)
{
    str_ = str;
    return *this;
}

StringComparisonAdaptor::StringComparisonAdaptor(std::string&& str) : str_{std::move(str)} {}

StringComparisonAdaptor& StringComparisonAdaptor::operator=(std::string&& str)
{
    str_ = std::move(str);
    return *this;
}

StringComparisonAdaptor::StringComparisonAdaptor(const score::cpp::string_view& score_future_cpp_str_view)
    : str_{std::string_view{score_future_cpp_str_view.data(), score_future_cpp_str_view.size()}}
{
}

StringComparisonAdaptor& StringComparisonAdaptor::operator=(const score::cpp::string_view& score_future_cpp_str_view)
{
    str_ = std::string_view{score_future_cpp_str_view.data(), score_future_cpp_str_view.size()};
    return *this;
}

StringComparisonAdaptor::StringComparisonAdaptor(const std::string_view& str_view) : str_{str_view} {}

StringComparisonAdaptor& StringComparisonAdaptor::operator=(const std::string_view& str_view)
{
    str_ = str_view;
    return *this;
}

StringComparisonAdaptor::StringComparisonAdaptor(const score::StringLiteral& c_str) : str_{c_str} {}

StringComparisonAdaptor& StringComparisonAdaptor::operator=(const score::StringLiteral& c_str)
{
    str_ = c_str;
    return *this;
}

// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall
// not be called implicitly.".
// Rationale: std::visit Throws std::bad_variant_access if as-variant(vars_i).valueless_by_exception() is true for any
// variant vars_i in vars. The variant may only become valueless if an exception is thrown during different stages.
// Since we don't throw exceptions, it's not possible that the variant can return true from valueless_by_exception and
// therefore not possible that std::visit throws an exception.
// coverity[autosar_cpp14_a15_5_3_violation]
score::cpp::string_view StringComparisonAdaptor::GetAsStringView() const noexcept
{
    return std::visit(score::cpp::overload(
                          [](std::string_view variant) noexcept {
                              return score::cpp::string_view{variant.data(), variant.size()};
                          },
                          [](const std::string& variant) noexcept {
                              return score::cpp::string_view{variant};
                          },
                          [](const score::StringLiteral& variant) noexcept {
                              return score::cpp::string_view{variant};
                          }),
                      str_);
}

bool operator==(const StringComparisonAdaptor& lhs, const StringComparisonAdaptor& rhs) noexcept
{
    return lhs.GetAsStringView() == rhs.GetAsStringView();
}

bool operator!=(const StringComparisonAdaptor& lhs, const StringComparisonAdaptor& rhs) noexcept
{
    return lhs.GetAsStringView() != rhs.GetAsStringView();
}

bool operator<(const StringComparisonAdaptor& lhs, const StringComparisonAdaptor& rhs) noexcept
{
    return lhs.GetAsStringView().compare(rhs.GetAsStringView()) < 0;
}

}  // namespace score::memory
