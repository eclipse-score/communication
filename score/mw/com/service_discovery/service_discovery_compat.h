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
#ifndef SCORE_MW_COM_SERVICE_DISCOVERY_SERVICE_DISCOVERY_COMPAT_H
#define SCORE_MW_COM_SERVICE_DISCOVERY_SERVICE_DISCOVERY_COMPAT_H

#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/enriched_instance_identifier.h"
#include "score/mw/com/impl/find_service_handle.h"
#include "score/mw/com/impl/find_service_handler.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/i_runtime.h"
#include "score/mw/com/impl/i_service_discovery.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/service_discovery/service_discovery_message_passing_client.h"
#include "score/mw/com/service_discovery/service_discovery_protocol.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace score::mw::com::impl
{

class ServiceDiscoveryCompat final : public IServiceDiscovery
{
  public:
    explicit ServiceDiscoveryCompat(IRuntime& runtime) noexcept;
    ServiceDiscoveryCompat(
        IRuntime& runtime,
        score::mw::com::service_discovery::ServiceDiscoveryMessagePassingClient::Config config) noexcept;
    ~ServiceDiscoveryCompat() noexcept override;

    [[nodiscard]] Result<void> OfferService(InstanceIdentifier) noexcept override;
    [[nodiscard]] Result<void> StopOfferService(InstanceIdentifier) noexcept override;
    [[nodiscard]] Result<void> StopOfferService(InstanceIdentifier, QualityTypeSelector quality_type) noexcept override;
    Result<FindServiceHandle> StartFindService(FindServiceHandler<HandleType>,
                                               const InstanceSpecifier) noexcept override;
    Result<FindServiceHandle> StartFindService(FindServiceHandler<HandleType>, InstanceIdentifier) noexcept override;
    Result<FindServiceHandle> StartFindService(FindServiceHandler<HandleType>,
                                               const EnrichedInstanceIdentifier) noexcept override;
    [[nodiscard]] Result<void> StopFindService(const FindServiceHandle) noexcept override;
    [[nodiscard]] Result<ServiceHandleContainer<HandleType>> FindService(
        InstanceIdentifier instance_identifier) noexcept override;
    [[nodiscard]] Result<ServiceHandleContainer<HandleType>> FindService(
        InstanceSpecifier instance_specifier) noexcept override;

  private:
    struct ServiceKeyHash
    {
        auto operator()(const score::mw::com::service_discovery::ServiceKey& key) const noexcept -> std::size_t;
    };

    struct TrackedIdentifier
    {
        InstanceIdentifier identifier;
        score::mw::com::service_discovery::ServiceKey key;
    };

    struct SearchEntry
    {
        std::shared_ptr<FindServiceHandler<HandleType>> handler;
        std::vector<TrackedIdentifier> tracked_identifiers{};
    };

    auto EnsureClientStarted() noexcept -> bool;
    auto StartFindServiceImpl(FindServiceHandle handle,
                              FindServiceHandler<HandleType> handler,
                              const std::vector<TrackedIdentifier>& tracked_identifiers) noexcept
        -> Result<FindServiceHandle>;
    auto ToTrackedIdentifier(const EnrichedInstanceIdentifier& enriched_instance_identifier) const noexcept
        -> std::optional<TrackedIdentifier>;
    auto ToRegistration(const EnrichedInstanceIdentifier& enriched_instance_identifier) const noexcept
        -> std::optional<score::mw::com::service_discovery::Registration>;
    auto BuildHandleContainer(const SearchEntry& search_entry) const noexcept -> ServiceHandleContainer<HandleType>;
    auto FindSearch(const FindServiceHandle& handle) noexcept -> SearchEntry*;
    auto OnNotification(const score::mw::com::service_discovery::ProtocolNotification& notification) noexcept -> void;
    static auto ToIntegrityLevel(QualityType quality_type) noexcept
        -> score::mw::com::service_discovery::IntegrityLevel;

    IRuntime& runtime_;
    score::mw::com::service_discovery::ServiceDiscoveryMessagePassingClient client_;
    bool client_started_{false};
    std::atomic<std::size_t> next_find_service_uid_{0U};
    std::recursive_mutex mutex_{};
    std::unordered_map<FindServiceHandle, SearchEntry> searches_{};
    std::unordered_map<score::mw::com::service_discovery::ServiceKey,
                       score::mw::com::service_discovery::RegistrationSet,
                       ServiceKeyHash>
        snapshots_{};
    std::unordered_map<score::mw::com::service_discovery::ServiceKey, std::uint64_t, ServiceKeyHash>
        key_to_daemon_find_handle_{};
    std::unordered_map<score::mw::com::service_discovery::ServiceKey, std::vector<FindServiceHandle>, ServiceKeyHash>
        key_to_handles_{};
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_SERVICE_DISCOVERY_SERVICE_DISCOVERY_COMPAT_H
