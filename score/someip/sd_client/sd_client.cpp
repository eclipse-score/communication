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
const score::message_passing::IClientFactory::ClientConfig kClientConfig{1U, 16U, true, false, false};

}  // namespace

SdClient::SdClient(AvailabilityHandler availability_handler, SubscriptionStateHandler subscription_state_handler)
    : availability_handler_{std::move(availability_handler)},
      subscription_state_handler_{std::move(subscription_state_handler)}
{
}

SdClient::~SdClient()
{
    if (connection_)
    {
        connection_->Stop();
        connection_.reset();
    }
}

std::optional<std::uint16_t> SdClient::Connect(std::chrono::milliseconds timeout)
{
    connection_ = client_factory_.Create(kProtocolConfig, kClientConfig);
    if (!connection_)
    {
        return std::nullopt;
    }

    connection_->Start(
        [this](IClientConnection::State state) {
            {
                std::lock_guard<std::mutex> lock{state_mutex_};
                state_ = state;
            }
            state_condition_.notify_all();
        },
        [this](score::cpp::span<const std::uint8_t> data) {
            OnNotify(data);
        });

    {
        std::unique_lock<std::mutex> lock{state_mutex_};
        if (!state_condition_.wait_for(lock, timeout, [this]() {
                return state_ != IClientConnection::State::kStarting;
            }))
        {
            return std::nullopt;
        }
        if (state_ != IClientConnection::State::kReady)
        {
            return std::nullopt;
        }
    }

    protocol::Message request{};
    request.opcode = protocol::Opcode::kRegister;
    const auto request_buffer = protocol::Encode(request);
    protocol::Buffer reply_buffer{};
    const auto reply =
        connection_->SendWaitReply(score::cpp::span<const std::uint8_t>{request_buffer.data(), request_buffer.size()},
                                   score::cpp::span<std::uint8_t>{reply_buffer.data(), reply_buffer.size()});
    if (!reply.has_value())
    {
        return std::nullopt;
    }
    const auto message = protocol::Decode(reply.value().data(), reply.value().size());
    if ((!message.has_value()) || (message->opcode != protocol::Opcode::kRegister))
    {
        return std::nullopt;
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
    if (!connection_)
    {
        return false;
    }
    const auto buffer = protocol::Encode(message);
    return connection_->Send(score::cpp::span<const std::uint8_t>{buffer.data(), buffer.size()}).has_value();
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
            if (availability_handler_)
            {
                availability_handler_(message->service, message->instance, message->flag != 0U);
            }
            break;
        case protocol::Opcode::kSubscriptionState:
            if (subscription_state_handler_)
            {
                subscription_state_handler_(
                    message->service, message->instance, message->eventgroup, message->flag != 0U);
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
