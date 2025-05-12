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
#include "score/mw/com/message_passing/qnx/resmgr_traits_common.h"

#include <score/assert.hpp>
#include <score/utility.hpp>

namespace score
{
namespace mw
{
namespace com
{
namespace message_passing
{

QnxResourcePath::QnxResourcePath(const std::string_view identifier) noexcept
    // Suppress "AUTOSAR C++14 M5-0-15", The rule states: "Array indexing shall be the only form of pointer arithmetic."
    // This is a false positive, calling static_vector constructor.
    // coverity[autosar_cpp14_m5_0_15_violation]
    : buffer_{GetQnxPrefix().cbegin(), GetQnxPrefix().cend()}
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION(identifier.size() <= kMaxIdentifierLen);
    score::cpp::ignore = buffer_.insert(buffer_.cend(), identifier.cbegin(), identifier.cend());
    buffer_.push_back(static_cast<char>(0));
}

}  // namespace message_passing
}  // namespace com
}  // namespace mw
}  // namespace score
