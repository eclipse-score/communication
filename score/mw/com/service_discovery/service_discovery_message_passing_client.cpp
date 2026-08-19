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
#include "score/mw/com/service_discovery/service_discovery_message_passing_client.h"

namespace score::mw::com::service_discovery
{

auto ServiceDiscoveryMessagePassingClient::DispatchNotification(const ProtocolNotification& notification) noexcept
    -> void
{
    std::scoped_lock lock{subscription_mutex_};
    for (auto& subscription : subscriptions_)
    {
        if (subscription.active && subscription.search_handle == notification.search_handle)
        {
            subscription.callback(notification);
        }
    }
}

auto ServiceDiscoveryMessagePassingClient::DeactivateSubscription(const std::uint64_t search_handle) noexcept -> void
{
    std::scoped_lock lock{subscription_mutex_};
    for (auto& subscription : subscriptions_)
    {
        if (subscription.active && subscription.search_handle == search_handle)
        {
            subscription.active = false;
            subscription.search_handle = 0U;
            subscription.callback = {};
        }
    }
}

ServiceDiscoveryMessagePassingClient::ServiceDiscoveryMessagePassingClient() noexcept
    : ServiceDiscoveryMessagePassingClient{Config{}}
{
}

ServiceDiscoveryMessagePassingClient::ServiceDiscoveryMessagePassingClient(Config config) noexcept
    : config_{std::move(config)}
{
}

ServiceDiscoveryMessagePassingClient::~ServiceDiscoveryMessagePassingClient() noexcept
{
    Stop();
}

auto ServiceDiscoveryMessagePassingClient::Start(const std::chrono::milliseconds timeout) noexcept -> bool
{
    const score::message_passing::ServiceProtocolConfig protocol_config{
        config_.service_identifier, config_.max_send_size, config_.max_reply_size, kMaxNotificationPayloadSize};
    const score::message_passing::IClientFactory::ClientConfig client_config{
        config_.max_async_replies, config_.max_queued_sends, true, false, true};

    connection_ = client_factory_.Create(protocol_config, client_config);
    if (!connection_)
    {
        return false;
    }

    using State = score::message_passing::IClientConnection::State;

    auto state_callback = [this](const State) {
        state_cv_.notify_all();
    };

    auto notify_callback = [this](score::cpp::span<const std::uint8_t> message) {
        const auto notification = DeserializeNotification(message);
        if (notification.has_value())
        {
            DispatchNotification(*notification);
        }
    };

    connection_->Start(state_callback, notify_callback);

    std::unique_lock lock{state_mutex_};
    return state_cv_.wait_for(lock, timeout, [this]() noexcept {
        return connection_->GetState() == State::kReady;
    });
}

auto ServiceDiscoveryMessagePassingClient::Stop() noexcept -> void
{
    if (connection_)
    {
        connection_->Stop();
        connection_ = nullptr;
    }

    std::scoped_lock subscription_lock{subscription_mutex_};
    for (auto& subscription : subscriptions_)
    {
        subscription.active = false;
        subscription.search_handle = 0U;
        subscription.callback = {};
    }
}

auto ServiceDiscoveryMessagePassingClient::Request(const ProtocolRequest& request) noexcept
    -> std::optional<ProtocolResponse>
{
    if (!connection_)
    {
        return std::nullopt;
    }

    std::array<std::uint8_t, kMaxRequestPayloadSize> request_buffer{};
    std::size_t request_size{0U};
    if (!SerializeRequest(
            request, score::cpp::span<std::uint8_t>{request_buffer.data(), request_buffer.size()}, request_size))
    {
        return std::nullopt;
    }

    std::array<std::uint8_t, kMaxResponsePayloadSize> reply_buffer{};

    const auto reply_result =
        connection_->SendWaitReply(score::cpp::span<const std::uint8_t>{request_buffer.data(), request_size},
                                   score::cpp::span<std::uint8_t>{reply_buffer.data(), reply_buffer.size()});
    if (!reply_result.has_value())
    {
        return std::nullopt;
    }

    return DeserializeResponse(*reply_result);
}

auto ServiceDiscoveryMessagePassingClient::SendRequestWithCallback(
    ProtocolRequest request,
    score::message_passing::IClientConnection::ReplyCallback callback) noexcept -> bool
{
    if (!connection_)
    {
        return false;
    }

    std::array<std::uint8_t, kMaxRequestPayloadSize> request_buffer{};
    std::size_t request_size{0U};
    if (!SerializeRequest(
            request, score::cpp::span<std::uint8_t>{request_buffer.data(), request_buffer.size()}, request_size))
    {
        return false;
    }

    const auto send_result = connection_->SendWithCallback(
        score::cpp::span<const std::uint8_t>{request_buffer.data(), request_size}, std::move(callback));
    return send_result.has_value();
}

auto ServiceDiscoveryMessagePassingClient::StartFindService(const ServiceKey& key, UpdateCallback callback) noexcept
    -> std::optional<ProtocolResponse>
{
    ProtocolRequest request{};
    request.operation = ProtocolOperation::kStartFindService;
    request.registration.key = key;
    const auto response = Request(request);
    if (!response.has_value() || response->status_code != 0U || response->search_handle == 0U)
    {
        return std::nullopt;
    }

    std::scoped_lock lock{subscription_mutex_};
    for (auto& subscription : subscriptions_)
    {
        if (subscription.active && subscription.search_handle == response->search_handle)
        {
            subscription.key = key;
            subscription.callback = std::move(callback);
            return response;
        }
    }

    for (auto& subscription : subscriptions_)
    {
        if (!subscription.active)
        {
            subscription.active = true;
            subscription.search_handle = response->search_handle;
            subscription.key = key;
            subscription.callback = std::move(callback);
            return response;
        }
    }

    return std::nullopt;
}

auto ServiceDiscoveryMessagePassingClient::StopFindService(const std::uint64_t search_handle) noexcept -> bool
{
    ProtocolRequest request{};
    request.operation = ProtocolOperation::kStopFindService;
    request.search_handle = search_handle;

    const auto response = Request(request);
    if (response.has_value())
    {
        if (response->status_code != 0U)
        {
            return false;
        }
        DeactivateSubscription(search_handle);
        return true;
    }

    return SendRequestWithCallback(request, [this, search_handle](auto message_expected) noexcept {
        if (!message_expected.has_value())
        {
            return;
        }

        const auto stop_response = DeserializeResponse(message_expected.value());
        if (!stop_response.has_value() || stop_response->status_code != 0U)
        {
            return;
        }

        DeactivateSubscription(search_handle);
    });
}

auto ServiceDiscoveryMessagePassingClient::AcquireCreationLock(const ServiceKey& key) noexcept -> bool
{
    ProtocolRequest request{};
    request.operation = ProtocolOperation::kAcquireCreationLock;
    request.registration.key = key;
    const auto response = Request(request);
    return response.has_value() && response->status_code == 0U;
}

auto ServiceDiscoveryMessagePassingClient::ReleaseCreationLock(const ServiceKey& key) noexcept -> bool
{
    ProtocolRequest request{};
    request.operation = ProtocolOperation::kReleaseCreationLock;
    request.registration.key = key;
    const auto response = Request(request);
    return response.has_value() && response->status_code == 0U;
}

auto ServiceDiscoveryMessagePassingClient::AcquireUsageSharedLock(const ServiceKey& key) noexcept -> bool
{
    ProtocolRequest request{};
    request.operation = ProtocolOperation::kAcquireUsageSharedLock;
    request.registration.key = key;
    const auto response = Request(request);
    return response.has_value() && response->status_code == 0U;
}

auto ServiceDiscoveryMessagePassingClient::AcquireUsageExclusiveLock(const ServiceKey& key) noexcept -> bool
{
    ProtocolRequest request{};
    request.operation = ProtocolOperation::kAcquireUsageExclusiveLock;
    request.registration.key = key;
    const auto response = Request(request);
    return response.has_value() && response->status_code == 0U;
}

auto ServiceDiscoveryMessagePassingClient::ReleaseUsageLock(const ServiceKey& key) noexcept -> bool
{
    ProtocolRequest request{};
    request.operation = ProtocolOperation::kReleaseUsageLock;
    request.registration.key = key;
    const auto response = Request(request);
    return response.has_value() && response->status_code == 0U;
}

}  // namespace score::mw::com::service_discovery
