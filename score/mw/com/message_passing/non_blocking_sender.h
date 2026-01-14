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
#ifndef SCORE_MW_COM_MESSAGE_PASSING_NONBLOCKINGSENDER_H
#define SCORE_MW_COM_MESSAGE_PASSING_NONBLOCKINGSENDER_H

#include "score/concurrency/executor.h"
#include "score/concurrency/task_result.h"
#include "score/memory/pmr_ring_buffer.h"
#include "score/mw/com/message_passing/i_sender.h"

#include <score/expected.hpp>
#include <score/memory.hpp>

#include <mutex>
#include <variant>
#include <vector>

namespace score::mw::com::message_passing
{

/// \brief This class provides a wrapper around any ISender implementation, which assures a non/never blocking behaviour
///        on Send() calls.
/// \attention It makes no sense to use it wrapping an ISender implementation, which already assures non-blocking!
///
/// \details Because of safety (ASIL-B) requirements, it is not acceptable, that a ASIL-B sender gets eventually
///          blocked by a ASIL-QM receiver (at least we want to prevent it, even if the ASIL-B app does its own runtime
///          supervision/watchdog mechanism).
///          The underlying OS specific implementations of ISender/IReceiver vary on their behaviour! Even if they all
///          need to be async to fulfil ISender contract, there is still a major difference between "async" and a
///          "non-blocking-guarantee"!
///          E.g. in QNX we currently use a ISender/IReceiver implementation based on QNX IPC-messaging. Since in QNX
///          (microkernel) there are no kernel buffers, which decouple ISender/IReceiver, a Send() call in our impl.
///          leads to a transition from sender proc to receiver proc, where our receiver impl. takes the message, queues
///          it in a locally managed queue for deferred processing and directly unblocks the sender again.
///          So in normal operation this is the most efficient solution in QNX and fully async by nature. But in case
///          some untrusted QM code within the receiver process compromises our reception thread (hinders its queueing/
///          quick ack to the sender), we could run into a "blocking" behaviour!
///
class NonBlockingSender final : public ISender
{
  public:
    /// \brief ctor for NonBlockingSender
    /// \param wrapped_sender a potentially blocking sender to be wrapped
    /// \param max_queue_size queue size to be used.
    /// \param executor execution policy to be used to call wrapped sender Send() calls from queue. As only one task at
    ///                 a time will be submitted anyhow, maxConcurrencyLevel of the executor needs only to be 1!
    // coverity[autosar_cpp14_a7_1_7_violation] false-positive: formatted this way by clang-format
    NonBlockingSender(score::cpp::pmr::unique_ptr<ISender> wrapped_sender,
                      const std::size_t max_queue_size,
                      concurrency::Executor& executor);

    ~NonBlockingSender() noexcept override;

    // Since queue_ is based on score::memory::PmrRingBuffer, cannot be moved or copied
    NonBlockingSender(const NonBlockingSender&) = delete;
    NonBlockingSender(NonBlockingSender&&) noexcept = delete;
    NonBlockingSender& operator=(const NonBlockingSender&) = delete;
    NonBlockingSender& operator=(NonBlockingSender&&) noexcept = delete;

    /// \brief Sends a ShortMessage with non-blocking guarantee.
    /// \param message message to be sent.
    /// \return See SendInternal()
    score::cpp::expected_blank<score::os::Error> Send(const message_passing::ShortMessage& message) noexcept override;

    /// \brief Sends a MediumMessage with non-blocking guarantee.
    /// \param message message to be sent.
    /// \return See SendInternal()
    score::cpp::expected_blank<score::os::Error> Send(const message_passing::MediumMessage& message) noexcept override;

    /// \brief Returns true as non-blocking guarantee is the job of tis wrapper.
    /// \return true
    bool HasNonBlockingGuarantee() const noexcept override;

  private:
    // coverity[autosar_cpp14_a0_1_1_violation] false-positive: is used in constructor
    static constexpr std::size_t QUEUE_SIZE_UPPER_LIMIT = 100U;

    /// \brief Function called by callable posted to executor. Takes message from queue front and calls Send() on the
    ///        wrapped sender and removes queue entry afterwards.
    /// \pre There is at least one element/message in the queue.
    /// \details If stop has been already requested, no Send() call is done. After Send() (independent of outcome) the
    ///          queue element is removed and if there is still a queue element left a new callable calling this very
    ///          same function is posted to the executor.
    /// \param token stop_token provided by executor.
    // coverity[autosar_cpp14_m7_3_1_violation] false-positive: class method (Ticket-234468)
    void SendQueueElements(const score::cpp::stop_token token) noexcept;

    /// \brief internal Send function taking either a short or medium message to be sent.
    /// \param message
    /// \return only returns an error (score::os::Error::Code::kResourceTemporarilyUnavailable) if queue is full or for
    ///         the underlying executor already shutdown was requested!
    ///         Any Send-errors encountered async, when sending internally from the queue will not be returned back.
    score::cpp::expected_blank<score::os::Error> SendInternal(const std::variant<ShortMessage, MediumMessage> message) noexcept;

    score::memory::PmrRingBuffer<std::variant<ShortMessage, MediumMessage>> queue_;
    std::mutex queue_mutex_;
    score::cpp::pmr::unique_ptr<ISender> wrapped_sender_;
    concurrency::Executor& executor_;
    /// \brief we store the task result of latest submit call to executor to be able to abort it in case of our
    ///        destruction, to avoid race conditions!
    concurrency::TaskResult<void> current_send_task_result_;
};

}  // namespace score::mw::com::message_passing

#endif  // SCORE_MW_COM_MESSAGE_PASSING_NONBLOCKINGSENDER_H
