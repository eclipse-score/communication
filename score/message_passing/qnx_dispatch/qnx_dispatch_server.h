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
#ifndef SCORE_LIB_MESSAGE_PASSING_QNX_DISPATCH_QNX_DISPATCH_SERVER_H
#define SCORE_LIB_MESSAGE_PASSING_QNX_DISPATCH_QNX_DISPATCH_SERVER_H

#include "score/message_passing/i_server.h"

#include "score/message_passing/i_server_connection.h"
#include "score/message_passing/qnx_dispatch/qnx_dispatch_engine.h"
#include "score/message_passing/qnx_dispatch/qnx_dispatch_server_factory.h"

#include <score/string.hpp>

#include <optional>

namespace score::message_passing::detail
{

// NOLINTNEXTLINE(score-struct-usage-compliance): see QnxDispatchEngine::ResourceManagerServer
class QnxDispatchServer final : public IServer, private QnxDispatchEngine::ResourceManagerServer
{
  public:
    // NOLINTNEXTLINE(score-struct-usage-compliance): see QnxDispatchEngine::ResourceManagerConnection
    class ServerConnection final : public IServerConnection, public QnxDispatchEngine::ResourceManagerConnection
    {
      public:
        ServerConnection(ClientIdentity client_identity, const QnxDispatchServer& server) noexcept;

        // User methods
        const ClientIdentity& GetClientIdentity() const noexcept override;
        UserData& GetUserData() noexcept override;

        score::cpp::expected_blank<score::os::Error> Reply(
            score::cpp::span<const std::uint8_t> message) noexcept override;
        score::cpp::expected_blank<score::os::Error> Notify(
            score::cpp::span<const std::uint8_t> message) noexcept override;
        void RequestDisconnect() noexcept override;

        // ResourceManagerConnection methods
        bool ProcessInput(const std::uint8_t code,
                          const score::cpp::span<const std::uint8_t> message) noexcept override;
        bool HasSomethingToRead() noexcept override;
        std::int32_t ProcessReadRequest(resmgr_context_t* const ctp) noexcept override;
        void ProcessDisconnect() noexcept override;

        // Server methods
        void AcceptConnection(UserData&& data, score::cpp::pmr::unique_ptr<ServerConnection>&& self) noexcept;

        ~ServerConnection() noexcept;

        ServerConnection(const ServerConnection&) noexcept = delete;
        ServerConnection(ServerConnection&&) noexcept = delete;
        ServerConnection& operator=(const ServerConnection&) noexcept = delete;
        ServerConnection& operator=(ServerConnection&&) noexcept = delete;

      private:
        QnxDispatchServer& GetQnxDispatchServer() noexcept
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast) by API design
            return static_cast<QnxDispatchServer&>(GetServer());
        }

        std::optional<UserData> user_data_;
        ClientIdentity client_identity_;

        score::cpp::pmr::unique_ptr<ServerConnection> self_;

        class SendMessage : public score::containers::intrusive_list_element<>
        {
          public:
            using allocator_type = score::cpp::pmr::polymorphic_allocator<SendMessage>;
            explicit SendMessage(const allocator_type& allocator)
                : score::containers::intrusive_list_element<>{}, message{allocator}, code{}
            {
            }

            score::cpp::pmr::vector<std::uint8_t> message;
            std::uint8_t code;
        };
        SendMessage reply_message_;
        score::cpp::pmr::vector<SendMessage> notify_storage_;
        score::containers::intrusive_list<SendMessage> notify_pool_;
        score::containers::intrusive_list<SendMessage> send_queue_;
    };

    QnxDispatchServer(std::shared_ptr<QnxDispatchEngine> engine,
                      const ServiceProtocolConfig& protocol_config,
                      const IServerFactory::ServerConfig& server_config) noexcept;
    ~QnxDispatchServer() noexcept override;

    QnxDispatchServer(const QnxDispatchServer&) = delete;
    QnxDispatchServer(QnxDispatchServer&&) = delete;
    QnxDispatchServer& operator=(const QnxDispatchServer&) = delete;
    QnxDispatchServer& operator=(QnxDispatchServer&&) = delete;

    score::cpp::expected_blank<score::os::Error> StartListening(
        ConnectCallback connect_callback,
        DisconnectCallback disconnect_callback,
        MessageCallback sent_callback,
        MessageCallback sent_with_reply_callback) noexcept override;

    void StopListening() noexcept override;

  private:
    std::int32_t ProcessConnect(resmgr_context_t* const ctp, io_open_t* const msg) noexcept override;

    const score::cpp::pmr::string identifier_;
    const std::uint32_t max_request_size_;
    const std::uint32_t max_reply_size_;
    const std::uint32_t max_notify_size_;
    const IServerFactory::ServerConfig server_config_;

    ConnectCallback connect_callback_;
    DisconnectCallback disconnect_callback_;
    MessageCallback sent_callback_;
    MessageCallback sent_with_reply_callback_;

    ISharedResourceEngine::CommandQueueEntry listener_command_;
    ISharedResourceEngine::PosixEndpointEntry listener_endpoint_;
};

}  // namespace score::message_passing::detail

#endif  // SCORE_LIB_MESSAGE_PASSING_QNX_DISPATCH_QNX_DISPATCH_SERVER_H
