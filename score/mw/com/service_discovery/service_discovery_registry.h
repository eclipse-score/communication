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
#ifndef SCORE_MW_COM_SERVICE_DISCOVERY_SERVICE_DISCOVERY_REGISTRY_H
#define SCORE_MW_COM_SERVICE_DISCOVERY_SERVICE_DISCOVERY_REGISTRY_H

#include "score/mw/com/service_discovery/i_service_discovery_registry.h"

#include <array>
#include <unordered_map>

namespace score::mw::com::service_discovery
{

class ServiceDiscoveryRegistry final : public IServiceDiscoveryRegistry
{
  public:
    auto Register(Registration registration, ProviderContext provider_context) noexcept
        -> std::optional<RegisterError> override;

    auto Unregister(const ServiceKey& key, ProviderContext provider_context) noexcept
        -> std::optional<UnregisterError> override;

    auto AcquireCreationLock(const ServiceKey& key, ProviderContext provider_context) noexcept
        -> std::optional<ServiceInstanceLockError> override;

    auto ReleaseCreationLock(const ServiceKey& key, ProviderContext provider_context) noexcept
        -> std::optional<ServiceInstanceLockError> override;

    auto AcquireUsageSharedLock(const ServiceKey& key, ProviderContext provider_context) noexcept
        -> std::optional<ServiceInstanceLockError> override;

    auto AcquireUsageExclusiveLock(const ServiceKey& key, ProviderContext provider_context) noexcept
        -> std::optional<ServiceInstanceLockError> override;

    auto ReleaseUsageLock(const ServiceKey& key, ProviderContext provider_context) noexcept
        -> std::optional<ServiceInstanceLockError> override;

    auto RemoveBySession(std::uint64_t provider_session_id) noexcept -> ServiceKeySet override;

    [[nodiscard]] auto Resolve(const ServiceKey& key) const noexcept -> RegistrationSet override;

  private:
    struct ServiceKeyHash
    {
        auto operator()(const ServiceKey& key) const noexcept -> std::size_t;
    };

    struct RegistrationList
    {
        RegistrationSet value{};
    };

    struct UsageLockState
    {
        std::uint64_t exclusive_owner_session_id{0U};
        std::array<std::uint64_t, kMaxRegistrationsPerService> shared_owner_session_ids{};
        std::size_t shared_owner_count{0U};
    };

    [[nodiscard]] static auto IsIntegrityClaimValid(const Registration& registration) noexcept -> bool;
    [[nodiscard]] static auto ContainsSharedOwner(const UsageLockState& usage_lock_state,
                                                  std::uint64_t provider_session_id) noexcept -> bool;

    std::unordered_map<ServiceKey, RegistrationList, ServiceKeyHash> services_{};
    std::unordered_map<ServiceKey, std::uint64_t, ServiceKeyHash> creation_locks_{};
    std::unordered_map<ServiceKey, UsageLockState, ServiceKeyHash> usage_locks_{};
};

}  // namespace score::mw::com::service_discovery

#endif  // SCORE_MW_COM_SERVICE_DISCOVERY_SERVICE_DISCOVERY_REGISTRY_H
