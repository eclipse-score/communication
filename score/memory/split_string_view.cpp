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

score::cpp::string_view::size_type FindNextSeperator(score::cpp::string_view view,
                                              const score::cpp::string_view::size_type start_index,
                                              const score::cpp::string_view::value_type seperator) noexcept
{
    view.remove_prefix(start_index);
    if (const auto pos = view.find(seperator); pos != score::cpp::string_view::npos)
    {
        return start_index + pos;
    }
    else
    {
        return start_index + view.size();
    }
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
    score::cpp::string_view result{split_view_.source_};
    result.remove_suffix(result.size() - seperator_index_);
    result.remove_prefix(start_index_);
    return result;
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
