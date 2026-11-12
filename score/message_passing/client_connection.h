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
#ifndef SCORE_LIB_MESSAGE_PASSING_CLIENT_CONNECTION_H
#define SCORE_LIB_MESSAGE_PASSING_CLIENT_CONNECTION_H

#include "score/message_passing/client_connection_lifetime.h"
#include "score/message_passing/client_send_queue.h"
#include "score/message_passing/i_client_connection.h"
#include "score/message_passing/i_client_factory.h"
#include "score/message_passing/i_shared_resource_engine.h"

#include <score/string.hpp>

#include <atomic>
#include <mutex>

namespace score::message_passing::detail
{

class ClientConnection final : public IClientConnection
{
  public:
    ClientConnection(std::shared_ptr<ISharedResourceEngine> engine,
                     const ServiceProtocolConfig& protocol_config,
                     const IClientFactory::ClientConfig& client_config) noexcept;
    ~ClientConnection() noexcept override;

    ClientConnection(const ClientConnection&) = delete;
    ClientConnection(ClientConnection&&) = delete;
    ClientConnection& operator=(const ClientConnection&) = delete;
    ClientConnection& operator=(ClientConnection&&) = delete;

    score::cpp::expected_blank<score::os::Error> Send(score::cpp::span<const std::uint8_t> message) noexcept override;

    score::cpp::expected<score::cpp::span<const std::uint8_t>, score::os::Error> SendWaitReply(
        score::cpp::span<const std::uint8_t> message,
        score::cpp::span<std::uint8_t> reply) noexcept override;

    score::cpp::expected_blank<score::os::Error> SendWithCallback(score::cpp::span<const std::uint8_t> message,
                                                                  ReplyCallback callback) noexcept override;

    State GetState() const noexcept override;

    StopReason GetStopReason() const noexcept override;

    void Start(StateCallback state_callback, NotifyCallback notify_callback) noexcept override;

    void Stop() noexcept override;

    void Restart() noexcept override;

  private:
    void TryConnect() noexcept;
    StopReason ProcessInputEvent() noexcept;
    void ScheduleSendDrain() noexcept;

    bool TrySetStopReason(const StopReason stop_reason) noexcept;

    void ProcessStateChangeToStopped() noexcept;
    void ProcessStateChange(const State state) noexcept;
    void SwitchToStopState() noexcept;
    void ScheduleStopFinalization() noexcept;
    void FinishStopState() noexcept;

    bool IsInCallback() const noexcept
    {
        return engine_->IsOnCallbackThread();
    }

    void DoRestart() noexcept;

    const std::shared_ptr<ISharedResourceEngine> engine_;
    const score::cpp::pmr::string identifier_;
    const std::uint32_t max_receive_size_;
    const IClientFactory::ClientConfig client_config_;

    std::atomic<State> state_;
    std::atomic<StopReason> stop_reason_;

    // to detach and survive the destructor, if needed for stopping
    struct CallbackContext
    {
        StateCallback state_callback;
        std::recursive_mutex finalize_mutex;
    };
    std::shared_ptr<CallbackContext> callback_context_;

    NotifyCallback notify_callback_;

    std::int32_t connect_retry_ms_;

    ClientConnectionLifetime lifetime_;
    ClientSendQueue sends_;

    ISharedResourceEngine::CommandQueueEntry connection_timer_;
    ISharedResourceEngine::CommandQueueEntry disconnection_command_;
    ISharedResourceEngine::CommandQueueEntry async_send_command_;
    ISharedResourceEngine::PosixEndpointEntry posix_endpoint_;
};

}  // namespace score::message_passing::detail

#endif  // SCORE_LIB_MESSAGE_PASSING_CLIENT_CONNECTION_H
