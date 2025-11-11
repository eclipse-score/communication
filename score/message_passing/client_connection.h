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

#include "score/message_passing/i_client_connection.h"
#include "score/message_passing/i_client_factory.h"
#include "score/message_passing/i_shared_resource_engine.h"

#include <score/string.hpp>
#include <score/vector.hpp>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>

namespace score
{
namespace message_passing
{
namespace detail
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
    bool TryQueueMessage(score::cpp::span<const std::uint8_t> message, ReplyCallback callback) noexcept;
    StopReason ProcessInputEvent() noexcept;

    // The lock shall be already taken.
    // The function may release it, call a user callback, and then lock it again.
    void ProcessSendQueueUnderLock(std::unique_lock<std::mutex>& lock) noexcept;
    void ArmSendQueueUnderLock() noexcept;

    bool TrySetStopReason(const StopReason stop_reason) noexcept;

    void ProcessStateChangeToStopped() noexcept;
    void ProcessStateChange(const State state) noexcept;
    void SwitchToStopState() noexcept;

    bool IsInCallback() const noexcept
    {
        return engine_->IsOnCallbackThread();
    }

    void DoRestart() noexcept;

    const std::shared_ptr<ISharedResourceEngine> engine_;
    const score::cpp::pmr::string identifier_;
    const std::uint32_t max_send_size_;
    const std::uint32_t max_receive_size_;
    const IClientFactory::ClientConfig client_config_;

    std::int32_t client_fd_;
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

    std::mutex send_mutex_;
    std::condition_variable send_condition_;

    // at the time of construction of the connection object, we preallocate the storage for the amount of messages
    // requested in the client_config to be sent asynchronously. The send_storage_ container is responsible for managing
    // the lifetime of these message objects. In addition, we arrange a list of the currently unused message objects
    // in send_pool_, relying on the allocation-free nature of intrusive lists. When we have a message to send
    // asynchronously, we borrow an element from the send_pool and put it into the send_queue_, which is also an
    // intrusive list container. When we send this message later, we return the freed element back to the send_pool_.
    // Thus, we have no extra memory allocation after creation of a ClientConnection object.
    class SendCommand : public score::containers::intrusive_list_element<>
    {
      public:
        using allocator_type = score::cpp::pmr::polymorphic_allocator<SendCommand>;
        explicit SendCommand(const allocator_type& allocator)
            : score::containers::intrusive_list_element<>{}, message(allocator), callback{}
        {
        }

        score::cpp::pmr::vector<std::uint8_t> message;
        ReplyCallback callback;
    };
    score::cpp::pmr::vector<SendCommand> send_storage_;
    score::containers::intrusive_list<SendCommand> send_pool_;
    score::containers::intrusive_list<SendCommand> send_queue_;

    std::optional<ReplyCallback> waiting_for_reply_;

    ISharedResourceEngine::CommandQueueEntry connection_timer_;
    ISharedResourceEngine::CommandQueueEntry disconnection_command_;
    ISharedResourceEngine::CommandQueueEntry async_send_command_;
    ISharedResourceEngine::PosixEndpointEntry posix_endpoint_;
};

}  // namespace detail
}  // namespace message_passing
}  // namespace score

#endif  // SCORE_LIB_MESSAGE_PASSING_CLIENT_CONNECTION_H
