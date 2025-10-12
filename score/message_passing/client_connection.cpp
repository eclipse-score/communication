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

// TODO: usleep only; remove if we decide to go back to assert stopped on destruction
#include <unistd.h>
#include <iostream>

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
      send_storage_{client_config.max_queued_sends + client_config.max_async_replies,
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
        send_command.message.reserve(max_send_size_);
    }
    send_pool_.assign(send_storage_.begin(), send_storage_.end());
}

ClientConnection::~ClientConnection() noexcept
{
    // TODO: we will need to decide if the second approach (force-stopping instead of asserting on being stopped)
    // is more viable. If it is, we will need to update the design docs.
#if 0
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(state_ == State::kStopped);
    {
        // if we are not called from the state callback, wait until it finishes
        std::lock_guard<std::recursive_mutex> guard{callback_context_->finalize_mutex};
    }
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(state_ == State::kStopped);
#else
    if (state_ == State::kStopped)
    {
        // if we are not called from the state callback, wait until it finishes
        std::lock_guard<std::recursive_mutex> guard{callback_context_->finalize_mutex};
    }
    else
    {
        Stop();
        while (state_ != State::kStopped)
        {
            score::cpp::ignore = usleep(10000);
        }
        std::lock_guard<std::recursive_mutex> guard{callback_context_->finalize_mutex};
    }
#endif
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
            // here we are temporarily blocking async_send_command_ from attempts to push it several times into the
            // queue; waiting_for_reply_ will be released in ProcessSendQueueUnderLock()
            waiting_for_reply_ = ReplyCallback{};

            if (!TryQueueMessage(message, ReplyCallback{}))
            {
                return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOMEM));
            }
            engine_->EnqueueCommand(
                async_send_command_,
                ISharedResourceEngine::TimePoint{},
                [this](auto) noexcept {
                    std::lock_guard<std::mutex> lock(send_mutex_);
                    ProcessSendQueueUnderLock();
                },
                this);
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
        const auto expected =
            engine_->SendProtocolMessage(client_fd_, score::cpp::to_underlying(ClientToServer::REQUEST), message);
        if (!expected.has_value())
        {
            return score::cpp::make_unexpected(expected.error());
        }
    }

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
        waiting_for_reply_ = ReplyCallback{};
        if (!TryQueueMessage(message, std::move(callback)))
        {
            return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOMEM));
        }
        engine_->EnqueueCommand(
            async_send_command_,
            ISharedResourceEngine::TimePoint{},
            [this](auto) noexcept {
                std::lock_guard<std::mutex> lock(send_mutex_);
                ProcessSendQueueUnderLock();
            },
            this);
    }
    else
    {
        waiting_for_reply_ = std::move(callback);
        const auto expected =
            engine_->SendProtocolMessage(client_fd_, score::cpp::to_underlying(ClientToServer::REQUEST), message);
        if (!expected.has_value())
        {
            return score::cpp::make_unexpected(expected.error());
        }
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
    if (state_ != State::kStopped || stop_reason_ == StopReason::kShutdown)
    {
        return;
    }

    DoRestart();
}

void ClientConnection::DoRestart() noexcept
{
    stop_reason_ = StopReason::kNone;
    ProcessStateChange(State::kStarting);

    std::cerr << "ClientConnection::DoRestart " << engine_->IsOnCallbackThread() << " " << identifier_ << " "
              << std::endl;

    connect_retry_ms_ = kConnectRetryMsStart;
    engine_->EnqueueCommand(
        connection_timer_,
        ISharedResourceEngine::TimePoint{},
        [this](auto) noexcept {
            TryConnect();
        },
        this);
}

void ClientConnection::TryConnect() noexcept
{
    // The order of access to these atomics is important, as stop_reason_ can change in background
    SCORE_LANGUAGE_FUTURECPP_ASSERT(((stop_reason_ == StopReason::kNone) && (state_ == State::kStarting)) ||
               ((state_ == State::kStopping) && (stop_reason_ == StopReason::kUserRequested)));
    SCORE_LANGUAGE_FUTURECPP_ASSERT(IsInCallback());

    std::cerr << "TryOpenClientConnection " << identifier_ << std::endl;
    auto fd_expected = engine_->TryOpenClientConnection(identifier_);
    if (!fd_expected.has_value())
    {
        auto error = fd_expected.error();
        auto os_code = error.GetOsDependentErrorCode();
        if (os_code != EAGAIN && os_code != ECONNREFUSED && os_code != ENOENT)
        {
            StopReason stop_reason = os_code == EACCES ? StopReason::kPermission : StopReason::kIoError;
            if (TrySetStopReason(stop_reason))
            {
                ProcessStateChange(State::kStopping);
                SwitchToStopState();
            }
            return;
        }
        std::int32_t retry_delay = connect_retry_ms_;
        connect_retry_ms_ += (connect_retry_ms_ + kConnectRetryT - 1) / kConnectRetryT;
        if (connect_retry_ms_ > kConnectRetryMsMax)
        {
            connect_retry_ms_ = kConnectRetryMsMax;
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
    engine_->RegisterPosixEndpoint(posix_endpoint_);

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
    switch (code)
    {
        case score::cpp::to_underlying(ServerToClient::REPLY):
        {
            std::unique_lock<std::mutex> lock{send_mutex_};
            if (waiting_for_reply_)
            {
                ReplyCallback callback = std::move(*waiting_for_reply_);
                waiting_for_reply_.reset();
                ProcessSendQueueUnderLock();
                lock.unlock();
                callback(message);
            }
            else
            {
                return StopReason::kIoError;
            }
        }
        break;
        case score::cpp::to_underlying(ServerToClient::NOTIFY):
            if (!notify_callback_.empty())
            {
                notify_callback_(message);
            }
            break;

        default:
            // unrecognised message; drop connection
            return StopReason::kIoError;
    }
    return StopReason::kNone;
}

void ClientConnection::ProcessSendQueueUnderLock() noexcept
{
    while (!send_queue_.empty())
    {
        SendCommand& send = send_queue_.front();
        send_queue_.pop_front();
        send_pool_.push_front(send);  // LIFO for better cache locality
        if (!send.callback.empty())
        {
            waiting_for_reply_ = std::move(send.callback);
            // NOLINTNEXTLINE(score-no-unnamed-temporary-objects) TODO:
            engine_->SendProtocolMessage(client_fd_, score::cpp::to_underlying(ClientToServer::REQUEST), send.message);
            break;
        }
        // NOLINTNEXTLINE(score-no-unnamed-temporary-objects) TODO:
        engine_->SendProtocolMessage(client_fd_, score::cpp::to_underlying(ClientToServer::SEND), send.message);
    }
}

void ClientConnection::SwitchToStopState() noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT(state_ == State::kStopping);
    SCORE_LANGUAGE_FUTURECPP_ASSERT(stop_reason_ != StopReason::kNone);
    SCORE_LANGUAGE_FUTURECPP_ASSERT(IsInCallback());
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
        std::shared_ptr<CallbackContext> context = callback_context_;
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
