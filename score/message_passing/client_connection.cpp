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
#include "score/message_passing/client_connection.h"

#include "score/message_passing/client_server_communication.h"
#include "score/message_passing/non_allocating_future/non_allocating_future.h"

#include <score/utility.hpp>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <thread>

namespace score
{
namespace message_passing
{
namespace detail
{

namespace
{
constexpr std::int32_t kConnectRetryMsStart = 50;
constexpr std::int32_t kConnectRetryT = 3;  // new_delay = prev_delay * (1 + 1/T)
constexpr std::int32_t kConnectRetryMsMax = 5000;
}  // namespace

ClientConnection::ClientConnection(std::shared_ptr<ISharedResourceEngine> engine,
                                   const ServiceProtocolConfig& protocol_config,
                                   const IClientFactory::ClientConfig& client_config) noexcept
    : IClientConnection{},
      engine_{std::move(engine)},
      identifier_{protocol_config.identifier.data(), protocol_config.identifier.size(), engine_->GetMemoryResource()},
      max_send_size_{protocol_config.max_send_size},
      max_receive_size_{std::max(protocol_config.max_reply_size, protocol_config.max_notify_size)},
      client_config_{client_config},
      client_fd_{-1},
      state_{State::kStopped},
      stop_reason_{StopReason::kInit},
      callback_context_{score::cpp::pmr::make_shared<CallbackContext>(engine_->GetMemoryResource())},
      notify_callback_{},
      connect_retry_ms_{kConnectRetryMsStart},
      send_mutex_{},
      send_condition_{},
      send_storage_{static_cast<std::size_t>(client_config.max_queued_sends) +
                        static_cast<std::size_t>(client_config.max_async_replies),
                    score::cpp::pmr::polymorphic_allocator<>(engine_->GetMemoryResource())},
      send_pool_{},
      send_queue_{},
      waiting_for_reply_{},
      connection_timer_{},
      disconnection_command_{},
      async_send_command_{},
      posix_endpoint_{}
{
    // TODO: separate max_queued_sends, max_async_replies, and maybe queued SendWaitReply
    for (auto& send_command : send_storage_)
    {
        send_command.message.reserve(static_cast<std::size_t>(max_send_size_));
    }
    send_pool_.assign(send_storage_.begin(), send_storage_.end());
}

ClientConnection::~ClientConnection() noexcept
{
    if (state_ == State::kStopped)
    {
        // if we are not called from the state callback, wait until it finishes
        std::lock_guard<std::recursive_mutex> guard{callback_context_->finalize_mutex};
    }
    else
    {
        ClientConnection::Stop();
        while (state_ != State::kStopped)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        std::lock_guard<std::recursive_mutex> guard{callback_context_->finalize_mutex};
    }
    send_pool_.clear();
}

bool ClientConnection::TryQueueMessage(score::cpp::span<const std::uint8_t> message, ReplyCallback callback) noexcept
{
    if (send_pool_.empty())
    {
        return false;
    }
    auto& send_command = send_pool_.front();
    send_pool_.pop_front();
    send_command.message.assign(message.begin(), message.end());
    send_command.callback = std::move(callback);
    send_queue_.push_back(send_command);
    return true;
}

score::cpp::expected_blank<score::os::Error> ClientConnection::Send(score::cpp::span<const std::uint8_t> message) noexcept
{
    if (message.size() > max_send_size_)
    {
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOMEM));
    }
    if (state_ != State::kReady)
    {
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(EINVAL));
    }
    if (!client_config_.fully_ordered && !client_config_.truly_async)
    {
        return engine_->SendProtocolMessage(client_fd_, score::cpp::to_underlying(ClientToServer::SEND), message);
    }
    std::lock_guard<std::mutex> guard{send_mutex_};
    if (!waiting_for_reply_.has_value())
    {
        if (client_config_.truly_async)
        {
            if (!TryQueueMessage(message, ReplyCallback{}))
            {
                return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOMEM));
            }
            ArmSendQueueUnderLock();
        }
        else
        {
            return engine_->SendProtocolMessage(client_fd_, score::cpp::to_underlying(ClientToServer::SEND), message);
        }
    }
    else
    {
        if (!TryQueueMessage(message, ReplyCallback{}))
        {
            return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOMEM));
        }
    }
    return {};
}

score::cpp::expected<score::cpp::span<const std::uint8_t>, score::os::Error> ClientConnection::SendWaitReply(
    score::cpp::span<const std::uint8_t> message,
    score::cpp::span<std::uint8_t> reply) noexcept
{
    if (IsInCallback())
    {
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(EAGAIN));
    }
    if (message.size() > max_send_size_)
    {
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOMEM));
    }
    if (state_ != State::kReady)
    {
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(EINVAL));
    }

    score::cpp::expected<score::cpp::span<const std::uint8_t>, score::os::Error> result{};
    NonAllocatingFuture future(send_mutex_, send_condition_, result);

    auto callback = [&reply, &future](score::cpp::expected<score::cpp::span<const std::uint8_t>, score::os::Error> message_expected) {
        if (!message_expected.has_value())
        {
            future.UpdateValueMarkReady(score::cpp::make_unexpected(message_expected.error()));
            return;
        }
        const auto reply_message = message_expected.value();
        if (reply_message.size() > reply.size())
        {
            future.UpdateValueMarkReady(score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOMEM)));
            return;
        }
        // NOLINTBEGIN(bugprone-suspicious-stringview-data-usage) `reply`'s buffer size got checked above
        // NOLINTNEXTLINE(score-banned-function) copying byte buffer
        score::cpp::ignore = std::memcpy(reply.data(), reply_message.data(), reply_message.size());
        future.UpdateValueMarkReady(score::cpp::span<const std::uint8_t>{reply.data(), reply_message.size()});
        // NOLINTEND(bugprone-suspicious-stringview-data-usage)
    };

    std::unique_lock<std::mutex> lock(send_mutex_);
    if (waiting_for_reply_.has_value())
    {
        // TODO: avoid copying the message

        // Suppress "AUTOSAR C++14 A5-1-4" rule finding: "A lambda expression object shall not outlive any of its
        // reference-captured objects.".
        // Either the callback is destructed inside the TryQueueMessage call, or it is queued and then fired only once
        // unblocking the send_condition_ while holding send_mutex_. We don't access the referenced values after
        // the send_condition_ is unblocked and we don't leave the SendWaitReply function scope before it's unblocked.
        // coverity[autosar_cpp14_a5_1_4_violation]
        if (!TryQueueMessage(message, std::move(callback)))
        {
            return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOMEM));
        }
    }
    else
    {
        waiting_for_reply_ = std::move(callback);
        lock.unlock();
        const auto expected =
            engine_->SendProtocolMessage(client_fd_, score::cpp::to_underlying(ClientToServer::REQUEST), message);
        lock.lock();
        if (!expected.has_value())
        {
            if (send_queue_.empty())
            {
                // no one managed to get into queue while we were blocking it
                waiting_for_reply_.reset();
            }
            else
            {
                // unblock the queue
                ArmSendQueueUnderLock();
            }
            return score::cpp::make_unexpected(expected.error());
        }
    }
    lock.unlock();

    future.Wait();
    return result;
}

score::cpp::expected_blank<score::os::Error> ClientConnection::SendWithCallback(score::cpp::span<const std::uint8_t> message,
                                                                       ReplyCallback callback) noexcept
{
    if (message.size() > max_send_size_)
    {
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOMEM));
    }
    if (state_ != State::kReady)
    {
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(EINVAL));
    }
    std::lock_guard<std::mutex> guard(send_mutex_);
    if (waiting_for_reply_.has_value())
    {
        if (!TryQueueMessage(message, std::move(callback)))
        {
            return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOMEM));
        }
        return {};
    }
    if (client_config_.truly_async)
    {
        if (!TryQueueMessage(message, std::move(callback)))
        {
            return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOMEM));
        }
        ArmSendQueueUnderLock();
    }
    else
    {
        const auto expected =
            engine_->SendProtocolMessage(client_fd_, score::cpp::to_underlying(ClientToServer::REQUEST), message);
        if (!expected.has_value())
        {
            return score::cpp::make_unexpected(expected.error());
        }
        waiting_for_reply_ = std::move(callback);
    }
    return {};
}

IClientConnection::State ClientConnection::GetState() const noexcept
{
    return state_;
}

IClientConnection::StopReason ClientConnection::GetStopReason() const noexcept
{
    return stop_reason_;
}

void ClientConnection::Start(StateCallback state_callback, NotifyCallback notify_callback) noexcept
{
    callback_context_->state_callback = std::move(state_callback);
    notify_callback_ = std::move(notify_callback);

    DoRestart();
}

void ClientConnection::Stop() noexcept
{
    if (TrySetStopReason(StopReason::kUserRequested))
    {
        ProcessStateChange(State::kStopping);
        if (IsInCallback())
        {
            SwitchToStopState();
        }
        else
        {
            engine_->EnqueueCommand(
                disconnection_command_,
                ISharedResourceEngine::TimePoint{},
                [this](auto) noexcept {
                    SwitchToStopState();
                },
                this);
        }
    }
}

void ClientConnection::Restart() noexcept
{
    if (state_ != State::kStopped)
    {
        return;
    }

    DoRestart();
}

void ClientConnection::DoRestart() noexcept
{
    stop_reason_ = StopReason::kNone;
    ProcessStateChange(State::kStarting);

    // Suppress AUTOSAR C++14 M8-4-4, rule finding: "A function identifier shall either be used to call the
    // function or it shall be preceded by &".
    // We deviate from this rule (which is also gone in Misra C++23).
    // Passing std::endl to std::cerr is the idiomatic way to print an error in C++.
    std::cerr << "ClientConnection::DoRestart " << engine_->IsOnCallbackThread() << " " << identifier_
              << " "
              // coverity[autosar_cpp14_m8_4_4_violation]
              << std::endl;

    connect_retry_ms_ = kConnectRetryMsStart;
    if (client_config_.sync_first_connect)
    {
        TryConnect();
    }
    else
    {
        engine_->EnqueueCommand(
            connection_timer_,
            ISharedResourceEngine::TimePoint{},
            [this](auto) noexcept {
                TryConnect();
            },
            this);
    }
}

void ClientConnection::TryConnect() noexcept
{
    // The order of access to these atomics is important, as stop_reason_ can change in background
    SCORE_LANGUAGE_FUTURECPP_ASSERT_DBG(((stop_reason_ == StopReason::kNone) && (state_ == State::kStarting)) ||
                   ((state_ == State::kStopping) && (stop_reason_ == StopReason::kUserRequested)));

    // Suppress AUTOSAR C++14 M8-4-4, rule finding: "A function identifier shall either be used to call the
    // function or it shall be preceded by &".
    // We deviate from this rule (which is also gone in Misra C++23).
    // Passing std::endl to std::cerr is the idiomatic way to print an error in C++.
    // coverity[autosar_cpp14_m8_4_4_violation]
    std::cerr << "TryOpenClientConnection " << identifier_ << std::endl;
    auto fd_expected = engine_->TryOpenClientConnection(identifier_);
    if (!fd_expected.has_value())
    {
        auto error = fd_expected.error();
        auto os_code = error.GetOsDependentErrorCode();
        if (((os_code != EAGAIN) && (os_code != ECONNREFUSED)) && (os_code != ENOENT))
        {
            // Suppress AUTOSAR C++14 M8-4-4, rule finding: "A function identifier shall either be used to call the
            // function or it shall be preceded by &".
            // We deviate from this rule (which is also gone in Misra C++23).
            // Passing std::endl to std::cerr is the idiomatic way to print an error in C++.
            // coverity[autosar_cpp14_m8_4_4_violation]
            std::cerr << "TryOpenClientConnection " << identifier_ << " non-retry OS error code " << os_code
                      << std::endl;
            StopReason stop_reason = os_code == EACCES ? StopReason::kPermission : StopReason::kIoError;
            if (TrySetStopReason(stop_reason))
            {
                ProcessStateChange(State::kStopping);
                SwitchToStopState();
            }
            return;
        }
        std::int32_t retry_delay = connect_retry_ms_;

        const auto retry_increase_ms =
            (static_cast<std::int64_t>(connect_retry_ms_) + static_cast<std::int64_t>(kConnectRetryT) - 1) /
            static_cast<std::int64_t>(kConnectRetryT);
        if ((retry_increase_ms <= kConnectRetryMsMax) && (connect_retry_ms_ <= kConnectRetryMsMax - retry_increase_ms))
        {
            // At this point checks guarantee no data loss
            // coverity[autosar_cpp14_a4_7_1_violation]
            connect_retry_ms_ += static_cast<std::int32_t>(retry_increase_ms);
        }

        engine_->EnqueueCommand(
            connection_timer_,
            engine_->FromNow(std::chrono::milliseconds(retry_delay)),
            [this](auto) noexcept {
                TryConnect();
            },
            this);
        return;
    }
    client_fd_ = fd_expected.value();
    posix_endpoint_.owner = this;
    posix_endpoint_.fd = client_fd_;
    posix_endpoint_.max_receive_size = max_receive_size_;
    posix_endpoint_.input = [this]() noexcept {
        StopReason stop_reason = ProcessInputEvent();
        if ((stop_reason != StopReason::kNone) && TrySetStopReason(stop_reason))
        {
            ProcessStateChange(State::kStopping);
            engine_->UnregisterPosixEndpoint(posix_endpoint_);
        }
    };
    posix_endpoint_.output = {};
    posix_endpoint_.disconnect = [this]() noexcept {
        SwitchToStopState();
    };
    if (IsInCallback())
    {
        engine_->RegisterPosixEndpoint(posix_endpoint_);
    }
    else
    {
        engine_->EnqueueCommand(
            connection_timer_,
            ISharedResourceEngine::TimePoint{},
            [this](auto) noexcept {
                engine_->RegisterPosixEndpoint(posix_endpoint_);
            },
            this);
    }

    ProcessStateChange(State::kReady);
}

IClientConnection::StopReason ClientConnection::ProcessInputEvent() noexcept
{
    std::uint8_t code{};
    auto message_expected = engine_->ReceiveProtocolMessage(client_fd_, code);
    if (!message_expected.has_value())
    {
        auto os_code = message_expected.error().GetOsDependentErrorCode();
        return os_code == EPIPE ? StopReason::kClosedByPeer : StopReason::kIoError;
    }
    auto message = message_expected.value();
    // This switch statement is considered not well-formed due to early exits, i.e. return statements.
    // New Misra rule 9.4.2 allows terminating switch statements with a return statement
    // coverity[autosar_cpp14_m6_4_3_violation]
    switch (code)
    {
        case score::cpp::to_underlying(ServerToClient::REPLY):
        {
            std::unique_lock<std::mutex> lock{send_mutex_};
            if (waiting_for_reply_.has_value())
            {
                ReplyCallback callback = std::move(*waiting_for_reply_);
                waiting_for_reply_.reset();
                ProcessSendQueueUnderLock(lock);
                lock.unlock();
                callback(message);
            }
            else
            {
                return StopReason::kIoError;
            }
            break;
        }
        case score::cpp::to_underlying(ServerToClient::NOTIFY):
        {
            if (!notify_callback_.empty())
            {
                notify_callback_(message);
            }
            break;
        }
        // New Misra rule 9.4.2 allows terminating switch statements with a return statement
        // coverity[autosar_cpp14_m6_4_5_violation]
        default:
        {
            // unrecognised message; drop connection
            return StopReason::kIoError;
        }
    }
    return StopReason::kNone;
}

void ClientConnection::ArmSendQueueUnderLock() noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_DBG(!waiting_for_reply_.has_value());

    // here we are temporarily blocking async_send_command_ from attempts to push it several times into the
    // queue; waiting_for_reply_ will be released in ProcessSendQueueUnderLock()
    waiting_for_reply_ = ReplyCallback{};
    engine_->EnqueueCommand(
        async_send_command_,
        ISharedResourceEngine::TimePoint{},
        [this](auto) noexcept {
            std::unique_lock<std::mutex> lock(send_mutex_);
            ProcessSendQueueUnderLock(lock);
        },
        this);
}

void ClientConnection::ProcessSendQueueUnderLock(std::unique_lock<std::mutex>& lock) noexcept
{
    while (!send_queue_.empty())
    {
        SendCommand& send = send_queue_.front();
        send_queue_.pop_front();
        send_pool_.push_front(send);  // LIFO for better cache locality
        if (!send.callback.empty())
        {
            waiting_for_reply_ = std::move(send.callback);
            // waiting_for_reply_ is now guaranteed to be occupied. This forces other potential fully_ordered or
            // truly_async senders to push their messages into the send_queue_. We neeed to unlock that queue
            // temporarily, as the other side of SendProtocolMessage is not under our control and it may cause
            // indefinite delay.
            lock.unlock();
            const auto expected =
                engine_->SendProtocolMessage(client_fd_, score::cpp::to_underlying(ClientToServer::REQUEST), send.message);
            lock.lock();
            if (expected.has_value())
            {
                break;
            }
            auto callback = std::move(*waiting_for_reply_);
            // We are not releasing waiting_for_reply_ yet. This makes our life easier if the callback and/or some
            // competing thread wants to queue another message - they will just add it to the send_queue_ and we will
            // process it in the next iteration.
            lock.unlock();
            callback(score::cpp::make_unexpected(expected.error()));
            lock.lock();
            waiting_for_reply_.reset();
        }
        else
        {
            // Temporarily make waiting_for_reply_ occupied to activate send_queue_ for fully_ordered or truly_async
            // senders and release the queue lock for the duration of SendProtocolMessage.
            waiting_for_reply_ = ReplyCallback{};
            lock.unlock();
            // nowhere to return the potential error
            score::cpp::ignore =
                engine_->SendProtocolMessage(client_fd_, score::cpp::to_underlying(ClientToServer::SEND), send.message);
            lock.lock();
            waiting_for_reply_.reset();
        }
    }
}

void ClientConnection::SwitchToStopState() noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_DBG(state_ == State::kStopping);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_DBG(stop_reason_ != StopReason::kNone);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_DBG(IsInCallback());
    posix_endpoint_.disconnect = {};  // no need to trigger the cleanup once more
    engine_->CleanUpOwner(this);
    if (client_fd_ != -1)
    {
        engine_->CloseClientConnection(client_fd_);
        client_fd_ = -1;
    }
    std::unique_lock<std::mutex> lock{send_mutex_};
    if (waiting_for_reply_)
    {
        auto callback = std::move(*waiting_for_reply_);
        waiting_for_reply_.reset();
        lock.unlock();
        if (!callback.empty())
        {
            callback(score::cpp::make_unexpected(score::os::Error::createFromErrno(EPIPE)));
        }
        lock.lock();
    }
    while (!send_queue_.empty())
    {
        auto callback = std::move(send_queue_.front().callback);
        send_queue_.pop_front();
        if (!callback.empty())
        {
            lock.unlock();
            callback(score::cpp::make_unexpected(score::os::Error::createFromErrno(EPIPE)));
            lock.lock();
        }
    }
    lock.unlock();
    ProcessStateChange(State::kStopped);
}

bool ClientConnection::TrySetStopReason(const StopReason stop_reason) noexcept
{
    // Suppress AUTOSAR C++14 M8-4-4, rule finding: "A function identifier shall either be used to call the
    // function or it shall be preceded by &".
    // We deviate from this rule (which is also gone in Misra C++23).
    // Passing std::endl to std::cerr is the idiomatic way to print an error in C++.
    // coverity[autosar_cpp14_m8_4_4_violation]
    std::cerr << "TrySetStopReason " << static_cast<std::uint32_t>(score::cpp::to_underlying(stop_reason)) << std::endl;

    // can happen via 2 paths: from callback thread and from Stop() call; if happens concurrently, the later attempt
    // needs to be ignored and its consequent attempt to switch to stop state needs to be suppressed
    StopReason old_stop_reason = stop_reason_.load();
    if (old_stop_reason != StopReason::kNone)
    {
        return false;
    }
    return stop_reason_.compare_exchange_strong(old_stop_reason, stop_reason);
}

void ClientConnection::ProcessStateChangeToStopped() noexcept
{
    if (callback_context_->state_callback.empty())
    {
        state_ = State::kStopped;
    }
    else
    {
        std::shared_ptr<CallbackContext> context{callback_context_};
        std::lock_guard<std::recursive_mutex> guard{context->finalize_mutex};
        // we need to be aware of three non-trivial cases here:
        // 1. destructor called from state_callback (normal use)
        // 2. Restart() called from state_callback (normal use)
        // 3. destructor called from another thread when it sees Stopped state (not recommended use)
        state_ = State::kStopped;
        context->state_callback(state_);
    }
}

void ClientConnection::ProcessStateChange(const State state) noexcept
{
    // Suppress AUTOSAR C++14 M8-4-4, rule finding: "A function identifier shall either be used to call the
    // function or it shall be preceded by &".
    // We deviate from this rule (which is also gone in Misra C++23).
    // Passing std::endl to std::cerr is the idiomatic way to print an error in C++.
    // coverity[autosar_cpp14_m8_4_4_violation]
    std::cerr << "ProcessStateChange " << static_cast<std::uint32_t>(score::cpp::to_underlying(state)) << std::endl;
    if (state != State::kStopped)
    {
        state_ = state;
        if (!callback_context_->state_callback.empty())
        {
            callback_context_->state_callback(state_);
        }
    }
    else
    {
        ProcessStateChangeToStopped();
    }
}

}  // namespace detail
}  // namespace message_passing
}  // namespace score
