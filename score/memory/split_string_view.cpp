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
#include "score/memory/split_string_view.h"

#include "score/assert.hpp"

#include <algorithm>
#include <limits>

namespace
{

score::cpp::string_view::size_type FindNextSeperator(const score::cpp::string_view view,
                                              const score::cpp::string_view::size_type start_index,
                                              const score::cpp::string_view::value_type seperator) noexcept
{
    // Suppress "AUTOSAR C++14 M5-0-15" rule finding: "Array indexing shall be the only form of pointer arithmetic.".
    // False positive, no pointer arithmetic.
    // coverity[autosar_cpp14_m5_0_15_violation : FALSE]
    const auto signed_index = std::distance(view.begin(), std::find(view.begin() + start_index, view.end(), seperator));

    // std::distance() returns a std::ptrdiff_t which is a signed integer type.
    static_assert(std::numeric_limits<score::cpp::string_view::size_type>::max() >=
                      static_cast<score::cpp::string_view::size_type>(std::numeric_limits<decltype(signed_index)>::max()),
                  "Maximum of distance type shall be contained in string_view size type.");

    // The signed_index shall be greater than or equal zero, because from begin() of the container there exists no valid
    // iterator to which the number of hops could be negative.
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(signed_index >= 0, "Signed_index shall be non-negative.");

    return static_cast<score::cpp::string_view::size_type>(signed_index);
}

}  // namespace

namespace score::memory
{

LazySplitStringView::LazySplitStringView(const score::cpp::string_view source,
                                         const score::cpp::string_view::value_type seperator) noexcept
    : source_{source}, seperator_{seperator}
{
}

LazySplitStringView::Iterator LazySplitStringView::cbegin() const noexcept
{
    return LazySplitStringView::Iterator{*this, 0U};
}

LazySplitStringView::Iterator LazySplitStringView::cend() const noexcept
{
    return LazySplitStringView::Iterator{*this, source_.size()};
}

LazySplitStringView::Iterator LazySplitStringView::begin() const noexcept
{
    return this->cbegin();
}

LazySplitStringView::Iterator LazySplitStringView::end() const noexcept
{
    return this->cend();
}

LazySplitStringView::Iterator::Iterator(const LazySplitStringView& view,
                                        const score::cpp::string_view::size_type start_index) noexcept
    : split_view_{view},
      start_index_{start_index},
      seperator_index_{FindNextSeperator(view.source_, start_index, view.seperator_)}
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(seperator_index_ >= start_index_,
                       "Class invariant shall ensure seperator index shall be greater or equal than start index.");
}

score::cpp::string_view LazySplitStringView::Iterator::operator*() const noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(seperator_index_ >= start_index_,
                       "Class invariant shall ensure seperator index shall be greater or equal than start index.");
    const score::cpp::string_view::size_type substring_size = seperator_index_ - start_index_;
    // Suppress "AUTOSAR C++14 M5-0-15" rule finding: "Array indexing shall be the only form of pointer arithmetic.".
    // False positive, no pointer arithmetic.
    // coverity[autosar_cpp14_m5_0_15_violation : FALSE]
    return score::cpp::string_view{split_view_.source_.begin() + start_index_, substring_size};
}

bool operator==(const LazySplitStringView::Iterator& lhs, const LazySplitStringView::Iterator& rhs) noexcept
{
    // Other members than start_index are invariant.
    return lhs.start_index_ == rhs.start_index_;
}

LazySplitStringView::Iterator& LazySplitStringView::Iterator::operator++() noexcept
{
    start_index_ = seperator_index_ + 1U;

    // Technically iterator must not be incremented at the end(), but prefer defensive checks.
    start_index_ = std::min(split_view_.source_.size(), start_index_);

    seperator_index_ = FindNextSeperator(split_view_.source_, start_index_, split_view_.seperator_);

    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(seperator_index_ >= start_index_,
                       "Class invariant shall ensure seperator index shall be greater or equal than start index.");

    return *this;
}

bool operator!=(const LazySplitStringView::Iterator& lhs, const LazySplitStringView::Iterator& rhs) noexcept
{
    return !(lhs == rhs);
}

}  // namespace score::memory
