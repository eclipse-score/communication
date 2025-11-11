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
#include "score/message_passing/qnx_dispatch/qnx_resource_path.h"

#include <score/assert.hpp>
#include <score/utility.hpp>
#include <cstddef>

namespace score
{
namespace message_passing
{
namespace detail
{

// constexpr std::size_t QnxResourcePath::kMaxIdentifierLen;

// Suppress "AUTOSAR C++14 M5-0-15" rule finding: "Array indexing shall be the only form of pointer arithmetic".
// Well-defined clear local use
QnxResourcePath::QnxResourcePath(const std::string_view identifier) noexcept
    // coverity[autosar_cpp14_m5_0_15_violation]
    : buffer_{GetQnxPrefix().cbegin(), GetQnxPrefix().cend()}
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION((identifier.size() > 0U) && (identifier.size() <= kMaxIdentifierLen));

    auto identifier_begin = identifier.cbegin();
    if (*identifier_begin == '/')
    {
        // coverity[autosar_cpp14_m5_0_15_violation]
        ++identifier_begin;
    }
    score::cpp::ignore = buffer_.insert(buffer_.cend(), identifier_begin, identifier.cend());
    buffer_.push_back('\0');  // AUTOSAR C++14 M5-0-11: Use character literal instead of plain integer
}

}  // namespace detail
}  // namespace message_passing
}  // namespace score
