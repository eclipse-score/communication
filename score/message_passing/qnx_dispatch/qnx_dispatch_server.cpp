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
#include "score/message_passing/qnx_dispatch/qnx_dispatch_server.h"

#include "score/message_passing/client_server_communication.h"
#include "score/message_passing/qnx_dispatch/qnx_dispatch_engine.h"

#include "score/os/errno.h"
#include <score/expected.hpp>
#include <score/utility.hpp>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace score::message_passing::detail
{

QnxDispatchServer::ServerConnection::ServerConnection(ClientIdentity client_identity,
                                                      const QnxDispatchServer& server) noexcept
    : IServerConnection{},
      QnxDispatchEngine::ResourceManagerConnection{},
      user_data_{},
      client_identity_{client_identity},
      self_{},
      reply_message_{server.engine_->GetMemoryResource()},
      notify_storage_{server.server_config_.max_queued_notifies, server.engine_->GetMemoryResource()},
      notify_pool_{},
      send_queue_{}
{
    reply_message_.message.reserve(static_cast<std::size_t>(server.max_reply_size_));
    reply_message_.code = score::cpp::to_underlying(ServerToClient::REPLY);

    for (auto& notify_message : notify_storage_)
    {
        notify_message.message.reserve(static_cast<std::size_t>(server.max_notify_size_));
        notify_message.code = score::cpp::to_underlying(ServerToClient::NOTIFY);
    }
    notify_pool_.assign(notify_storage_.begin(), notify_storage_.end());
}

// coverity[autosar_cpp14_m7_3_1_violation] false-positive: class method (Ticket-234468)
void QnxDispatchServer::ServerConnection::AcceptConnection(UserData&& data,
                                                           score::cpp::pmr::unique_ptr<ServerConnection>&& self) noexcept
{
    user_data_ = std::move(data);
    self_ = std::move(self);
}

// coverity[autosar_cpp14_m7_3_1_violation] false-positive: class method (Ticket-234468)
const ClientIdentity& QnxDispatchServer::ServerConnection::GetClientIdentity() const noexcept
{
    return client_identity_;
}

// coverity[autosar_cpp14_m7_3_1_violation] false-positive: class method (Ticket-234468)
UserData& QnxDispatchServer::ServerConnection::GetUserData() noexcept
{
    return *user_data_;
}

score::cpp::expected_blank<score::os::Error> QnxDispatchServer::ServerConnection::Reply(
    score::cpp::span<const std::uint8_t> message) noexcept
{
    auto& server = GetQnxDispatchServer();

    if (message.size() > server.max_reply_size_)
    {
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOMEM));
    }

    reply_message_.message.assign(message.begin(), message.end());

    send_queue_.push_back(reply_message_);
    auto& os_resources = server.engine_->GetOsResources();
    // NOLINTNEXTLINE(score-banned-function) implementing FFI wrapper
    os_resources.iofunc->iofunc_notify_trigger(
        notify_.data(), static_cast<std::int32_t>(send_queue_.size()), IOFUNC_NOTIFY_INPUT);
    return {};
}

score::cpp::expected_blank<score::os::Error> QnxDispatchServer::ServerConnection::Notify(
    score::cpp::span<const std::uint8_t> message) noexcept
{
    auto& server = GetQnxDispatchServer();

    if (message.size() > server.max_notify_size_)
    {
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOMEM));
    }

    if (notify_pool_.empty())
    {
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOMEM));
    }

    auto& notify_message = notify_pool_.front();
    notify_pool_.pop_front();
    notify_message.message.assign(message.begin(), message.end());
    send_queue_.push_back(notify_message);
    auto& os_resources = server.engine_->GetOsResources();

    std::int32_t size = 1;
    // NOLINTNEXTLINE(score-banned-function) implementing FFI wrapper
    os_resources.iofunc->iofunc_notify_trigger(notify_.data(), size, IOFUNC_NOTIFY_INPUT);
    return {};
}

void QnxDispatchServer::ServerConnection::RequestDisconnect() noexcept
{
    // TODO: implement as ionotify for zero-read (once this functionality is demanded)
}

// Suppress AUTOSAR C++14 A15-5-3 violation for variant access in ProcessInput
// Rationale: std::get<HandlerPointerT>(user_data) is preceded by std::holds_alternative check,
// guaranteeing the variant contains the correct type. No std::bad_variant_access can be thrown.
// The exception handling rule is satisfied as no actual exception can occur in this context.
// coverity[autosar_cpp14_a15_5_3_violation]
bool QnxDispatchServer::ServerConnection::ProcessInput(const std::uint8_t code,
                                                       const score::cpp::span<const std::uint8_t> message) noexcept
{
    auto& server = GetQnxDispatchServer();
    auto& user_data = *user_data_;
    using HandlerPointerT = score::cpp::pmr::unique_ptr<IConnectionHandler>;
    bool result = false;

    switch (code)
    {
        case score::cpp::to_underlying(ClientToServer::REQUEST):
        {
            score::cpp::expected_blank<score::os::Error> response_result;
            if (std::holds_alternative<HandlerPointerT>(user_data))
            {
                response_result = std::get<HandlerPointerT>(user_data)->OnMessageSentWithReply(*this, message);
            }
            else
            {
                response_result = server.sent_with_reply_callback_(*this, message);
            }
            result = response_result.has_value();
            break;
        }

        case score::cpp::to_underlying(ClientToServer::SEND):
        {
            score::cpp::expected_blank<score::os::Error> send_result;
            if (std::holds_alternative<HandlerPointerT>(user_data))
            {
                send_result = std::get<HandlerPointerT>(user_data)->OnMessageSent(*this, message);
            }
            else
            {
                send_result = server.sent_callback_(*this, message);
            }
            result = send_result.has_value();
            break;
        }

        default:
            // unrecognised message; drop connection
            result = false;
            break;
    }

    return result;
}

bool QnxDispatchServer::ServerConnection::HasSomethingToRead() noexcept
{
    return !send_queue_.empty();
}

// Suppress AUTOSAR C++14 A8-4-10 violation for QNX resource manager API
// Rationale: ProcessReadRequest must conform to QNX resmgr callback signature.
// QNX API requires pointer parameter - cannot use reference due to C API constraints.
// coverity[autosar_cpp14_a8_4_10_violation]
std::int32_t QnxDispatchServer::ServerConnection::ProcessReadRequest(resmgr_context_t* const ctp) noexcept
{
    if (send_queue_.empty())
    {
        return EAGAIN;
    }

    auto& send_message = send_queue_.front();
    // Suppress AUTOSAR C++14 A0-1-1 violation for io_size variable
    // Rationale: io_size is used as template parameter for std::array<iov_t, io_size>.
    // coverity[autosar_cpp14_a0_1_1_violation]
    constexpr std::size_t io_size = 2U;
    std::array<iov_t, io_size> io{};
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access) C API
    io[0].iov_base = &send_message.code;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access) C API
    io[0].iov_len = sizeof(send_message.code);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access) C API
    io[1].iov_base = send_message.message.data();
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access) C API
    io[1].iov_len = send_message.message.size();

    // Calculate total size with bounds checking
    constexpr std::size_t header_size = sizeof(send_message.code);
    const std::size_t message_size = send_message.message.size();
    // Suppress AUTOSAR C++14 A4-7-1 violation for message size calculation
    // Rationale: Message sizes are validated at construction and user input validation.
    // header_size is compile-time constant (sizeof(uint8_t) = 1).
    // message_size is bounded by max_reply_size_ or max_notify_size_ limits.
    // if user specifies too large message size, std::terminate is called beforehand.
    // coverity[autosar_cpp14_a4_7_1_violation]
    const std::size_t total_size = header_size + message_size;

    // Suppress AUTOSAR C++14 M5-0-4
    // Rationale: _IO_SET_READ_NBYTES requires int32_t parameter, but we've validated
    // user input(max_reply_size or max_notify_size). It will doesn't lose precision.
    // coverity[autosar_cpp14_m5_0_4_violation]
    // coverity[autosar_cpp14_a4_7_1_violation]
    _IO_SET_READ_NBYTES(ctp, static_cast<std::int32_t>(total_size));
    auto& server = GetQnxDispatchServer();
    auto& os_resources = server.engine_->GetOsResources();

    // TODO: drop on error?
    // could also be dispatch->resmgr_msgreplyv(), if we decide to introduce that
    score::cpp::ignore = os_resources.channel->MsgReplyv(ctp->rcvid, ctp->status, io.data(), io.size());
    send_queue_.pop_front();
    if (send_message.code == score::cpp::to_underlying(ServerToClient::NOTIFY))
    {
        // if a NOTIFY message instance, return it to notify pool
        notify_pool_.push_front(send_message);
    }

    // Suppress AUTOSAR C++14 M5-0-10, A5-2-2 violation in QNX system macro
    // Rationale: _RESMGR_NOREPLY is from QNX system headers.
    // Cannot modify QNX headers.
    // coverity[autosar_cpp14_m5_0_10_violation]
    // coverity[autosar_cpp14_a5_2_2_violation]
    return _RESMGR_NOREPLY;
}

void QnxDispatchServer::ServerConnection::ProcessDisconnect() noexcept
{
    self_.reset();
}

// Suppress AUTOSAR C++14 A15-5-1, A15-5-3 violation for variant access in destructor
// Rationale: std::get<HandlerPointerT>(user_data) is preceded by std::holds_alternative check,
// guaranteeing the variant contains the correct type. No std::bad_variant_access can be thrown.
// The exception handling rule is satisfied as no actual exception can occur in this context.
// coverity[autosar_cpp14_a15_5_1_violation]
// coverity[autosar_cpp14_a15_5_3_violation]
QnxDispatchServer::ServerConnection::~ServerConnection() noexcept
{
    auto& server = GetQnxDispatchServer();
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
            server.disconnect_callback_(*this);
        }
    }
    // TODO: if reusing connection instance, don't forget to restore notify_pool_
    send_queue_.clear();
    notify_pool_.clear();
}

QnxDispatchServer::QnxDispatchServer(std::shared_ptr<QnxDispatchEngine> engine,
                                     const ServiceProtocolConfig& protocol_config,
                                     const IServerFactory::ServerConfig& server_config) noexcept
    : IServer{},
      QnxDispatchEngine::ResourceManagerServer{std::move(engine)},
      identifier_{protocol_config.identifier.data(), protocol_config.identifier.size(), engine_->GetMemoryResource()},
      max_request_size_{protocol_config.max_send_size},
      max_reply_size_{protocol_config.max_reply_size},
      max_notify_size_{protocol_config.max_notify_size},
      server_config_{server_config},
      connect_callback_{},
      disconnect_callback_{},
      sent_callback_{},
      sent_with_reply_callback_{},
      listener_command_{},
      listener_endpoint_{}
{
    score::cpp::ignore = max_request_size_;  // currently unused
}

QnxDispatchServer::~QnxDispatchServer() noexcept
{
    // MISRA C++ 2008 Rule 12-1-1 Compliant: Direct base class call
    // Rationale: Calling base class Stop() directly avoids virtual dispatch
    // and dynamic type usage during destruction
    QnxDispatchEngine::ResourceManagerServer::Stop();
}

// NOLINTNEXTLINE(google-default-arguments) TODO:
// coverity[autosar_cpp14_m7_3_1_violation] false-positive: class method (Ticket-234468)
score::cpp::expected_blank<score::os::Error> QnxDispatchServer::StartListening(ConnectCallback connect_callback,
                                                                      DisconnectCallback disconnect_callback,
                                                                      MessageCallback sent_callback,
                                                                      MessageCallback sent_with_reply_callback) noexcept
{
    connect_callback_ = std::move(connect_callback);
    disconnect_callback_ = std::move(disconnect_callback);
    sent_callback_ = std::move(sent_callback);
    sent_with_reply_callback_ = std::move(sent_with_reply_callback);

    const QnxResourcePath path{identifier_};

    // ResourceManagerServer::
    return Start(path);
}

void QnxDispatchServer::StopListening() noexcept
{
    // ResourceManagerServer::
    Stop();
}

// Suppress "AUTOSAR C++14 A9-5-1", The rule states: "Unions shall not be used."
// QNX union.
// Suppress AUTOSAR C++14 A8-4-10 violation for QNX resource manager API
// Rationale: ProcessReadRequest must conform to QNX resmgr callback signature.
// QNX API requires pointer parameter - cannot use reference due to C API constraints.
// coverity[autosar_cpp14_a9_5_1_violation]
// coverity[autosar_cpp14_a8_4_10_violation]
std::int32_t QnxDispatchServer::ProcessConnect(resmgr_context_t* const ctp, io_open_t* const msg) noexcept
{
    auto& os_resources = engine_->GetOsResources();
    auto& channel = os_resources.channel;

    _client_info cinfo{};
    const auto result = channel->ConnectClientInfo(ctp->info.scoid, &cinfo, 0);
    if (!result.has_value())
    {
        return EINVAL;
    }
    ClientIdentity identity{ctp->info.pid, cinfo.cred.euid, cinfo.cred.egid};
    // here, we need to provide server directly as an argument
    auto connection = score::cpp::pmr::make_unique<ServerConnection>(engine_->GetMemoryResource(), identity, *this);

    auto data_expected = connect_callback_(*connection);
    if (!data_expected.has_value())
    {
        return EACCES;
    }

    const score::cpp::expected_blank<std::int32_t> status = engine_->AttachConnection(ctp, msg, *this, *connection);
    if (!status.has_value())
    {
        return status.error();
    }

    // from now on, the connection can extract server from its iofunc_ocb_t base class
    connection->AcceptConnection(std::move(data_expected.value()), std::move(connection));
    return EOK;
}

}  // namespace score::message_passing::detail
