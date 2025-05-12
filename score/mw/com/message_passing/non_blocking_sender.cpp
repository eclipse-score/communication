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
#include "score/mw/com/message_passing/non_blocking_sender.h"

#include <score/assert.hpp>
#include <score/expected.hpp>

#include <exception>
#include <iostream>
#include <utility>

namespace score::mw::com::message_passing
{

NonBlockingSender::NonBlockingSender(score::cpp::pmr::unique_ptr<ISender> wrapped_sender,
                                     const std::size_t max_queue_size,
                                     score::concurrency::Executor& executor)
    : ISender{},
      queue_{max_queue_size, score::cpp::pmr::get_default_resource()},
      queue_mutex_{},
      wrapped_sender_{std::move(wrapped_sender)},
      executor_{executor},
      current_send_task_result_{}
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE((wrapped_sender_ != nullptr), "Wrapped sender must not be null.");
    if (max_queue_size > QUEUE_SIZE_UPPER_LIMIT)
    {
        // Using std::endl to std::ostream object with the stream operator follows the idiomatic way that both
        // features in conjunction were designed in the C++ standard.
        std::cerr << "NonBlockingSender: Given max_queue_size: "
                  << max_queue_size
                  // coverity[autosar_cpp14_m8_4_4_violation] See above
                  << " exceeds built-in QUEUE_SIZE_UPPER_LIMIT." << std::endl;
        /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
        std::terminate();
    }
}

score::cpp::expected_blank<score::os::Error> NonBlockingSender::Send(const ShortMessage& message) noexcept
{
    return SendInternal(std::variant<ShortMessage, MediumMessage>(message));
}

score::cpp::expected_blank<score::os::Error> NonBlockingSender::Send(const MediumMessage& message) noexcept
{
    return SendInternal(std::variant<ShortMessage, MediumMessage>(message));
}

// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall not
// be called implicitly.".
// The function is noexcept. It uses some calls that are not marked as noexcept, but won't throw exceptions.
// coverity[autosar_cpp14_a15_5_3_violation]
void NonBlockingSender::SendQueueElements(const score::cpp::stop_token token) noexcept
{
    // This function is run when we have at least one element in the queue_ and the previous run of this function
    // is not accessing the queue_ anymore.
    // We have a single-producer, single-consumer queue; concurrent access to queue_.size_ is protected
    // by queue_mutex_; there shall be no concurrent access to individual elements of the queue.
    while (not token.stop_requested())
    {
        score::cpp::expected_blank<score::os::Error> send_result{};
        if (std::holds_alternative<ShortMessage>(queue_.front()))
        {
            send_result = wrapped_sender_->Send(std::get<ShortMessage>(queue_.front()));
        }
        else
        {
            send_result = wrapped_sender_->Send(std::get<MediumMessage>(queue_.front()));
        }

        if (send_result.operator bool() == false)
        {
            // Using std::endl to std::ostream object with the stream operator follows the idiomatic way that both
            // features in conjunction were designed in the C++ standard.
            // coverity[autosar_cpp14_m8_4_4_violation] See above
            std::cerr << "NonBlockingSender: SendQueueElements failed with error: " << send_result.error() << std::endl;
        }

        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            queue_.pop_front();
            if (queue_.empty())
            {
                break;
            }
        }
    }
}

score::cpp::expected_blank<score::os::Error> NonBlockingSender::SendInternal(
    const std::variant<ShortMessage, MediumMessage> message) noexcept
{
    std::lock_guard<std::mutex> guard(queue_mutex_);
    if ((queue_.full()) || executor_.ShutdownRequested())
    {
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(EAGAIN));
    }
    else
    {
        queue_.emplace_back(message);
        if (queue_.size() == 1U)
        {
            // Suppress "AUTOSAR C++14 A15-4-2" rule finding. This rule states: "I a function is declared to be
            // noexcept, noexcept(true) or noexcept(<true condition>), then it shall not exit with an exception"
            // By design, if `executor_.Submit()` ever fails, we expect program termination.
            // coverity[autosar_cpp14_a15_4_2_violation]
            current_send_task_result_ = executor_.Submit([this](const score::cpp::stop_token token) noexcept {
                SendQueueElements(token);
            });
        }
        return score::cpp::expected_blank<score::os::Error>();
    }
}

bool NonBlockingSender::HasNonBlockingGuarantee() const noexcept
{
    return true;
}

NonBlockingSender::~NonBlockingSender()
{
    if (current_send_task_result_.Valid())
    {
        // we aren't interested in the task result
        current_send_task_result_.Abort();

        // to avoid race-conditions, we still wait for the result here.
        score::cpp::ignore = current_send_task_result_.Wait();
    }
}

}  // namespace score::mw::com::message_passing
