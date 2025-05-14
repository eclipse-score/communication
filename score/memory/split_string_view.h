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
#ifndef SCORE_LIB_MEMORY_SPLIT_STRING_VIEW_H
#define SCORE_LIB_MEMORY_SPLIT_STRING_VIEW_H

#include "score/string_view.hpp"

namespace score::memory
{

/// \brief Split a string_view by a character into multiple substring views.
/// \details This class shall **not** use any dynamic memory allocation.
/// This splitter is lazy, because it only processes one element at a time.
/// Thus it is not possible to get the number of substrings in advance and the class only provides a forward iterator.
/// Substring views might be empty, e.g. if a seperator occurs at the beginning, or is repeated multiple times.
///
/// This class shall behave similar to C++20 `std::ranges::lazy_split_view`.
/// The primary use case is with a range-based for loop:
/// ```C++
///     LazySplitStringView splitter("Hello World"_sv, ' ');
///     for (std::string_view segment : splitter)
///     {
///         HandleSegment(segment)
///     }
/// ```
///
/// Thus we apply the same behavior in the edge cases as std::ranges::lazy_split_view:
///     If source == seperator, one empty substring shall be returned.
///     Else if the source begins with a seperator, the first substring shall be an empty string view.
///     Else if the source ends with a seperator the splitter shall ignore the right side of the trailing seperator.
class LazySplitStringView final
{
  public:
    explicit LazySplitStringView(const score::cpp::string_view source, const score::cpp::string_view::value_type seperator) noexcept;

    class Iterator
    {
      public:
        /// \brief Returns the view on the current substring
        /// \precondition *this != end()
        score::cpp::string_view operator*() const noexcept;

        /// \brief Move this to the next substring.
        Iterator& operator++() noexcept;

        friend bool operator==(const Iterator& lhs, const Iterator& rhs) noexcept;
        friend bool operator!=(const Iterator& lhs, const Iterator& rhs) noexcept;

      private:
        explicit Iterator(const LazySplitStringView&, const score::cpp::string_view::size_type start_index) noexcept;
        const LazySplitStringView& split_view_;
        score::cpp::string_view::size_type start_index_;
        score::cpp::string_view::size_type seperator_index_;  // Class invariant: seperator_index_ >= start_index_

        // Suppress "AUTOSAR C++14 A11-3-1" rule finding: "Friend declarations shall not be used.".
        // The 'friend' class is employed to encapsulate non-public members.
        // This design choice protects end users from implementation details
        // and prevents incorrect usage. Friend classes provide controlled
        // access to private members, utilized internally, ensuring that
        // end users cannot access implementation specifics.
        // coverity[autosar_cpp14_a11_3_1_violation]
        friend LazySplitStringView;
    };

    // Code style: must use lowercase for `begin()` and `end()` so that this can be used as a forward iterator in the
    // standard library and range-based for loops.
    Iterator cbegin() const noexcept;
    Iterator cend() const noexcept;
    Iterator begin() const noexcept;
    Iterator end() const noexcept;

  private:
    score::cpp::string_view source_;
    score::cpp::string_view::value_type seperator_;
};

}  // namespace score::memory

#endif  // SCORE_LIB_MEMORY_STRING_LITERAL_H
