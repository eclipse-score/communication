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
#include "score/mw/com/service_discovery/service_discovery_message_passing_server.h"

#include "score/message_passing/i_server_connection.h"

#include <array>
#include <cerrno>
#include <variant>

namespace score::mw::com::service_discovery
{

namespace
{

[[nodiscard]] auto DoesSearchMatchKey(const ServiceKey& search_key, const ServiceKey& changed_key) noexcept -> bool
{
    if (search_key.service_id != changed_key.service_id)
    {
        return false;
    }

    return IsAnyInstanceKey(search_key) || search_key.instance_id == changed_key.instance_id;
}

}  // namespace

auto ServiceDiscoveryMessagePassingServer::GetProviderContext(
    score::message_passing::IServerConnection& connection) noexcept -> ProviderContext
{
    ProviderContext provider_context{};
    const auto& client_identity = connection.GetClientIdentity();
    provider_context.provider_uid = static_cast<std::uint32_t>(client_identity.uid);
    provider_context.provider_pid = static_cast<std::uint32_t>(client_identity.pid);

    if (const auto* session_id = std::get_if<std::uintptr_t>(&connection.GetUserData()))
    {
        provider_context.provider_session_id = static_cast<std::uint64_t>(*session_id);
    }

    return provider_context;
}

auto ServiceDiscoveryMessagePassingServer::RemoveSubscriberEntries(const std::uint64_t session_id) noexcept -> void
{
    for (auto& subscriber : subscribers_)
    {
        if (subscriber.active && subscriber.session_id == session_id)
        {
            subscriber.active = false;
            subscriber.search_handle = 0U;
            subscriber.connection = nullptr;
        }
    }
}

auto ServiceDiscoveryMessagePassingServer::NotifySubscribers(const ServiceKey& key) noexcept -> void
{
    for (auto& subscriber : subscribers_)
    {
        if (subscriber.active && subscriber.connection != nullptr && DoesSearchMatchKey(subscriber.key, key))
        {
            const auto registrations = daemon_.Resolve(subscriber.key);

            ProtocolNotification notification{};
            notification.search_handle = subscriber.search_handle;
            notification.key = subscriber.key;
            notification.response.status_code = 0U;
            for (std::size_t index = 0U; index < registrations.size; ++index)
            {
                if (!notification.response.PushRegistration(registrations.values[index]))
                {
                    break;
                }
            }

            std::array<std::uint8_t, kMaxNotificationPayloadSize> notification_buffer{};
            std::size_t notification_size{0U};
            if (!SerializeNotification(
                    notification,
                    score::cpp::span<std::uint8_t>{notification_buffer.data(), notification_buffer.size()},
                    notification_size))
            {
                continue;
            }

            score::cpp::ignore = subscriber.connection->Notify(
                score::cpp::span<const std::uint8_t>{notification_buffer.data(), notification_size});
        }
    }
}

auto ServiceDiscoveryMessagePassingServer::HandleStartFindService(score::message_passing::IServerConnection& connection,
                                                                  const ProtocolRequest& request) noexcept
    -> score::cpp::expected_blank<score::os::Error>
{
    const auto provider_context = GetProviderContext(connection);
    for (auto& subscriber : subscribers_)
    {
        if (subscriber.active && subscriber.session_id == provider_context.provider_session_id &&
            subscriber.key == request.registration.key)
        {
            subscriber.connection = &connection;

            ProtocolResponse response{};
            response.status_code = 0U;
            response.search_handle = subscriber.search_handle;
            const auto registrations = daemon_.Resolve(request.registration.key);
            for (std::size_t index = 0U; index < registrations.size; ++index)
            {
                if (!response.PushRegistration(registrations.values[index]))
                {
                    break;
                }
            }

            std::array<std::uint8_t, kMaxResponsePayloadSize> response_buffer{};
            std::size_t response_size{0U};
            if (!SerializeResponse(response,
                                   score::cpp::span<std::uint8_t>{response_buffer.data(), response_buffer.size()},
                                   response_size))
            {
                return score::cpp::make_unexpected(score::os::Error::createFromErrno(EINVAL));
            }
            return connection.Reply(score::cpp::span<const std::uint8_t>{response_buffer.data(), response_size});
        }
    }

    for (auto& subscriber : subscribers_)
    {
        if (!subscriber.active)
        {
            subscriber.active = true;
            subscriber.session_id = provider_context.provider_session_id;
            subscriber.search_handle = next_search_handle_;
            ++next_search_handle_;
            subscriber.key = request.registration.key;
            subscriber.connection = &connection;

            ProtocolResponse response{};
            response.status_code = 0U;
            response.search_handle = subscriber.search_handle;
            const auto registrations = daemon_.Resolve(request.registration.key);
            for (std::size_t index = 0U; index < registrations.size; ++index)
            {
                if (!response.PushRegistration(registrations.values[index]))
                {
                    break;
                }
            }

            std::array<std::uint8_t, kMaxResponsePayloadSize> response_buffer{};
            std::size_t response_size{0U};
            if (!SerializeResponse(response,
                                   score::cpp::span<std::uint8_t>{response_buffer.data(), response_buffer.size()},
                                   response_size))
            {
                return score::cpp::make_unexpected(score::os::Error::createFromErrno(EINVAL));
            }
            return connection.Reply(score::cpp::span<const std::uint8_t>{response_buffer.data(), response_size});
        }
    }

    return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOMEM));
}

auto ServiceDiscoveryMessagePassingServer::HandleStopFindService(score::message_passing::IServerConnection& connection,
                                                                 const ProtocolRequest& request) noexcept
    -> score::cpp::expected_blank<score::os::Error>
{
    std::array<std::uint8_t, kMaxResponsePayloadSize> response_buffer{};
    std::size_t response_size{0U};
    ProtocolResponse response{};
    response.status_code = 0U;
    response.search_handle = request.search_handle;

    const auto provider_context = GetProviderContext(connection);
    for (auto& subscriber : subscribers_)
    {
        if (subscriber.active && subscriber.session_id == provider_context.provider_session_id &&
            subscriber.search_handle == request.search_handle)
        {
            subscriber.active = false;
            subscriber.search_handle = 0U;
            subscriber.connection = nullptr;
            if (!SerializeResponse(response,
                                   score::cpp::span<std::uint8_t>{response_buffer.data(), response_buffer.size()},
                                   response_size))
            {
                return score::cpp::make_unexpected(score::os::Error::createFromErrno(EINVAL));
            }
            return connection.Reply(score::cpp::span<const std::uint8_t>{response_buffer.data(), response_size});
        }
    }

    if (!SerializeResponse(
            response, score::cpp::span<std::uint8_t>{response_buffer.data(), response_buffer.size()}, response_size))
    {
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(EINVAL));
    }
    return connection.Reply(score::cpp::span<const std::uint8_t>{response_buffer.data(), response_size});
}

ServiceDiscoveryMessagePassingServer::ServiceDiscoveryMessagePassingServer(ServiceDiscoveryDaemon& daemon) noexcept
    : ServiceDiscoveryMessagePassingServer{daemon, Config{}}
{
}

ServiceDiscoveryMessagePassingServer::ServiceDiscoveryMessagePassingServer(ServiceDiscoveryDaemon& daemon,
                                                                           Config config) noexcept
    : daemon_{daemon}, config_{std::move(config)}
{
}

auto ServiceDiscoveryMessagePassingServer::Start() noexcept -> bool
{
    const score::message_passing::ServiceProtocolConfig protocol_config{
        config_.service_identifier, config_.max_send_size, config_.max_reply_size, kMaxNotificationPayloadSize};
    const score::message_passing::IServerFactory::ServerConfig server_config{
        config_.max_queued_sends, config_.pre_alloc_connections, 8U};

    server_ = server_factory_.Create(protocol_config, server_config);
    if (!server_)
    {
        return false;
    }

    auto connect_callback = [this](score::message_passing::IServerConnection& /*connection*/) -> std::uintptr_t {
        const auto session_id = next_session_id_;
        ++next_session_id_;
        return static_cast<std::uintptr_t>(session_id);
    };

    auto disconnect_callback = [this](score::message_passing::IServerConnection& connection) {
        const auto provider_context = GetProviderContext(connection);
        RemoveSubscriberEntries(provider_context.provider_session_id);
        const auto changed_keys = daemon_.OnDisconnected(provider_context);
        for (std::size_t index = 0U; index < changed_keys.size; ++index)
        {
            NotifySubscribers(changed_keys.values[index]);
        }
    };

    auto sent_with_reply_callback =
        [this](score::message_passing::IServerConnection& connection,
               score::cpp::span<const std::uint8_t> message) -> score::cpp::expected_blank<score::os::Error> {
        const auto request = DeserializeRequest(message);
        if (!request.has_value())
        {
            return score::cpp::make_unexpected(score::os::Error::createFromErrno(EINVAL));
        }

        if (request->operation == ProtocolOperation::kStartFindService)
        {
            return HandleStartFindService(connection, *request);
        }

        if (request->operation == ProtocolOperation::kStopFindService)
        {
            return HandleStopFindService(connection, *request);
        }

        thread_local std::array<std::uint8_t, kMaxResponsePayloadSize> response_payload{};
        std::size_t response_size{0U};

        const auto success =
            daemon_.HandlePayload(message,
                                  score::cpp::span<std::uint8_t>{response_payload.data(), response_payload.size()},
                                  GetProviderContext(connection),
                                  response_size);
        if (!success)
        {
            return score::cpp::make_unexpected(score::os::Error::createFromErrno(EINVAL));
        }

        if (request->operation == ProtocolOperation::kRegister || request->operation == ProtocolOperation::kUnregister)
        {
            const auto decoded_response =
                DeserializeResponse(score::cpp::span<const std::uint8_t>{response_payload.data(), response_size});
            if (decoded_response.has_value() && decoded_response->status_code == 0U)
            {
                NotifySubscribers(request->registration.key);
            }
        }

        return connection.Reply(score::cpp::span<const std::uint8_t>{response_payload.data(), response_size});
    };

    auto sent_callback =
        [](score::message_passing::IServerConnection& /*connection*/,
           score::cpp::span<const std::uint8_t> /*message*/) -> score::cpp::expected_blank<score::os::Error> {
        return {};
    };

    const auto start_result =
        server_->StartListening(connect_callback, disconnect_callback, sent_callback, sent_with_reply_callback);
    return start_result.has_value();
}

auto ServiceDiscoveryMessagePassingServer::Stop() noexcept -> void
{
    if (server_)
    {
        server_->StopListening();
        server_ = nullptr;
    }
}

}  // namespace score::mw::com::service_discovery
