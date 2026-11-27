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
#include "score/someip/sd_client/sd_client.h"

#include <array>
#include <memory>
#include <utility>

namespace score::someip_sd
{

namespace
{

using score::message_passing::IClientConnection;

const score::message_passing::ServiceProtocolConfig kProtocolConfig{protocol::kServiceIdentifier,
                                                                    protocol::kMessageSize,
                                                                    protocol::kMessageSize,
                                                                    protocol::kMessageSize};
// {max_async_replies, max_queued_sends, fully_ordered, truly_async, sync_first_connect}: truly asynchronous sends with
// a client-side queue, so Send/SendWithCallback never block the caller in the transport (a full queue fails the send).
const score::message_passing::IClientFactory::ClientConfig kClientConfig{1U, 16U, true, true, false};

bool IsValidFlag(const std::uint8_t flag) noexcept
{
    return (flag == 0U) || (flag == 1U);
}

}  // namespace

SdClient::SdClient(score::message_passing::IClientFactory& client_factory,
                   AvailabilityHandler availability_handler,
                   SubscriptionStateHandler subscription_state_handler)
    : SdClient{client_factory, std::move(availability_handler), std::move(subscription_state_handler), {}}
{
}

SdClient::SdClient(score::message_passing::IClientFactory& client_factory,
                   AvailabilityHandler availability_handler,
                   SubscriptionStateHandler subscription_state_handler,
                   ConnectionLostHandler connection_lost_handler)
    : availability_handler_{std::move(availability_handler)},
      subscription_state_handler_{std::move(subscription_state_handler)},
      connection_lost_handler_{std::move(connection_lost_handler)},
      client_factory_{client_factory}
{
}

SdClient::~SdClient()
{
    const std::lock_guard lifecycle_lock{lifecycle_mutex_};
    StopAndResetConnection();
}

std::optional<std::uint16_t> SdClient::Connect(std::chrono::milliseconds timeout)
{
    const std::lock_guard lifecycle_lock{lifecycle_mutex_};
    {
        const std::lock_guard connection_lock{connection_mutex_};
        if (client_id_.has_value())
        {
            std::lock_guard state_lock{state_mutex_};
            if (state_ == IClientConnection::State::kReady)
            {
                return client_id_;
            }
        }
    }

    StopAndResetConnection();

    auto connection = client_factory_.Create(kProtocolConfig, kClientConfig);
    if (!connection)
    {
        return std::nullopt;
    }

    const auto deadline = std::chrono::steady_clock::now() + timeout;
    {
        const std::lock_guard connection_lock{connection_mutex_};
        connection_ = std::move(connection);
        client_id_.reset();
    }
    {
        const std::lock_guard state_lock{state_mutex_};
        state_ = IClientConnection::State::kStarting;
        intentional_shutdown_ = false;
        session_was_ready_ = false;
        session_loss_pending_ = false;
    }
    IClientConnection* connection_ptr{nullptr};
    {
        const std::lock_guard connection_lock{connection_mutex_};
        connection_ptr = connection_.get();
    }
    connection_ptr->Start(
        [this](IClientConnection::State state) {
            bool connection_lost{false};
            {
                std::lock_guard<std::mutex> lock{state_mutex_};
                if (state == IClientConnection::State::kReady)
                {
                    session_was_ready_ = true;
                }
                if ((state == IClientConnection::State::kStopping || state == IClientConnection::State::kStopped) &&
                    session_was_ready_ && !intentional_shutdown_)
                {
                    session_loss_pending_ = true;
                }
                connection_lost = (state == IClientConnection::State::kStopped) && session_loss_pending_;
                if (connection_lost)
                {
                    session_was_ready_ = false;
                    session_loss_pending_ = false;
                }
                state_ = state;
                // Notify under the lock: a waiter (e.g. ~SdClient) may destroy *this once it sees the new state.
                state_condition_.notify_all();
            }
            if (connection_lost && connection_lost_handler_)
            {
                connection_lost_handler_();
            }
        },
        [this](score::cpp::span<const std::uint8_t> data) {
            OnNotify(data);
        });

    {
        std::unique_lock<std::mutex> lock{state_mutex_};
        if (!state_condition_.wait_until(lock, deadline, [this]() {
                return state_ != IClientConnection::State::kStarting;
            }))
        {
            lock.unlock();
            StopAndResetConnection();
            return std::nullopt;
        }
        if (state_ != IClientConnection::State::kReady)
        {
            lock.unlock();
            StopAndResetConnection();
            return std::nullopt;
        }
    }

    protocol::Message request{};
    request.opcode = protocol::Opcode::kRegister;
    const auto request_buffer = protocol::Encode(request);
    struct RegistrationReply
    {
        std::mutex mutex;
        std::condition_variable condition;
        bool completed{false};
        std::optional<protocol::Message> message;
    };
    const auto registration_reply = std::make_shared<RegistrationReply>();
    score::cpp::expected_blank<score::os::Error> send_result{};
    {
        const std::lock_guard connection_lock{connection_mutex_};
        if (!connection_)
        {
            return std::nullopt;
        }
        send_result = connection_->SendWithCallback(
            score::cpp::span<const std::uint8_t>{request_buffer.data(), request_buffer.size()},
            [registration_reply](auto reply) {
                std::optional<protocol::Message> message;
                if (reply.has_value())
                {
                    const auto reply_message = protocol::Decode(reply->data(), reply->size());
                    if (reply_message.has_value() && (reply_message->opcode == protocol::Opcode::kRegister))
                    {
                        message = reply_message;
                    }
                }
                {
                    const std::lock_guard lock{registration_reply->mutex};
                    registration_reply->message = message;
                    registration_reply->completed = true;
                }
                registration_reply->condition.notify_all();
            });
    }
    if (!send_result.has_value())
    {
        StopAndResetConnection();
        return std::nullopt;
    }

    std::optional<protocol::Message> message;
    {
        std::unique_lock lock{registration_reply->mutex};
        if (!registration_reply->condition.wait_until(lock, deadline, [&registration_reply]() {
                return registration_reply->completed;
            }))
        {
            lock.unlock();
            StopAndResetConnection();
            return std::nullopt;
        }
        message = registration_reply->message;
    }
    if (!message.has_value())
    {
        StopAndResetConnection();
        return std::nullopt;
    }

    {
        const std::lock_guard connection_lock{connection_mutex_};
        client_id_ = message->client_id;
    }
    return message->client_id;
}

bool SdClient::RequestService(std::uint16_t service, std::uint16_t instance, std::uint8_t major, std::uint32_t minor)
{
    protocol::Message message{};
    message.opcode = protocol::Opcode::kRequestService;
    message.service = service;
    message.instance = instance;
    message.major = major;
    message.minor = minor;
    return Send(message);
}

bool SdClient::RequestService(const ServiceDetails& details)
{
    return RequestService(details.service, details.instance, details.major, details.minor);
}

bool SdClient::ReleaseService(std::uint16_t service, std::uint16_t instance)
{
    protocol::Message message{};
    message.opcode = protocol::Opcode::kReleaseService;
    message.service = service;
    message.instance = instance;
    return Send(message);
}

bool SdClient::Subscribe(std::uint16_t service, std::uint16_t instance, std::uint16_t eventgroup, std::uint8_t major)
{
    protocol::Message message{};
    message.opcode = protocol::Opcode::kSubscribe;
    message.service = service;
    message.instance = instance;
    message.eventgroup = eventgroup;
    message.major = major;
    return Send(message);
}

bool SdClient::Unsubscribe(std::uint16_t service, std::uint16_t instance, std::uint16_t eventgroup)
{
    protocol::Message message{};
    message.opcode = protocol::Opcode::kUnsubscribe;
    message.service = service;
    message.instance = instance;
    message.eventgroup = eventgroup;
    return Send(message);
}

bool SdClient::OfferService(std::uint16_t service, std::uint16_t instance, std::uint8_t major, std::uint32_t minor)
{
    protocol::Message message{};
    message.opcode = protocol::Opcode::kOfferService;
    message.service = service;
    message.instance = instance;
    message.major = major;
    message.minor = minor;
    return Send(message);
}

bool SdClient::StopOfferService(std::uint16_t service, std::uint16_t instance)
{
    protocol::Message message{};
    message.opcode = protocol::Opcode::kStopOfferService;
    message.service = service;
    message.instance = instance;
    return Send(message);
}

bool SdClient::Send(const protocol::Message& message)
{
    const std::lock_guard connection_lock{connection_mutex_};
    if (!connection_ || !client_id_.has_value())
    {
        return false;
    }
    {
        const std::lock_guard state_lock{state_mutex_};
        if (state_ != IClientConnection::State::kReady)
        {
            return false;
        }
    }
    const auto buffer = protocol::Encode(message);
    return connection_->Send(score::cpp::span<const std::uint8_t>{buffer.data(), buffer.size()}).has_value();
}

void SdClient::StopAndResetConnection()
{
    score::message_passing::IClientConnection* connection{nullptr};
    {
        const std::lock_guard connection_lock{connection_mutex_};
        connection = connection_.get();
    }
    if (connection == nullptr)
    {
        return;
    }

    bool is_stopped{false};
    {
        const std::lock_guard state_lock{state_mutex_};
        is_stopped = state_ == IClientConnection::State::kStopped;
        intentional_shutdown_ = !is_stopped;
    }
    if (!is_stopped)
    {
        connection->Stop();
        std::unique_lock state_lock{state_mutex_};
        state_condition_.wait(state_lock, [this]() {
            return state_ == IClientConnection::State::kStopped;
        });
    }

    score::cpp::pmr::unique_ptr<IClientConnection> stopped_connection{};
    {
        const std::lock_guard connection_lock{connection_mutex_};
        stopped_connection = std::move(connection_);
        client_id_.reset();
    }
    // The connection destructor may wait for a running state callback (which can call into the owner), so it must
    // not run while connection_mutex_ is held by this thread.
    stopped_connection.reset();
    {
        const std::lock_guard state_lock{state_mutex_};
        intentional_shutdown_ = false;
    }
}

void SdClient::OnNotify(score::cpp::span<const std::uint8_t> data)
{
    const auto message = protocol::Decode(data.data(), data.size());
    if (!message.has_value())
    {
        return;
    }
    switch (message->opcode)
    {
        case protocol::Opcode::kAvailability:
            if (availability_handler_ && IsValidFlag(message->flag))
            {
                availability_handler_(message->service, message->instance, message->flag == 1U);
            }
            break;
        case protocol::Opcode::kSubscriptionState:
            if (subscription_state_handler_ && IsValidFlag(message->flag))
            {
                subscription_state_handler_(
                    message->service, message->instance, message->eventgroup, message->flag == 1U);
            }
            break;
        case protocol::Opcode::kRegister:
        case protocol::Opcode::kRequestService:
        case protocol::Opcode::kReleaseService:
        case protocol::Opcode::kSubscribe:
        case protocol::Opcode::kUnsubscribe:
        case protocol::Opcode::kOfferService:
        case protocol::Opcode::kStopOfferService:
        default:
            break;
    }
}

}  // namespace score::someip_sd
