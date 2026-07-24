/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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
#include "score/mw/com/gateway/transport_layer/sample/bidirectional_transport.h"

#include "score/concurrency/simple_task.h"
#include "score/mw/log/logging.h"
#include "score/os/socket.h"
#include <score/span.hpp>

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <memory>
#include <thread>
#include <tuple>

namespace score::mw::com::gateway
{

namespace
{

using score::os::Socket;

/// \brief Sets the TCP_NODELAY option on the given socket file descriptor to disable Nagle's algorithm in order to
/// reduce latency.
void SetTcpNoDelay(std::int32_t socket_fd)
{
    std::int32_t flag = 1;
    const auto socket_result = Socket::instance().setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    if (!socket_result.has_value())
    {
        ::score::mw::log::LogError() << "Failed to set TCP_NODELAY for socket " << socket_fd << ": "
                                     << socket_result.error().ToString();
        return;
    }
}

}  // namespace

BidirectionalTransport::BidirectionalTransport(HyperVisorSocketConfiguration socket_config) noexcept
    : socket_config_(std::move(socket_config)),
      message_framer_(std::make_unique<MessageFramer>()),
      pending_tracker_(std::make_unique<PendingRequestTracker>())
{
}

BidirectionalTransport::BidirectionalTransport(HyperVisorSocketConfiguration socket_config,
                                               std::unique_ptr<IMessageFramer> message_framer,
                                               std::unique_ptr<IPendingRequestTracker> pending_tracker) noexcept
    : socket_config_(std::move(socket_config)),
      message_framer_(std::move(message_framer)),
      pending_tracker_(std::move(pending_tracker))
{
}

BidirectionalTransport::~BidirectionalTransport()
{
    Shutdown();
}

score::ResultBlank BidirectionalTransport::Setup()
{
    auto listen_result = SetupListenSocket();
    if (!listen_result.has_value())
    {
        score::mw::log::LogError() << "BidirectionalTransport: failed to set up listen socket";
        return listen_result;
    }
    threads_.Enqueue(score::concurrency::SimpleTaskFactory::Make(score::cpp::pmr::get_default_resource(),
                                                                 [this](const score::cpp::stop_token stop_token) {
                                                                     this->ConnectionLoop(stop_token);
                                                                 }));
    threads_.Enqueue(score::concurrency::SimpleTaskFactory::Make(score::cpp::pmr::get_default_resource(),
                                                                 [this](const score::cpp::stop_token stop_token) {
                                                                     this->DispatchLoop(stop_token);
                                                                 }));

    // we block the connection loop until the first connection is established to ensure that Setup() only returns once
    // the transport is actually ready to send and receive messages
    while (!is_connected_ && !threads_.ShutdownRequested())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (!is_connected_)
    {
        Shutdown();
        return score::MakeUnexpected(TransportErrorc::kConnectionFailure);
    }

    return {};
}

score::ResultBlank BidirectionalTransport::SetupSendSocket(score::cpp::stop_token stop_token)
{
    auto socket_result = Socket::instance().socket(Socket::Domain::kIPv4, SOCK_STREAM, 0);
    if (!socket_result.has_value())
    {
        return score::MakeUnexpected(TransportErrorc::kConnectionFailure);
    }
    UniqueSocket socket(socket_result.value());

    SetTcpNoDelay(socket.Get());

    // Setup remote address structure from configuration
    sockaddr_in remote_addr{};
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(socket_config_.remote_port_);
    const auto ip_bytes = socket_config_.remote_ip_.ToIpv4Bytes();
    std::memcpy(&remote_addr.sin_addr, ip_bytes.data(), sizeof(remote_addr.sin_addr));
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) required by POSIX sockaddr API
    const auto* remote_addr_ptr = reinterpret_cast<const struct sockaddr*>(&remote_addr);

    constexpr auto kRetryInterval = std::chrono::milliseconds(50);

    while (!stop_token.stop_requested())
    {
        auto connect_result = Socket::instance().connect(socket.Get(), remote_addr_ptr, sizeof(remote_addr));
        if (connect_result.has_value())
        {
            send_socket_ = std::move(socket);
            return {};
        }
        // retry indefinitely until stop is requested for any errors, could potentially hang here if the remote side is
        // never available
        score::mw::log::LogDebug() << "BidirectionalTransport: connect failed, retrying in " << kRetryInterval.count()
                                   << " ms: " << connect_result.error().ToString();
        std::this_thread::sleep_for(kRetryInterval);

        // Creating a new socket for each retry since the previous connect attempt might have put the socket into an
        // unusable state.
        auto new_sock_result = Socket::instance().socket(Socket::Domain::kIPv4, SOCK_STREAM, 0);
        if (!new_sock_result.has_value())
        {
            ::score::mw::log::LogError() << "BidirectionalTransport: failed to recreate socket for retry";
            return score::MakeUnexpected(TransportErrorc::kConnectionFailure);
        }
        socket.Reset(new_sock_result.value());
        SetTcpNoDelay(socket.Get());
    }

    ::score::mw::log::LogError() << "BidirectionalTransport: connect aborted";
    return score::MakeUnexpected(TransportErrorc::kConnectionFailure);
}

score::ResultBlank BidirectionalTransport::SetupListenSocket()
{
    auto socket_result = Socket::instance().socket(Socket::Domain::kIPv4, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (!socket_result.has_value())
    {
        return score::MakeUnexpected(TransportErrorc::kConnectionFailure);
    }

    UniqueSocket socket(socket_result.value());

    constexpr std::int32_t opt = 1;
    std::ignore = Socket::instance().setsockopt(socket.Get(), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in local_addr{};
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(socket_config_.local_port_);

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) required by POSIX sockaddr API
    const auto* local_addr_ptr = reinterpret_cast<const struct sockaddr*>(&local_addr);

    auto bind_result = Socket::instance().bind(socket.Get(), local_addr_ptr, sizeof(local_addr));
    if (!bind_result.has_value())
    {
        ::score::mw::log::LogError() << "BidirectionalTransport: bind failed on port " << socket_config_.local_port_;
        return score::MakeUnexpected(TransportErrorc::kConnectionFailure);
    }
    constexpr std::int32_t backlog = 1;
    auto listen_result = Socket::instance().listen(socket.Get(), backlog);
    if (!listen_result.has_value())
    {
        return score::MakeUnexpected(TransportErrorc::kConnectionFailure);
    }
    listen_socket_ = std::move(socket);
    return {};
}

void BidirectionalTransport::ConnectionLoop(score::cpp::stop_token stop_token)
{
    while (!stop_token.stop_requested())
    {
        if (!listen_socket_.IsValid())
        {
            auto result = SetupListenSocket();
            if (!result.has_value())
            {
                ::score::mw::log::LogError() << "BidirectionalTransport: failed to re-establish listen socket";
                return;
            }
        }

        auto send_task_result = threads_.Submit([this](score::cpp::stop_token st) {
            return SetupSendSocket(st);
        });

        const bool receive_connected = WaitForConnection(stop_token);

        const auto send_result = send_task_result.Get();

        if (!receive_connected || !send_result.has_value())
        {
            CleanupSocketsForReconnection();
            break;  // Shutdown has been requested or setup failed.
        }

        is_connected_ = true;
        ReceiveUntilDisconnect(stop_token);

        CleanupSocketsForReconnection();
        if (!stop_token.stop_requested())
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

bool BidirectionalTransport::WaitForConnection(score::cpp::stop_token stop_token)
{
    struct sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);

    while (!stop_token.stop_requested())
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) required by POSIX sockaddr API
        auto accept_result = Socket::instance().accept(
            listen_socket_.Get(), reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);

        if (!accept_result.has_value())
        {
            // EAGAIN and EWOULDBLOCK are expected since the listen socket is non-blocking, so we just continue waiting
            // in that case. For other errors we still continue trying to accept new connections since
            // transient errors can occur and we want to be resilient against them.
            if (accept_result.error() == score::os::Error::createFromErrno(EAGAIN) ||
                accept_result.error() == score::os::Error::createFromErrno(EWOULDBLOCK))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            if (!stop_token.stop_requested())
            {
                ::score::mw::log::LogError() << "BidirectionalTransport: accept failed";
            }
            continue;
        }

        UniqueSocket client_sock(accept_result.value());
        SetTcpNoDelay(client_sock.Get());

        // On QNX, accepted sockets inherit SOCK_NONBLOCK from the listen socket.
        // Clear it so recv() blocks properly instead of returning EAGAIN immediately.
        int flags = fcntl(client_sock.Get(), F_GETFL, 0);
        if (flags != -1)
        // COV_JUSTIFIED_START gateway-clear-nonblock-on-accepted-socket
        {
            fcntl(client_sock.Get(), F_SETFL, flags & ~O_NONBLOCK);
        }
        // COV_JUSTIFIED_STOP
        receive_socket_ = std::move(client_sock);
        listen_socket_.Reset();
        return true;
    }
    return false;
}

void BidirectionalTransport::ReceiveUntilDisconnect(score::cpp::stop_token stop_token)
{
    while (!stop_token.stop_requested() && is_connected_)
    {
        auto message = message_framer_->ReceiveMessage(receive_socket_.Get());
        if (message == nullptr)
        {
            if (!stop_token.stop_requested())
            {
                is_connected_ = false;
                ::score::mw::log::LogWarn() << "BidirectionalTransport: disconnected, will attempt to reconnect";
            }
            return;
        }
        HandleIncomingMessage(std::move(message));
    }
}

void BidirectionalTransport::HandleIncomingMessage(std::unique_ptr<TransportMessage> message)
{
    if (IsResponse(message->GetType()))
    {
        HandleResponse(std::move(message));
        return;
    }

    if (!has_message_handler_)
    {
        ::score::mw::log::LogWarn() << "BidirectionalTransport: received message but no handler registered to handle "
                                       "it: messages are dropped. Message type: "
                                    << static_cast<int>(message->GetType());
        return;
    }

    if (RequiresResponse(message->GetType()))
    {
        SendAck(message->GetSequenceNumber());
    }

    // Post to dispatch queue rather than calling the handler inline.
    {
        std::lock_guard<std::mutex> lock(dispatch_mutex_);
        dispatch_queue_.push(std::move(message));
    }
    dispatch_cv_.notify_one();
}

void BidirectionalTransport::CleanupSocketsForReconnection()
{
    is_connected_ = false;
    {
        std::lock_guard<std::mutex> lock(send_mutex_);
        send_socket_.ShutdownAndReset();
    }
    receive_socket_.ShutdownAndReset();
    pending_tracker_->NotifyAll();
}

void BidirectionalTransport::DispatchLoop(const score::cpp::stop_token& stop_token)
{
    while (!stop_token.stop_requested())
    {
        std::unique_ptr<TransportMessage> message;
        {
            std::unique_lock<std::mutex> lock(dispatch_mutex_);
            dispatch_cv_.wait(lock, [this] {
                return !dispatch_queue_.empty() || dispatch_shutdown_.load();
            });

            if (dispatch_shutdown_.load() && dispatch_queue_.empty())
            {
                break;
            }

            message = std::move(dispatch_queue_.front());
            dispatch_queue_.pop();
        }

        message_handler_(std::move(message));  // COV_JUSTIFIED gateway-dispatch-loop-calls-handler
    }
}

void BidirectionalTransport::HandleResponse(std::unique_ptr<TransportMessage> response)
{
    if (response->GetType() != MessageType::kAckResponse)
    // COV_JUSTIFIED_START gateway-handle-response-only-ack-response
    {
        return;
    }
    // COV_JUSTIFIED_STOP

    const auto* ack = static_cast<AckResponse*>(response.get());
    pending_tracker_->Acknowledge(ack->GetAckedSequence());
}

score::ResultBlank BidirectionalTransport::SendAck(const std::uint32_t sequence)
{
    AckResponse ack_response(sequence);
    std::lock_guard<std::mutex> lock(send_mutex_);
    return message_framer_->SendMessage(send_socket_.Get(), ack_response);
}

score::ResultBlank BidirectionalTransport::TrySendAndWaitForAck(TransportMessage& message, const std::uint32_t sequence)
{
    {
        std::lock_guard<std::mutex> lock(send_mutex_);
        auto send_result = message_framer_->SendMessage(send_socket_.Get(), message);
        if (!send_result.has_value())
        {
            score::mw::log::LogError() << "BidirectionalTransport: failed to send message of type "
                                       << static_cast<int>(message.GetType());
            return send_result;
        }
    }

    return pending_tracker_->WaitForAck(
        sequence, std::chrono::milliseconds(socket_config_.request_timeout_ms_), is_connected_);
}

score::ResultBlank BidirectionalTransport::SendRequest(TransportMessage& message)
{
    if (!is_connected_)
    {
        return score::MakeUnexpected(TransportErrorc::kNotConnected, "BidirectionalTransport: not connected");
    }

    const std::uint32_t sequence = pending_tracker_->GetNextSequenceNumber();
    message.SetSequenceNumber(sequence);

    pending_tracker_->RegisterPendingRequest(sequence);

    score::ResultBlank last_result = score::MakeUnexpected(TransportErrorc::kSendFailure);
    for (std::size_t attempt = 0U; attempt < kMaxSendRetries; ++attempt)
    {
        if (!is_connected_)
        {
            last_result = score::MakeUnexpected(TransportErrorc::kNotConnected);
            break;
        }

        if (attempt > 0U)
        {
            ::score::mw::log::LogWarn() << "BidirectionalTransport: request timeout or failure, retrying (" << attempt
                                        << "/" << kMaxSendRetries << ")";
            pending_tracker_->ResetAcknowledgement(sequence);
        }

        last_result = TrySendAndWaitForAck(message, sequence);
        if (last_result.has_value())
        {
            pending_tracker_->ErasePendingRequest(sequence);
            return {};
        }

        // Don't retry on disconnect — the remote has no knowledge of this pending request
        if (!is_connected_)
        {
            break;
        }
    }

    pending_tracker_->ErasePendingRequest(sequence);
    return last_result;
}

score::ResultBlank BidirectionalTransport::SendNotification(TransportMessage& message)
{
    if (!is_connected_)
    {
        return score::MakeUnexpected(TransportErrorc::kNotConnected);
    }

    std::lock_guard<std::mutex> lock(send_mutex_);
    return message_framer_->SendMessage(send_socket_.Get(), message);
}

void BidirectionalTransport::SetMessageHandler(MessageHandler handler)
{
    message_handler_ = std::move(handler);
    has_message_handler_ = true;
}

bool BidirectionalTransport::IsConnected() const
{
    return is_connected_.load();
}

void BidirectionalTransport::Shutdown()
{
    is_connected_ = false;

    pending_tracker_->NotifyAll();

    send_socket_.ShutdownFd();
    receive_socket_.ShutdownFd();
    listen_socket_.ShutdownFd();

    dispatch_shutdown_ = true;
    dispatch_cv_.notify_all();

    threads_.Shutdown();
}

}  // namespace score::mw::com::gateway
