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
#ifndef SCORE_LIB_MEMORY_ANY_STRING_VIEW_H
#define SCORE_LIB_MEMORY_ANY_STRING_VIEW_H

#include "score/string_view.hpp"

#include <string_view>

namespace score::memory
{

// Goal of this class is to ease the transition between score::cpp::string_view and std::string_view
// Thus the mare goal of this class is to allow implicit conversions between these two types.
class AnyStringView
{
  public:
    // NOLINTNEXTLINE(google-explicit-constructor) See argumentation above
    constexpr AnyStringView(const char* literal) : string_view_{literal} {};

    // NOLINTNEXTLINE(google-explicit-constructor) See argumentation above
    constexpr AnyStringView(const std::string_view& string_view) : string_view_{string_view} {};

    // NOLINTNEXTLINE(google-explicit-constructor) See argumentation above
    constexpr AnyStringView(const score::cpp::string_view& string_view)
        : string_view_{string_view.data(), string_view.size()} {};

    template <typename Allocator>
    // NOLINTNEXTLINE(google-explicit-constructor) See argumentation above
    constexpr AnyStringView(const std::basic_string<char, std::char_traits<char>, Allocator>& s) noexcept
        : string_view_{s.data(), s.size()}
    {
    }

    // NOLINTBEGIN(google-explicit-constructor) See argumentation above
    // coverity[autosar_cpp14_a13_5_2_violation]
    constexpr operator score::cpp::string_view() const
    {
        return {string_view_.data(), string_view_.size()};
    }
    // NOLINTEND(google-explicit-constructor) see above

    // NOLINTBEGIN(google-explicit-constructor) See argumentation above
    // coverity[autosar_cpp14_a13_5_2_violation]
    constexpr operator std::string_view() const
    {
        return string_view_;
    }
    // NOLINTEND(google-explicit-constructor) see above

  private:
    std::string_view string_view_;
};

}  // namespace score::memory

#endif  // SCORE_LIB_MEMORY_ANY_STRING_VIEW_H
