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
#include "score/message_passing/unix_domain/unix_domain_server.h"

#include "score/message_passing/client_server_communication.h"
#include "score/message_passing/unix_domain/unix_domain_engine.h"
#include "score/message_passing/unix_domain/unix_domain_socket_address.h"

#include <score/utility.hpp>

namespace score
{
namespace message_passing
{
namespace detail
{

namespace
{

// The socket listening backlog is quite an arbitrary value: if a client gets ECONNREFUSED because it didn't fit
// into the connection backlog queue, it will try to reconnect after some delay again.
constexpr std::int32_t kSocketListenBacklog = 20;

}  // namespace

UnixDomainServer::ServerConnection::ServerConnection(UnixDomainServer& server,
                                                     std::int32_t fd,
                                                     ClientIdentity client_identity) noexcept
    : server_{server}, user_data_{}, client_identity_{client_identity}, fd_{fd}
{
}

void UnixDomainServer::ServerConnection::AcceptConnection(UserData&& data,
                                                          score::cpp::pmr::unique_ptr<ServerConnection>&& self) noexcept
{
    user_data_ = std::move(data);
    endpoint_.owner = &server_;
    endpoint_.fd = fd_;
    endpoint_.max_receive_size = server_.max_request_size_;
    endpoint_.input = [this]() noexcept {
        if (!ProcessInput())
        {
            server_.engine_->UnregisterPosixEndpoint(endpoint_);
        }
    };
    endpoint_.output = {};
    endpoint_.disconnect = [this]() noexcept {
        self_.reset();
    };

    server_.engine_->RegisterPosixEndpoint(endpoint_);
    self_ = std::move(self);
}

const ClientIdentity& UnixDomainServer::ServerConnection::GetClientIdentity() const noexcept
{
    return client_identity_;
}

UserData& UnixDomainServer::ServerConnection::GetUserData() noexcept
{
    return *user_data_;
}

score::cpp::expected_blank<score::os::Error> UnixDomainServer::ServerConnection::Reply(
    score::cpp::span<const std::uint8_t> message) noexcept
{
    if (message.size() > server_.max_reply_size_)
    {
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOMEM));
    }
    return server_.engine_->SendProtocolMessage(
        endpoint_.fd, score::cpp::to_underlying(ServerToClient::REPLY), message);
}

score::cpp::expected_blank<score::os::Error> UnixDomainServer::ServerConnection::Notify(
    score::cpp::span<const std::uint8_t> message) noexcept
{
    if (message.size() > server_.max_notify_size_)
    {
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOMEM));
    }
    return server_.engine_->SendProtocolMessage(
        endpoint_.fd, score::cpp::to_underlying(ServerToClient::NOTIFY), message);
}

void UnixDomainServer::ServerConnection::RequestDisconnect() noexcept
{
    std::lock_guard<std::recursive_mutex> guard(server_.connection_setup_mutex_);
    server_.engine_->UnregisterPosixEndpoint(endpoint_);
}

bool UnixDomainServer::ServerConnection::ProcessInput() noexcept
{
    std::uint8_t code;
    auto& user_data = *user_data_;
    using HandlerPointerT = score::cpp::pmr::unique_ptr<IConnectionHandler>;
    auto message_expected = server_.engine_->ReceiveProtocolMessage(endpoint_.fd, code);
    if (!message_expected.has_value())
    {
        return false;
    }
    auto message = message_expected.value();
    switch (code)
    {
        case score::cpp::to_underlying(ClientToServer::REQUEST):
            return (std::holds_alternative<HandlerPointerT>(user_data)
                        ? std::get<HandlerPointerT>(user_data)->OnMessageSentWithReply(*this, message)
                        : server_.sent_with_reply_callback_(*this, message))
                .has_value();

        case score::cpp::to_underlying(ClientToServer::SEND):
            return (std::holds_alternative<HandlerPointerT>(user_data)
                        ? std::get<HandlerPointerT>(user_data)->OnMessageSent(*this, message)
                        : server_.sent_callback_(*this, message))
                .has_value();

        default:
            // unrecognised message; drop connection
            return false;
    }
    return true;
}

UnixDomainServer::ServerConnection::~ServerConnection() noexcept
{
    if (user_data_.has_value())
    {
        auto& user_data = *user_data_;
        using HandlerPointerT = score::cpp::pmr::unique_ptr<IConnectionHandler>;
        if (std::holds_alternative<HandlerPointerT>(user_data))
        {
            std::get<HandlerPointerT>(user_data)->OnDisconnect(*this);
        }
        else
        {
            server_.disconnect_callback_(*this);
        }
    }
    server_.engine_->GetOsResources().unistd->close(fd_);
}

UnixDomainServer::UnixDomainServer(std::shared_ptr<UnixDomainEngine> engine,
                                   const ServiceProtocolConfig& protocol_config,
                                   const IServerFactory::ServerConfig& /*server_config*/) noexcept
    : engine_{std::move(engine)},
      identifier_{protocol_config.identifier.data(), protocol_config.identifier.size(), engine_->GetMemoryResource()},
      max_request_size_{protocol_config.max_send_size},
      max_reply_size_{protocol_config.max_reply_size},
      max_notify_size_{protocol_config.max_notify_size},
      server_fd_{-1}
{
}

UnixDomainServer::~UnixDomainServer() noexcept
{
    StopListening();
}

score::cpp::expected_blank<score::os::Error> UnixDomainServer::StartListening(
    ConnectCallback connect_callback,
    DisconnectCallback disconnect_callback,
    MessageCallback sent_callback,
    MessageCallback sent_with_reply_callback) noexcept
{
    connect_callback_ = std::move(connect_callback);
    disconnect_callback_ = std::move(disconnect_callback);
    sent_callback_ = std::move(sent_callback);
    sent_with_reply_callback_ = std::move(sent_with_reply_callback);

    UnixDomainSocketAddress addr{identifier_, true};
    auto& socket = engine_->GetOsResources().socket;

    auto fd_expected = socket->socket(score::os::Socket::Domain::kUnix, SOCK_STREAM, 0);
    if (!fd_expected.has_value())
    {
        return score::cpp::make_unexpected(fd_expected.error());
    }
    server_fd_ = fd_expected.value();

    auto bind_expected = socket->bind(server_fd_, addr.data(), addr.size());
    if (!bind_expected.has_value())
    {
        engine_->GetOsResources().unistd->close(server_fd_);
        server_fd_ = -1;
        return score::cpp::make_unexpected(bind_expected.error());
    }

    auto listen_expected = socket->listen(server_fd_, kSocketListenBacklog);
    if (!listen_expected.has_value())
    {
        engine_->GetOsResources().unistd->close(server_fd_);
        server_fd_ = -1;
        return score::cpp::make_unexpected(listen_expected.error());
    }

    listener_endpoint_.owner = this;
    listener_endpoint_.fd = server_fd_;
    listener_endpoint_.max_receive_size = 0;
    listener_endpoint_.input = [this]() noexcept {
        ProcessConnect();
    };
    listener_endpoint_.output = {};
    listener_endpoint_.disconnect = {};

    engine_->EnqueueCommand(
        listener_command_,
        ISharedResourceEngine::TimePoint{},
        [this](auto) noexcept {
            engine_->RegisterPosixEndpoint(listener_endpoint_);
        },
        this);

    return {};
}

void UnixDomainServer::StopListening() noexcept
{
    if (server_fd_ >= 0)
    {
        engine_->CleanUpOwner(this);
        engine_->GetOsResources().unistd->close(server_fd_);
        server_fd_ = -1;
    }
}

void UnixDomainServer::ProcessConnect() noexcept
{
    auto& socket = engine_->GetOsResources().socket;

    const std::int32_t data_fd = socket->accept(server_fd_, nullptr, nullptr).value();
#ifdef __QNX__
    // no support for SO_PEERCRED on QNX
    ClientIdentity identity{0, 0, 0};
#else
    ucred cr;
    socklen_t cr_len = sizeof(cr);
    auto sockopt_expected = socket->getsockopt(data_fd, SOL_SOCKET, SO_PEERCRED, &cr, &cr_len);
    if (!sockopt_expected.has_value() || cr_len != sizeof(cr))
    {
        engine_->GetOsResources().unistd->close(data_fd);
        return;
    }

    ClientIdentity identity{cr.pid, cr.uid, cr.gid};
#endif
    auto connection =
        score::cpp::pmr::make_unique<ServerConnection>(engine_->GetMemoryResource(), *this, data_fd, identity);

    std::lock_guard<std::recursive_mutex> guard(connection_setup_mutex_);
    // It should be possible to request disconnect for other connections from this callback;
    // however, after this callback ends, it should not be possible to disconnect the new connection
    // until it is at least placed in the UnixDomainEngine's insertion queue.
    // UnixDomainServer::ProcessConnect() runs exclusively on the UnixDomainEngine's internal thread,
    // so a recursive mutex shall do the job.
    auto data_expected = connect_callback_(*connection);
    if (!data_expected.has_value())
    {
        return;
    }
    connection->AcceptConnection(std::move(data_expected.value()), std::move(connection));
}

}  // namespace detail
}  // namespace message_passing
}  // namespace score
