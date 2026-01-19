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
#ifndef SCORE_LIB_MESSAGE_PASSING_QNX_DISPATCH_QNX_RESOURCE_PATH_H
#define SCORE_LIB_MESSAGE_PASSING_QNX_DISPATCH_QNX_RESOURCE_PATH_H

#include <score/static_vector.hpp>

#include <string_view>

namespace score
{
namespace message_passing
{
namespace detail
{

inline constexpr std::string_view GetQnxPrefix() noexcept
{
    // TODO: add new path to secpol (after switching users of message_passing to 2.0)
    using std::literals::string_view_literals::operator""sv;
    return "/mw_com/message_passing/"sv;
}

class QnxResourcePath
{
  public:
    explicit QnxResourcePath(const std::string_view identifier) noexcept;

    const char* c_str() const noexcept
    {
        return buffer_.data();
    }

    static constexpr std::size_t kMaxIdentifierLen = 256U;

  private:
    score::cpp::static_vector<char, GetQnxPrefix().size() + kMaxIdentifierLen + 1U> buffer_;
};

}  // namespace detail
}  // namespace message_passing
}  // namespace score

#endif  // SCORE_LIB_MESSAGE_PASSING_QNX_DISPATCH_QNX_RESOURCE_PATH_H
