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

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <thread>
#include <tuple>

namespace score::mw::com::gateway
{

namespace
{

using score::os::Socket;

std::unique_ptr<TransportMessage> CreateMessageForType(MessageType type)
{
    switch (type)
    {
        case MessageType::kProvideServiceRequest:
            return std::make_unique<ProvideServiceRequest>();
        case MessageType::kOfferServiceRequest:
            return std::make_unique<OfferServiceRequest>();
        case MessageType::kStopOfferServiceRequest:
            return std::make_unique<StopOfferServiceRequest>();
        case MessageType::kRegisterNotificationRequest:
            return std::make_unique<RegisterNotificationRequest>();
        case MessageType::kUnregisterNotificationRequest:
            return std::make_unique<UnregisterNotificationRequest>();
        case MessageType::kUpdateNotification:
            return std::make_unique<UpdateNotification>();
        case MessageType::kAckResponse:
            return std::make_unique<AckResponse>();
        case MessageType::kInvalid:
        default:
            return nullptr;
    }
}

void SetTcpNoDelay(std::int32_t socket_fd)
{
    std::int32_t flag = 1;
    const auto socket_result = Socket::instance().setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    if (!socket_result.has_value())
    {
        ::score::mw::log::LogError() << "Failed to set TCP_NODELAY: " << socket_result.error().ToString();
        return;
    }
}

int SendAll(std::int32_t socket_fd, const void* data, std::size_t length)
{
    auto* ptr = static_cast<const char*>(data);
    while (length > 0)
    {
        auto result = Socket::instance().sendto(socket_fd, ptr, length, Socket::MessageFlag::kNone, nullptr, 0);
        if (!result.has_value())
        {
            return -1;
        }
        const auto sent = static_cast<std::size_t>(result.value());
        ptr += sent;
        length -= sent;
    }
    return 0;
}

ssize_t ReceiveAll(std::int32_t socket_fd, void* buffer, std::size_t length)
{
    auto* ptr = static_cast<char*>(buffer);
    const auto original_length = length;
    const score::os::Socket::MessageFlag flags{};
    while (length > 0)
    {
        auto result = Socket::instance().recv(socket_fd, ptr, length, flags);
        if (!result.has_value())
        {
            ::score::mw::log::LogError() << "ReceiveAll: recv error: " << result.error().ToString()
                                         << " fd=" << socket_fd << " remaining=" << length;
            return -1;
        }
        if (result.value() == 0)
        {
            ::score::mw::log::LogWarn() << "ReceiveAll: peer closed connection, fd=" << socket_fd
                                        << " remaining=" << length << " of " << original_length;
            return 0;
        }
        const auto received = static_cast<std::size_t>(result.value());
        ptr += received;
        length -= received;
    }
    return static_cast<ssize_t>(original_length);
}

}  // namespace

BidirectionalTransport::BidirectionalTransport(HyperVisorSocketConfiguration socket_config) noexcept
    : socket_config_(std::move(socket_config))
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
        // unusable state. This is especially important if the remote side is not up yet, since in that case we expect
        // multiple retries and want to avoid issues with reusing a socket that has been put into an error state by the
        // failed connect attempt.
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
        // This is used to handle the case where the connection is lost and we need to re-establish it. In that case we
        // need to set up a new listen socket since the previous one would have been used to accept the connection that
        // got lost and is now in an unusable state. By setting up a new listen socket we can ensure that we are able to
        // accept a new connection when the remote side comes back up.
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
        {
            fcntl(client_sock.Get(), F_SETFL, flags & ~O_NONBLOCK);
        }

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
        auto message = ReceiveMessageFromSocket(receive_socket_);
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

    // Post to dispatch queue rather than calling the handler inline. This keeps the
    // receive loop free to process incoming ACKs while the handler (which may itself
    // call SendRequest) runs on the dedicated dispatch thread.
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
    pending_cv_.notify_all();
}

void BidirectionalTransport::DispatchLoop(score::cpp::stop_token /*stop_token*/)
{
    while (true)
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

        message_handler_(std::move(message));
    }
}

void BidirectionalTransport::HandleResponse(std::unique_ptr<TransportMessage> response)
{
    if (response->GetType() != MessageType::kAckResponse)
    {
        // Just a sanity check, we should only receive AckResponses as responses since that's the only response type we
        // have.
        return;
    }

    const auto* ack = static_cast<AckResponse*>(response.get());

    std::lock_guard<std::mutex> lock(pending_mutex_);
    auto it = pending_requests_.find(ack->GetAckedSequence());
    if (it != pending_requests_.end())
    {
        it->second.acknowledged = true;
        pending_cv_.notify_all();
    }
}

score::ResultBlank BidirectionalTransport::SendAck(const std::uint32_t sequence)
{
    AckResponse ack_response(sequence);
    std::lock_guard<std::mutex> lock(send_mutex_);
    return SendMessageOnSocket(send_socket_, ack_response);
}

std::uint32_t BidirectionalTransport::GetNextSequenceNumber()
{
    return next_sequence_.fetch_add(1U, std::memory_order_relaxed);
}

score::ResultBlank BidirectionalTransport::TrySendAndWaitForAck(TransportMessage& message, const std::uint32_t sequence)
{
    {
        std::lock_guard<std::mutex> lock(send_mutex_);
        auto send_result = SendMessageOnSocket(send_socket_, message);
        if (!send_result.has_value())
        {
            score::mw::log::LogError() << "BidirectionalTransport: failed to send message of type "
                                       << static_cast<int>(message.GetType());
            return send_result;
        }
    }

    std::unique_lock<std::mutex> lock(pending_mutex_);
    auto it = pending_requests_.find(sequence);
    if (it == pending_requests_.end())
    {
        score::mw::log::LogError() << "BidirectionalTransport: internal error: pending request not found for sequence "
                                   << sequence;
        return score::MakeUnexpected(TransportErrorc::kSendFailure);
    }

    pending_cv_.wait_for(lock, std::chrono::milliseconds(socket_config_.request_timeout_ms_), [&it, this] {
        return it->second.acknowledged || !is_connected_.load();
    });

    if (it->second.acknowledged)
    {
        score::mw::log::LogDebug() << "BidirectionalTransport: received ack for sequence " << sequence;
        return {};
    }

    if (!is_connected_.load())
    {
        ::score::mw::log::LogError() << "BidirectionalTransport: connection lost while waiting for ack for sequence "
                                     << sequence;
        return score::MakeUnexpected(TransportErrorc::kNotConnected);
    }

    ::score::mw::log::LogError() << "BidirectionalTransport: timeout waiting for ack for sequence " << sequence;
    return score::MakeUnexpected(TransportErrorc::kTimeout,
                                 "timeout waiting for ack for sequence " + std::to_string(sequence));
}

score::ResultBlank BidirectionalTransport::SendRequest(TransportMessage& message)
{
    if (!is_connected_)
    {
        return score::MakeUnexpected(TransportErrorc::kNotConnected, "BidirectionalTransport: not connected");
    }

    const std::uint32_t sequence = GetNextSequenceNumber();
    message.SetSequenceNumber(sequence);

    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        pending_requests_[sequence] = PendingRequest{};
    }

    score::ResultBlank last_result = score::MakeUnexpected(TransportErrorc::kSendFailure);
    for (std::size_t attempt = 0U; attempt < kMaxRetries; ++attempt)
    {
        if (!is_connected_)
        {
            last_result = score::MakeUnexpected(TransportErrorc::kNotConnected);
            break;
        }

        if (attempt > 0U)
        {
            ::score::mw::log::LogWarn() << "BidirectionalTransport: request timeout or failure, retrying (" << attempt
                                        << "/" << kMaxRetries << ")";
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_requests_[sequence].acknowledged = false;
        }

        last_result = TrySendAndWaitForAck(message, sequence);
        if (last_result.has_value())
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            // erase the pending request since we got the ack successfully.
            pending_requests_.erase(sequence);
            return {};
        }

        // Don't retry on disconnect — the remote has no knowledge of this pending request
        if (!is_connected_)
        {
            break;
        }
    }

    std::lock_guard<std::mutex> lock(pending_mutex_);
    pending_requests_.erase(sequence);  // erase the pending request since we are giving up after max retries.
    return last_result;
}

score::ResultBlank BidirectionalTransport::SendNotification(TransportMessage& message)
{
    if (!is_connected_)
    {
        return score::MakeUnexpected(TransportErrorc::kNotConnected);
    }

    std::lock_guard<std::mutex> lock(send_mutex_);
    return SendMessageOnSocket(send_socket_, message);
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

    pending_cv_.notify_all();

    send_socket_.ShutdownFd();
    receive_socket_.ShutdownFd();
    listen_socket_.ShutdownFd();

    dispatch_shutdown_ = true;
    dispatch_cv_.notify_all();

    threads_.Shutdown();
}

score::ResultBlank BidirectionalTransport::SendMessageOnSocket(const UniqueSocket& socket,
                                                               const TransportMessage& message)
{
    const auto payload_size = message.Serialize(send_buffer_);
    if (payload_size == 0U)
    {
        return score::MakeUnexpected(TransportErrorc::kSendFailure);
    }

    MessageHeader header = message.GetHeader();
    header.payload_size = static_cast<std::uint32_t>(payload_size);

    std::uint8_t header_buf[MessageHeader::kWireSize]{};
    header.SerializeToBuffer(header_buf);

    if (SendAll(socket.Get(), header_buf, MessageHeader::kWireSize) != 0)
    {
        return score::MakeUnexpected(TransportErrorc::kSendFailure);
    }

    if (payload_size > 0U && SendAll(socket.Get(), send_buffer_.data(), payload_size) != 0)
    {
        return score::MakeUnexpected(TransportErrorc::kSendFailure);
    }

    return {};
}

std::unique_ptr<TransportMessage> BidirectionalTransport::ReceiveMessageFromSocket(const UniqueSocket& socket)
{
    std::uint8_t header_buf[MessageHeader::kWireSize]{};
    if (ReceiveAll(socket.Get(), header_buf, MessageHeader::kWireSize) <= 0)
    {
        return nullptr;
    }

    MessageHeader header{};
    header.DeserializeFromBuffer(header_buf);

    if (header.payload_size > kMaxPayloadSize)
    {
        ::score::mw::log::LogError() << "BidirectionalTransport: payload size " << header.payload_size
                                     << " exceeds maximum " << kMaxPayloadSize;
        return nullptr;
    }

    if (header.payload_size > 0U)
    {
        const auto bytes_received = ReceiveAll(socket.Get(), receive_buffer_.data(), header.payload_size);
        if (bytes_received <= 0)
        {
            return nullptr;
        }
    }

    auto message = CreateMessageForType(header.type);
    if (message == nullptr)
    {
        ::score::mw::log::LogError() << "BidirectionalTransport: unknown message type "
                                     << static_cast<int>(header.type);
        return nullptr;
    }

    message->SetSequenceNumber(header.sequence);

    if (header.payload_size > 0U &&
        !message->Deserialize(score::cpp::span<const std::uint8_t>(receive_buffer_.data(), header.payload_size)))
    {
        ::score::mw::log::LogError() << "BidirectionalTransport: deserialization failed";
        return nullptr;
    }

    return message;
}

}  // namespace score::mw::com::gateway
