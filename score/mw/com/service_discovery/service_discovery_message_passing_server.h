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
#ifndef SCORE_MW_COM_SERVICE_DISCOVERY_SERVICE_DISCOVERY_MESSAGE_PASSING_SERVER_H
#define SCORE_MW_COM_SERVICE_DISCOVERY_SERVICE_DISCOVERY_MESSAGE_PASSING_SERVER_H

#include "score/mw/com/service_discovery/service_discovery_daemon.h"

#include "score/message_passing/server_factory.h"
#include "score/message_passing/service_protocol_config.h"

#include <score/memory.hpp>

#include <array>
#include <cstdint>
#include <string>

namespace score::mw::com::service_discovery
{

class ServiceDiscoveryMessagePassingServer final
{
  public:
    struct Config
    {
        std::string service_identifier{"mw_com_service_discovery"};
        std::uint32_t max_send_size{4096U};
        std::uint32_t max_reply_size{4096U};
        std::uint32_t max_queued_sends{64U};
        std::uint32_t pre_alloc_connections{8U};
    };

    explicit ServiceDiscoveryMessagePassingServer(ServiceDiscoveryDaemon& daemon) noexcept;
    explicit ServiceDiscoveryMessagePassingServer(ServiceDiscoveryDaemon& daemon, Config config) noexcept;

    auto Start() noexcept -> bool;
    auto Stop() noexcept -> void;

  private:
    struct SubscriberEntry
    {
        bool active{false};
        std::uint64_t session_id{0U};
        std::uint64_t search_handle{0U};
        ServiceKey key{};
        score::message_passing::IServerConnection* connection{nullptr};
    };

    [[nodiscard]] static auto GetProviderContext(score::message_passing::IServerConnection& connection) noexcept
        -> ProviderContext;
    auto HandleStartFindService(score::message_passing::IServerConnection& connection,
                                const ProtocolRequest& request) noexcept
        -> score::cpp::expected_blank<score::os::Error>;
    auto HandleStopFindService(score::message_passing::IServerConnection& connection,
                               const ProtocolRequest& request) noexcept -> score::cpp::expected_blank<score::os::Error>;
    auto NotifySubscribers(const ServiceKey& key) noexcept -> void;
    auto RemoveSubscriberEntries(std::uint64_t session_id) noexcept -> void;

    ServiceDiscoveryDaemon& daemon_;
    Config config_;
    std::uint64_t next_session_id_{1U};
    std::uint64_t next_search_handle_{1U};
    std::array<SubscriberEntry, kMaxRegistrationsPerService> subscribers_{};
    score::message_passing::ServerFactory server_factory_{};
    score::cpp::pmr::unique_ptr<score::message_passing::IServer> server_{};
};

}  // namespace score::mw::com::service_discovery

#endif  // SCORE_MW_COM_SERVICE_DISCOVERY_SERVICE_DISCOVERY_MESSAGE_PASSING_SERVER_H
