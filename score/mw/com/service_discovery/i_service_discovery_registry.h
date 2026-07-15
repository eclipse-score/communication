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
#ifndef SCORE_MW_COM_SERVICE_DISCOVERY_I_SERVICE_DISCOVERY_REGISTRY_H
#define SCORE_MW_COM_SERVICE_DISCOVERY_I_SERVICE_DISCOVERY_REGISTRY_H

#include "score/mw/com/service_discovery/service_discovery_types.h"

#include <optional>

namespace score::mw::com::service_discovery
{

enum class RegisterError : std::uint8_t
{
    kAlreadyRegistered,
    kCapacityExceeded,
    kInvalidIntegrityClaim,
};

enum class UnregisterError : std::uint8_t
{
    kNotFound,
    kOwnershipMismatch,
};

enum class ServiceInstanceLockError : std::uint8_t
{
    kConflict,
    kNotFound,
    kOwnershipMismatch,
    kCapacityExceeded,
};

class IServiceDiscoveryRegistry
{
  public:
    virtual ~IServiceDiscoveryRegistry() noexcept = default;

    virtual auto Register(Registration registration, ProviderContext provider_context) noexcept
        -> std::optional<RegisterError> = 0;

    virtual auto Unregister(const ServiceKey& key, ProviderContext provider_context) noexcept
        -> std::optional<UnregisterError> = 0;

    virtual auto AcquireCreationLock(const ServiceKey& key, ProviderContext provider_context) noexcept
        -> std::optional<ServiceInstanceLockError> = 0;

    virtual auto ReleaseCreationLock(const ServiceKey& key, ProviderContext provider_context) noexcept
        -> std::optional<ServiceInstanceLockError> = 0;

    virtual auto AcquireUsageSharedLock(const ServiceKey& key, ProviderContext provider_context) noexcept
        -> std::optional<ServiceInstanceLockError> = 0;

    virtual auto AcquireUsageExclusiveLock(const ServiceKey& key, ProviderContext provider_context) noexcept
        -> std::optional<ServiceInstanceLockError> = 0;

    virtual auto ReleaseUsageLock(const ServiceKey& key, ProviderContext provider_context) noexcept
        -> std::optional<ServiceInstanceLockError> = 0;

    virtual auto RemoveBySession(std::uint64_t provider_session_id) noexcept -> ServiceKeySet = 0;

    [[nodiscard]] virtual auto Resolve(const ServiceKey& key) const noexcept -> RegistrationSet = 0;
};

}  // namespace score::mw::com::service_discovery

#endif  // SCORE_MW_COM_SERVICE_DISCOVERY_I_SERVICE_DISCOVERY_REGISTRY_H
