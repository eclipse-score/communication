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
#ifndef SCORE_MW_COM_SERVICE_DISCOVERY_SERVICE_DISCOVERY_MESSAGE_PASSING_CLIENT_H
#define SCORE_MW_COM_SERVICE_DISCOVERY_SERVICE_DISCOVERY_MESSAGE_PASSING_CLIENT_H

#include "score/mw/com/service_discovery/service_discovery_protocol.h"

#include "score/message_passing/client_factory.h"
#include "score/message_passing/service_protocol_config.h"

#include <score/callback.hpp>
#include <score/memory.hpp>

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

namespace score::mw::com::service_discovery
{

class ServiceDiscoveryMessagePassingClient final
{
  public:
    using UpdateCallback = score::cpp::callback<void(const ProtocolNotification&) /* noexcept */>;

    struct Config
    {
        std::string service_identifier{"mw_com_service_discovery"};
        std::uint32_t max_send_size{4096U};
        std::uint32_t max_reply_size{4096U};
        std::uint32_t max_async_replies{1U};
        std::uint32_t max_queued_sends{16U};
    };

    explicit ServiceDiscoveryMessagePassingClient() noexcept;
    explicit ServiceDiscoveryMessagePassingClient(Config config) noexcept;
    ~ServiceDiscoveryMessagePassingClient() noexcept;

    auto Start(std::chrono::milliseconds timeout) noexcept -> bool;
    auto Stop() noexcept -> void;

    [[nodiscard]] auto Request(const ProtocolRequest& request) noexcept -> std::optional<ProtocolResponse>;
    [[nodiscard]] auto StartFindService(const ServiceKey& key, UpdateCallback callback) noexcept
        -> std::optional<ProtocolResponse>;
    auto StopFindService(std::uint64_t search_handle) noexcept -> bool;
    auto AcquireCreationLock(const ServiceKey& key) noexcept -> bool;
    auto ReleaseCreationLock(const ServiceKey& key) noexcept -> bool;
    auto AcquireUsageSharedLock(const ServiceKey& key) noexcept -> bool;
    auto AcquireUsageExclusiveLock(const ServiceKey& key) noexcept -> bool;
    auto ReleaseUsageLock(const ServiceKey& key) noexcept -> bool;

  private:
    struct SubscriptionEntry
    {
        bool active{false};
        std::uint64_t search_handle{0U};
        ServiceKey key{};
        UpdateCallback callback{};
    };

    auto DispatchNotification(const ProtocolNotification& notification) noexcept -> void;
    auto DeactivateSubscription(std::uint64_t search_handle) noexcept -> void;
    auto SendRequestWithCallback(score::mw::com::service_discovery::ProtocolRequest request,
                                 score::message_passing::IClientConnection::ReplyCallback callback) noexcept -> bool;

    Config config_;
    score::message_passing::ClientFactory client_factory_{};
    score::cpp::pmr::unique_ptr<score::message_passing::IClientConnection> connection_{};

    std::mutex state_mutex_{};
    std::condition_variable state_cv_{};
    std::mutex subscription_mutex_{};
    std::array<SubscriptionEntry, kMaxRegistrationsPerService> subscriptions_{};
};

}  // namespace score::mw::com::service_discovery

#endif  // SCORE_MW_COM_SERVICE_DISCOVERY_SERVICE_DISCOVERY_MESSAGE_PASSING_CLIENT_H
