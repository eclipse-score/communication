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
#include "score/message_passing/log/log.h"
#include "score/message_passing/log/log_on_timeout.h"

#include <score/utility.hpp>

#include <cstddef>
#include <cstdint>
#include <thread>
#include <utility>

namespace score::message_passing::detail
{

namespace
{
constexpr std::int32_t kConnectRetryMsStart = 50;
constexpr std::int32_t kConnectRetryT = 3;  // new_delay = prev_delay * (1 + 1/T)
constexpr std::int32_t kConnectRetryMsMax = 5000;

constexpr std::chrono::milliseconds kConnectIpcWarningDelay{20};
}  // namespace

ClientConnection::ClientConnection(std::shared_ptr<ISharedResourceEngine> engine,
                                   const ServiceProtocolConfig& protocol_config,
                                   const IClientFactory::ClientConfig& client_config) noexcept
    : IClientConnection{},
      engine_{std::move(engine)},
      identifier_{protocol_config.identifier.data(), protocol_config.identifier.size(), engine_->GetMemoryResource()},
      max_receive_size_{std::max(protocol_config.max_reply_size, protocol_config.max_notify_size)},
      client_config_{client_config},
      state_{State::kStopped},
      stop_reason_{StopReason::kInit},
      callback_context_{score::cpp::pmr::make_shared<CallbackContext>(engine_->GetMemoryResource())},
      notify_callback_{},
      connect_retry_ms_{kConnectRetryMsStart},
      lifetime_{state_,
                stop_reason_,
                [this]() noexcept {
                    ScheduleStopFinalization();
                }},
      sends_{lifetime_,
             engine_->GetMemoryResource(),
             client_config,
             protocol_config.max_send_size,
             [this](std::int32_t descriptor, std::uint8_t code, ClientSendQueue::Message message) noexcept {
                 return engine_->SendProtocolMessage(descriptor, code, message);
             },
             [this]() noexcept {
                 ScheduleSendDrain();
             }},
      connection_timer_{},
      disconnection_command_{},
      async_send_command_{},
      posix_endpoint_{}
{
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
}

score::cpp::expected_blank<score::os::Error> ClientConnection::Send(
    score::cpp::span<const std::uint8_t> message) noexcept
{
    return sends_.Send(message);
}

score::cpp::expected<score::cpp::span<const std::uint8_t>, score::os::Error> ClientConnection::SendWaitReply(
    score::cpp::span<const std::uint8_t> message,
    score::cpp::span<std::uint8_t> reply) noexcept
{
    if (IsInCallback())
    {
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(EAGAIN));
    }
    return sends_.SendWaitReply(message, reply);
}

score::cpp::expected_blank<score::os::Error> ClientConnection::SendWithCallback(
    score::cpp::span<const std::uint8_t> message,
    ReplyCallback callback) noexcept
{
    return sends_.SendWithCallback(message, std::move(callback));
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

    LogInfo(engine_->GetLogger(), "ClientConnection::DoRestart ", engine_->IsOnCallbackThread(), " ", identifier_);

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

    const auto& logger = engine_->GetLogger();

    LogInfo(logger, "TryOpenClientConnection ", identifier_);
    LogWarnOnTimeout delay_guard(
        logger, kConnectIpcWarningDelay, "TryOpenClientConnection", std::string_view{identifier_});
    auto fd_expected = engine_->TryOpenClientConnection(identifier_);
    delay_guard.release();
    if (!fd_expected.has_value())
    {
        auto error = fd_expected.error();
        auto os_code = error.GetOsDependentErrorCode();
        if (((os_code != EAGAIN) && (os_code != ECONNREFUSED)) && (os_code != ENOENT))
        {
            LogError(logger, "TryOpenClientConnection ", identifier_, " non-retry OS error code ", os_code);
            StopReason stop_reason = (os_code == EACCES) ? StopReason::kPermission : StopReason::kIoError;
            if (TrySetStopReason(stop_reason))
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
            return;
        }
        std::int32_t retry_delay = connect_retry_ms_;

        // kConnectRetryT is a positive compile-time constant, so it can never be 0 or -1; this guarantees the
        // division below can never hit the undefined-behavior case of dividend == INT64_MIN with divisor == -1,
        // nor divide by zero.
        static_assert(kConnectRetryT > 0,
                      "kConnectRetryT must be a positive divisor to avoid overflow/underflow in division.");
        const auto retry_increase_ms =
            (static_cast<std::int64_t>(connect_retry_ms_) + static_cast<std::int64_t>(kConnectRetryT) - 1) /
            static_cast<std::int64_t>(kConnectRetryT);
        if (retry_increase_ms <= kConnectRetryMsMax)
        {
            // retry_increase_ms is now known to be <= kConnectRetryMsMax, so this subtraction (evaluated in
            // std::int64_t) cannot underflow.
            const auto max_allowed_current_retry_ms = kConnectRetryMsMax - retry_increase_ms;
            if (connect_retry_ms_ <= max_allowed_current_retry_ms)
            {
                // At this point checks guarantee connect_retry_ms_ + retry_increase_ms <= kConnectRetryMsMax,
                // which fits well within std::int32_t; compute the sum in std::int64_t and assign once instead
                // of using operator+= on connect_retry_ms_, so the addition and the narrowing cast back to
                // std::int32_t are both provably free of overflow/data loss.
                // coverity[autosar_cpp14_a4_7_1_violation]
                connect_retry_ms_ =
                    static_cast<std::int32_t>(static_cast<std::int64_t>(connect_retry_ms_) + retry_increase_ms);
            }
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
    lifetime_.InstallDescriptor(fd_expected.value());
    posix_endpoint_.owner = this;
    posix_endpoint_.fd = fd_expected.value();
    posix_endpoint_.max_receive_size = max_receive_size_;
    posix_endpoint_.input = [this]() noexcept {
        StopReason stop_reason = ProcessInputEvent();
        if ((stop_reason != StopReason::kNone) && TrySetStopReason(stop_reason))
        {
            ProcessStateChange(State::kStopping);
            engine_->UnregisterPosixEndpoint(posix_endpoint_);
        }
    };
    posix_endpoint_.ping = [this]() noexcept {
        if (!notify_callback_.empty())
        {
            notify_callback_({});
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
    auto message_expected = engine_->ReceiveProtocolMessage(posix_endpoint_.fd, code);
    if (!message_expected.has_value())
    {
        auto os_code = message_expected.error().GetOsDependentErrorCode();
        if (os_code == EAGAIN)
        {
            // cam happen due to notificaion intended to older connection, but shall be rare
            LogWarn(engine_->GetLogger(), "ProcessInputEvent ", identifier_, " spurious wake-up");
            return StopReason::kNone;
        }
        return (os_code == EPIPE) ? StopReason::kClosedByPeer : StopReason::kIoError;
    }
    auto message = message_expected.value();
    // This switch statement is considered not well-formed due to early exits, i.e. return statements.
    // New Misra rule 9.4.2 allows terminating switch statements with a return statement
    // coverity[autosar_cpp14_m6_4_3_violation]
    // Deviation of MISRA RULE-7-0-5: codeql::misra_deviation_next_line(switch-enum-underlying-type-discriminant)
    switch (code)
    {
        // Deviation of MISRA RULE-7-0-5: codeql::misra_deviation_next_line(switch-enum-underlying-type-discriminant)
        case score::cpp::to_underlying(ServerToClient::REPLY):
        {
            if (!sends_.HandleReply(message))
            {
                return StopReason::kIoError;
            }
            break;
        }
        // Deviation of MISRA RULE-7-0-5: codeql::misra_deviation_next_line(switch-enum-underlying-type-discriminant)
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

void ClientConnection::ScheduleSendDrain() noexcept
{
    engine_->EnqueueCommand(
        async_send_command_,
        ISharedResourceEngine::TimePoint{},
        [this](auto) noexcept {
            sends_.Drain();
        },
        this);
}

void ClientConnection::SwitchToStopState() noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_DBG(state_ == State::kStopping);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_DBG(stop_reason_ != StopReason::kNone);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_DBG(IsInCallback());
    if (lifetime_.RetirementPending())
    {
        FinishStopState();
        return;
    }
    posix_endpoint_.disconnect = {};  // no need to trigger the cleanup once more
    engine_->CleanUpOwner(this);
    sends_.CancelPending();
    if (lifetime_.CancellationFinished())
    {
        FinishStopState();
    }
}

void ClientConnection::ScheduleStopFinalization() noexcept
{
    engine_->EnqueueCommand(
        disconnection_command_,
        ISharedResourceEngine::TimePoint{},
        [this](auto) noexcept {
            SwitchToStopState();
        },
        this);
}

void ClientConnection::FinishStopState() noexcept
{
    const auto fd = lifetime_.RetireDescriptor();
    if (fd != -1)
    {
        engine_->CloseClientConnection(fd);
    }
    ProcessStateChange(State::kStopped);
}

bool ClientConnection::TrySetStopReason(const StopReason stop_reason) noexcept
{
    LogInfo(engine_->GetLogger(), "TrySetStopReason ", score::cpp::to_underlying(stop_reason));

    return lifetime_.TrySetStopReason(stop_reason);
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
    LogInfo(engine_->GetLogger(), "ProcessStateChange ", score::cpp::to_underlying(state));

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

}  // namespace score::message_passing::detail
