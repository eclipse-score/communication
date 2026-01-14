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
#ifndef SCORE_MW_COM_MESSAGE_PASSING_RESMGR_TRAITS_COMMON_H
#define SCORE_MW_COM_MESSAGE_PASSING_RESMGR_TRAITS_COMMON_H

#include <score/static_vector.hpp>

#include <array>
#include <string_view>

namespace score::mw::com::message_passing
{

inline constexpr std::string_view GetQnxPrefix() noexcept
{
    using std::literals::operator""sv;
    return "/mw_com/message_passing"sv;
}

class QnxResourcePath
{
  public:
    explicit QnxResourcePath(const std::string_view identifier) noexcept;

    const char* c_str() const noexcept
    {
        return buffer_.data();
    }

    // coverity[autosar_cpp14_a0_1_1_violation] false-positive: is used to define buffer_ size
    static constexpr std::size_t kMaxIdentifierLen = 256U;

  private:
    score::cpp::static_vector<char, GetQnxPrefix().size() + kMaxIdentifierLen + 1U> buffer_;
};

}  // namespace score::mw::com::message_passing

#endif  // SCORE_MW_COM_MESSAGE_PASSING_RESMGR_TRAITS_COMMON_H
