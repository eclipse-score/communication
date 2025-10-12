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
#ifndef SCORE_LIB_MESSAGE_PASSING_UNIX_DOMAIN_UNIX_DOMAIN_SERVER_H
#define SCORE_LIB_MESSAGE_PASSING_UNIX_DOMAIN_UNIX_DOMAIN_SERVER_H

#include "score/message_passing/i_server.h"

#include "score/message_passing/i_server_connection.h"
#include "score/message_passing/unix_domain/unix_domain_engine.h"
#include "score/message_passing/unix_domain/unix_domain_server_factory.h"

#include <score/string.hpp>

#include <optional>

namespace score
{
namespace message_passing
{
namespace detail
{

class UnixDomainServer final : public IServer
{
  public:
    class ServerConnection final : public IServerConnection
    {
      public:
        ServerConnection(UnixDomainServer& server, std::int32_t fd, ClientIdentity client_identity) noexcept;

        // User methods
        const ClientIdentity& GetClientIdentity() const noexcept override;
        UserData& GetUserData() noexcept override;

        score::cpp::expected_blank<score::os::Error> Reply(score::cpp::span<const std::uint8_t> message) noexcept override;
        score::cpp::expected_blank<score::os::Error> Notify(score::cpp::span<const std::uint8_t> message) noexcept override;
        void RequestDisconnect() noexcept override;

        // Server methods
        void AcceptConnection(UserData&& data, score::cpp::pmr::unique_ptr<ServerConnection>&& self) noexcept;
        bool ProcessInput() noexcept;

        ~ServerConnection() noexcept;

      private:
        UnixDomainServer& server_;
        std::optional<UserData> user_data_;
        ClientIdentity client_identity_;
        std::int32_t fd_;
        ISharedResourceEngine::PosixEndpointEntry endpoint_;
        score::cpp::pmr::unique_ptr<ServerConnection> self_;
    };

    UnixDomainServer(std::shared_ptr<UnixDomainEngine> engine,
                     const ServiceProtocolConfig& protocol_config,
                     const IServerFactory::ServerConfig& server_config) noexcept;
    ~UnixDomainServer() noexcept override;

    score::cpp::expected_blank<score::os::Error> StartListening(
        ConnectCallback connect_callback,
        DisconnectCallback disconnect_callback = DisconnectCallback{},
        MessageCallback sent_callback = MessageCallback{},
        MessageCallback sent_with_reply_callback = MessageCallback{}) noexcept override;

    void StopListening() noexcept override;

  private:
    void ProcessConnect() noexcept;

    std::shared_ptr<UnixDomainEngine> engine_;
    const score::cpp::pmr::string identifier_;
    const std::uint32_t max_request_size_;
    const std::uint32_t max_reply_size_;
    const std::uint32_t max_notify_size_;

    std::int32_t server_fd_;
    std::recursive_mutex connection_setup_mutex_;

    ConnectCallback connect_callback_;
    DisconnectCallback disconnect_callback_;
    MessageCallback sent_callback_;
    MessageCallback sent_with_reply_callback_;

    ISharedResourceEngine::CommandQueueEntry listener_command_;
    ISharedResourceEngine::PosixEndpointEntry listener_endpoint_;
};

}  // namespace detail
}  // namespace message_passing
}  // namespace score

#endif  // SCORE_LIB_MESSAGE_PASSING_UNIX_DOMAIN_UNIX_DOMAIN_SERVER_H
