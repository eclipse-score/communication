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
#ifndef SCORE_LIB_MESSAGE_PASSING_TIMED_COMMAND_QUEUE_ENTRY_H
#define SCORE_LIB_MESSAGE_PASSING_TIMED_COMMAND_QUEUE_ENTRY_H

#include "score/containers/intrusive_list.h"

#include <score/callback.hpp>

#include <chrono>

namespace score
{
namespace message_passing
{
namespace detail
{

class TimedCommandQueue;

class TimedCommandQueueEntry : public score::containers::intrusive_list_element<TimedCommandQueue>
{
  public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::steady_clock::time_point;
    using QueuedCallback = score::cpp::callback<void(TimePoint) /* noexcept */>;

  private:
    // Suppress "AUTOSAR C++14 A11-3-1" rule finding: "Friend declarations shall not be used."
    // TimedCommandQueue acts like a module scope
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class TimedCommandQueue;

    TimePoint until_{};
    const void* owner_{nullptr};
    QueuedCallback callback_{};
};

}  // namespace detail
}  // namespace message_passing
}  // namespace score

#endif  // SCORE_LIB_MESSAGE_PASSING_TIMED_COMMAND_QUEUE_ENTRY_H
