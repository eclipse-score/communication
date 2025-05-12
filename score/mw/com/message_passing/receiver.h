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
#ifndef SCORE_MW_COM_MESSAGE_PASSING_RECEIVER_H
#define SCORE_MW_COM_MESSAGE_PASSING_RECEIVER_H

#include "score/concurrency/executor.h"
#include "score/concurrency/task_result.h"
#include "score/os/errno.h"
#include "score/mw/com/message_passing/i_receiver.h"
#include "score/mw/com/message_passing/message.h"
#include "score/mw/com/message_passing/receiver_config.h"
#include "score/mw/com/message_passing/shared_properties.h"

#include <score/callback.hpp>
#include <score/expected.hpp>
#include <score/memory.hpp>
#include <score/stop_token.hpp>
#include <score/string.hpp>
#include <score/unordered_map.hpp>
#include <score/vector.hpp>

#include <cstdint>

#include <algorithm>
#include <iostream>
#include <optional>
#include <string_view>
#include <thread>
#include <utility>
#include <variant>

namespace score::mw::com::message_passing
{

/// \brief Generic implementation of IReceiver parametrized with ChannelTraits.
/// \tparam ChannelTraits The type that describes how to do the actual receiving.
///         Shall behave as if it contains the following members:
///         \code{.cpp}
///             static constexpr std::size_t kConcurrency; /* amount of threads worth running */
///             using file_descriptor_type = sometype; /* behaves like a POSIX handle */
///             static constexpr file_descriptor_type INVALID_FILE_DESCRIPTOR;
///             static score::cpp::expected<file_descriptor_type, score::os::Error>
///                 open_receiver(std::string_view identifier,
///                      const score::cpp::pmr::vector<uid_t>& allowed_uids,
///                      std::int32_t max_number_message_in_queue) noexcept;
///             static void close_receiver(file_descriptor_type file_descriptor,
///                               std::string_view identifier) noexcept;
///             static void stop_receive(file_descriptor_type file_descriptor) noexcept;
///             template <typename ShortMessageProcessor, typename MediumMessageProcessor>
///             static score::cpp::expected<bool, score::os::Error>
///                 receive_next(file_descriptor_type file_descriptor,
///                              ShortMessageProcessor fShort,
///                              MediumMessageProcessor fMedium) noexcept;
///         \endcode
///         ChannelTraits::receive_next() waits for the next message to processes, then calls
///         the corresponding handler and returns true. If ChannelTraits::stop_receive() has been called,
///         ChannelTraits::receive_next() breaks the wait and returns false.
///         If multiple ChannelTraits::receive_next() are running, the matching number of ChannelTraits::stop_receive()
///         shall be called to stop them all.
template <typename ChannelTraits>
class Receiver final : public IReceiver
{
  public:
    using allocator_type = score::cpp::pmr::polymorphic_allocator<Receiver<ChannelTraits>>;
    using FDResourcesType = typename ChannelTraits::FileDescriptorResourcesType;

    /// \brief Constructs a Receiver with respective Receiver to register callbacks.
    /// \param identifier The common identifier between Sender and Receiver for the channel (maps to a path in the
    /// filesystem)
    /// \param executor An executor where the asynchronous blocking listening task can be scheduled
    /// \param allowed_uids A list of uids allowed for senders (if supported by implementation).
    ///        Empty list is equivalent to unrestricted access.
    /// \param receiver_config additional receiver configuration parameters
    /// \param allocator allocator bound to memory resource for allocating the memory required by the Receiver object
    explicit Receiver(const std::string_view identifier,
                      concurrency::Executor& executor,
                      const score::cpp::span<const uid_t> allowed_uids,
                      const ReceiverConfig& receiver_config,
                      const allocator_type& allocator) noexcept;

    /// \brief Stop listening for message, there is no guarantee that all messages that have been send will be received
    ~Receiver() noexcept override;

    // We have an underlying file descriptor which denys copying.
    // We deny moving due to unsure handling of captured this pointer.
    Receiver(const Receiver&) = delete;
    Receiver& operator=(const Receiver&) & = delete;
    Receiver(Receiver&&) = delete;
    Receiver& operator=(Receiver&&) & = delete;

    void Register(const MessageId id, ShortMessageReceivedCallback callback) noexcept override;
    void Register(const MessageId id, MediumMessageReceivedCallback callback) noexcept override;

    score::cpp::expected_blank<score::os::Error> StartListening() noexcept override;

  private:
    void RunListeningThread(score::cpp::stop_token token,
                            const std::size_t thread,
                            const std::size_t max_threads) const noexcept;
    void MessageLoop(const std::size_t thread) const noexcept;
    void ExecuteMessageHandler(const ShortMessage) const noexcept;
    void ExecuteMessageHandler(const MediumMessage) const noexcept;

    concurrency::Executor& executor_;
    score::cpp::pmr::unordered_map<MessageId, std::variant<ShortMessageReceivedCallback, MediumMessageReceivedCallback>>
        registered_callbacks_;
    typename ChannelTraits::file_descriptor_type file_descriptor_;
    score::cpp::pmr::string identifier_;
    score::cpp::pmr::vector<score::concurrency::TaskResult<void>> working_tasks_;
    score::cpp::pmr::vector<uid_t> allowed_uids_;
    std::int32_t max_number_message_in_queue_;
    std::optional<std::chrono::milliseconds> message_loop_delay_;
    FDResourcesType fd_resources_;
};

template <typename ChannelTraits>
Receiver<ChannelTraits>::Receiver(const std::string_view identifier,
                                  concurrency::Executor& executor,
                                  const score::cpp::span<const uid_t> allowed_uids,
                                  const ReceiverConfig& receiver_config,
                                  const allocator_type& allocator) noexcept
    : IReceiver{},
      executor_{executor},
      registered_callbacks_{},
      file_descriptor_{ChannelTraits::INVALID_FILE_DESCRIPTOR},
      identifier_{identifier.cbegin(), identifier.cend(), allocator},
      allowed_uids_{allowed_uids.cbegin(), allowed_uids.cend(), allocator},
      max_number_message_in_queue_{receiver_config.max_number_message_in_queue},
      message_loop_delay_{receiver_config.message_loop_delay},
      fd_resources_{ChannelTraits::GetDefaultOSResources(allocator.resource())}
{
}

template <typename ChannelTraits>
Receiver<ChannelTraits>::~Receiver() noexcept
{
    for (auto& working_task : working_tasks_)
    {
        if (working_task.Valid())  // LCOV_EXCL_BR_LINE :Not possible to make working task as invalid
        {
            working_task.Abort();
            score::cpp::ignore = working_task.Wait();
        }
    }

    if (file_descriptor_ != ChannelTraits::INVALID_FILE_DESCRIPTOR)
    {
        ChannelTraits::close_receiver(file_descriptor_, identifier_, fd_resources_);
    }
}

template <typename ChannelTraits>
auto Receiver<ChannelTraits>::StartListening() noexcept -> score::cpp::expected_blank<score::os::Error>
{
    const auto handle =
        ChannelTraits::open_receiver(identifier_, allowed_uids_, max_number_message_in_queue_, fd_resources_);
    if (handle.has_value() == false)
    {
        return score::cpp::make_unexpected(handle.error());
    }

    file_descriptor_ = handle.value();

    // start waiting for messages
    const std::size_t max_threads = std::min(ChannelTraits::kConcurrency, executor_.MaxConcurrencyLevel());
    for (std::size_t i = 0U; i < max_threads; ++i)
    {
        // Suppress "AUTOSAR C++14 A15-4-2" rule finding. This rule states: "I a function is declared to be
        // noexcept, noexcept(true) or noexcept(<true condition>), then it shall not exit with an exception"
        // We have exceptions disabled by the platform AoUs
        // coverity[autosar_cpp14_a15_4_2_violation]
        score::cpp::ignore =
            working_tasks_.emplace_back(executor_.Submit([this, i, max_threads](const score::cpp::stop_token& token) noexcept {
                this->RunListeningThread(token, i, max_threads);
            }));
    }
    return {};
}

template <typename ChannelTraits>
void Receiver<ChannelTraits>::Register(const MessageId id, ShortMessageReceivedCallback callback) noexcept
{
    score::cpp::ignore = registered_callbacks_.insert({id, std::move(callback)});
}

template <typename ChannelTraits>
void Receiver<ChannelTraits>::Register(const MessageId id, MediumMessageReceivedCallback callback) noexcept
{
    score::cpp::ignore = registered_callbacks_.insert({id, std::move(callback)});
}

template <typename ChannelTraits>
void Receiver<ChannelTraits>::RunListeningThread(score::cpp::stop_token token,
                                                 const std::size_t thread,
                                                 const std::size_t max_threads) const noexcept
{
    // we spawn multiple threads and need to send stop_receive for each thread spawned. However, we cannot guarantee
    // that every callback will stop exactly the thread it was registered in. To avoid a situation when a callback
    // stops another thread, whose callback is then destructed before activation, we will stop all the threads from a
    // single callback (belonging to the thread number 0).
    // An alternative would be a rendezvous point before the return from this (MessageLoop) function (std::latch from
    // C++20), but it would be harder to implement from scratch.
    const score::cpp::stop_token dummy_token;
    const auto message_loop_thread_0_id = std::this_thread::get_id();

    // Suppress "AUTOSAR C++14 M0-1-3" and "AUTOSAR C++14 M0-1-9" rule violations. The rule states
    // "A project shall not contain unused variables." and "There shall be no dead code.", respectively.
    // `stop_callback` installs a handler that is active in the variable's scope.
    // coverity[autosar_cpp14_m0_1_9_violation : FALSE]
    // coverity[autosar_cpp14_m0_1_3_violation : FALSE]
    score::cpp::stop_callback stop{
        thread == 0U ? token : dummy_token, [this, max_threads, message_loop_thread_0_id]() noexcept {
            const bool is_current_thread_message_loop_thread_0 = std::this_thread::get_id() == message_loop_thread_0_id;
            for (std::size_t i = 0U; i < max_threads; ++i)
            {
                if ((i == 0U) && is_current_thread_message_loop_thread_0)
                {
                    // We must not call stop_receive here because we have not entered the message
                    // loop at this point. Calling stop_receive here would result in a dead lock.
                    continue;
                }
                ChannelTraits::stop_receive(file_descriptor_, fd_resources_);
            }
        }};

    // No need to enter the message loop if stop already requested at this point.
    // Entering the loop would mean we never terminate message loop thread 0 because we did not call stop_receive for
    // it (c.f. the stop callback above).
    if (token.stop_requested())
    {
        return;
    }

    MessageLoop(thread);
}

template <typename ChannelTraits>
// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall not
// be called implicitly.".
// The function is noexcept. It uses some calls that are not marked as noexcept, but won't throw exceptions.
// coverity[autosar_cpp14_a15_5_3_violation]
void Receiver<ChannelTraits>::MessageLoop(const std::size_t thread) const noexcept
{
    while (true)
    {
        const auto received = ChannelTraits::receive_next(
            file_descriptor_,
            thread,
            [this](const ShortMessage& message) noexcept {
                ExecuteMessageHandler(message);
            },
            [this](const MediumMessage& message) noexcept {
                ExecuteMessageHandler(message);
            },
            fd_resources_);
        if (received.has_value())
        {
            if (received.value() == false)
            {
                // the channel received the stop request; we shall stop the thread now.
                // In C++20, the rendezvous point would be here.
                return;
            }
        }
        else
        {
            // Using std::endl to std::ostream object with the stream operator follows the idiomatic way that both
            // features in conjunction were designed in the C++ standard.
            // coverity[autosar_cpp14_m8_4_4_violation] See above
            std::cerr << "Could not receive message with error " << received.error() << std::endl;
        }

        if (message_loop_delay_.has_value())
        {
            // Since this is an unbounded loop we artificially limit the processing rate to ensure FFI from
            // misbehaving senders if necessary.
            std::this_thread::sleep_for(message_loop_delay_.value());
        }
    }
}

template <typename ChannelTraits>
// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall not
// be called implicitly.".
// The function is noexcept. It uses some calls that are not marked as noexcept, but won't throw exceptions.
// coverity[autosar_cpp14_a15_5_3_violation]
void Receiver<ChannelTraits>::ExecuteMessageHandler(const ShortMessage message) const noexcept
{
    const auto iterator = registered_callbacks_.find(message.id);
    if (iterator != registered_callbacks_.cend())
    {
        const auto& callback = std::get<ShortMessageReceivedCallback>(iterator->second);
        callback(message.payload, message.pid);
    }
    else
    {
        // Using std::endl to std::ostream object with the stream operator follows the idiomatic way that both
        // features in conjunction were designed in the C++ standard.
        // coverity[autosar_cpp14_m8_4_4_violation] See above
        std::cerr << "No callback registered for message " << message.id << std::endl;
    }
}

template <typename ChannelTraits>
// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall not
// be called implicitly.".
// The function is noexcept. It uses some calls that are not marked as noexcept, but won't throw exceptions.
// coverity[autosar_cpp14_a15_5_3_violation]
void Receiver<ChannelTraits>::ExecuteMessageHandler(const MediumMessage message) const noexcept
{
    const auto iterator = registered_callbacks_.find(message.id);
    if (iterator != registered_callbacks_.cend())
    {
        const auto& callback = std::get<MediumMessageReceivedCallback>(iterator->second);
        callback(message.payload, message.pid);
    }
    else
    {
        // Using std::endl to std::ostream object with the stream operator follows the idiomatic way that both
        // features in conjunction were designed in the C++ standard.
        // coverity[autosar_cpp14_m8_4_4_violation] See above
        std::cerr << "No callback registered for message " << message.id << std::endl;
    }
}

}  // namespace score::mw::com::message_passing

#endif  // SCORE_MW_COM_MESSAGE_PASSING_RECEIVER_H
