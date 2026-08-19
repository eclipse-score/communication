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
#include "score/mw/com/service_discovery/service_discovery_registry.h"

#include <algorithm>
#include <cstddef>
#include <functional>

namespace score::mw::com::service_discovery
{

auto ServiceDiscoveryRegistry::Register(Registration registration, ProviderContext provider_context) noexcept
    -> std::optional<RegisterError>
{
    if (!IsIntegrityClaimValid(registration))
    {
        return RegisterError::kInvalidIntegrityClaim;
    }

    registration.provider_uid = provider_context.provider_uid;
    registration.provider_pid = provider_context.provider_pid;
    registration.provider_session_id = provider_context.provider_session_id;

    auto& registrations = services_[registration.key].value;
    for (std::size_t index = 0U; index < registrations.size; ++index)
    {
        const auto& existing_registration = registrations.values[index];
        if (existing_registration.provider_session_id == registration.provider_session_id)
        {
            return RegisterError::kAlreadyRegistered;
        }
    }

    if (!registrations.Push(registration))
    {
        return RegisterError::kCapacityExceeded;
    }

    return std::nullopt;
}

auto ServiceDiscoveryRegistry::Unregister(const ServiceKey& key, const ProviderContext provider_context) noexcept
    -> std::optional<UnregisterError>
{
    const auto map_entry = services_.find(key);
    if (map_entry == services_.cend())
    {
        return UnregisterError::kNotFound;
    }

    auto& registrations = map_entry->second.value;
    std::size_t remove_index = registrations.size;
    for (std::size_t index = 0U; index < registrations.size; ++index)
    {
        const auto& registration = registrations.values[index];
        if (registration.provider_session_id == provider_context.provider_session_id)
        {
            remove_index = index;
            break;
        }
    }

    if (remove_index == registrations.size)
    {
        return UnregisterError::kOwnershipMismatch;
    }

    for (std::size_t index = remove_index; index + 1U < registrations.size; ++index)
    {
        registrations.values[index] = registrations.values[index + 1U];
    }
    --registrations.size;

    if (registrations.Empty())
    {
        services_.erase(map_entry);
    }

    const auto creation_lock_entry = creation_locks_.find(key);
    if (creation_lock_entry != creation_locks_.cend() &&
        creation_lock_entry->second == provider_context.provider_session_id)
    {
        creation_locks_.erase(creation_lock_entry);
    }

    return std::nullopt;
}

auto ServiceDiscoveryRegistry::AcquireCreationLock(const ServiceKey& key,
                                                   const ProviderContext provider_context) noexcept
    -> std::optional<ServiceInstanceLockError>
{
    const auto lock_entry = creation_locks_.find(key);
    if (lock_entry == creation_locks_.cend())
    {
        creation_locks_.emplace(key, provider_context.provider_session_id);
        return std::nullopt;
    }

    if (lock_entry->second == provider_context.provider_session_id)
    {
        return std::nullopt;
    }

    return ServiceInstanceLockError::kConflict;
}

auto ServiceDiscoveryRegistry::ReleaseCreationLock(const ServiceKey& key,
                                                   const ProviderContext provider_context) noexcept
    -> std::optional<ServiceInstanceLockError>
{
    const auto lock_entry = creation_locks_.find(key);
    if (lock_entry == creation_locks_.cend())
    {
        return ServiceInstanceLockError::kNotFound;
    }

    if (lock_entry->second != provider_context.provider_session_id)
    {
        return ServiceInstanceLockError::kOwnershipMismatch;
    }

    creation_locks_.erase(lock_entry);
    return std::nullopt;
}

auto ServiceDiscoveryRegistry::AcquireUsageSharedLock(const ServiceKey& key,
                                                      const ProviderContext provider_context) noexcept
    -> std::optional<ServiceInstanceLockError>
{
    auto& usage_lock_state = usage_locks_[key];
    if (usage_lock_state.exclusive_owner_session_id != 0U)
    {
        return usage_lock_state.exclusive_owner_session_id == provider_context.provider_session_id
                   ? std::nullopt
                   : std::optional<ServiceInstanceLockError>{ServiceInstanceLockError::kConflict};
    }

    if (ContainsSharedOwner(usage_lock_state, provider_context.provider_session_id))
    {
        return std::nullopt;
    }

    if (usage_lock_state.shared_owner_count >= usage_lock_state.shared_owner_session_ids.size())
    {
        return ServiceInstanceLockError::kCapacityExceeded;
    }

    usage_lock_state.shared_owner_session_ids[usage_lock_state.shared_owner_count] =
        provider_context.provider_session_id;
    ++usage_lock_state.shared_owner_count;
    return std::nullopt;
}

auto ServiceDiscoveryRegistry::AcquireUsageExclusiveLock(const ServiceKey& key,
                                                         const ProviderContext provider_context) noexcept
    -> std::optional<ServiceInstanceLockError>
{
    auto& usage_lock_state = usage_locks_[key];
    if (usage_lock_state.exclusive_owner_session_id == provider_context.provider_session_id)
    {
        return std::nullopt;
    }

    if (usage_lock_state.exclusive_owner_session_id != 0U || usage_lock_state.shared_owner_count != 0U)
    {
        return ServiceInstanceLockError::kConflict;
    }

    usage_lock_state.exclusive_owner_session_id = provider_context.provider_session_id;
    return std::nullopt;
}

auto ServiceDiscoveryRegistry::ReleaseUsageLock(const ServiceKey& key, const ProviderContext provider_context) noexcept
    -> std::optional<ServiceInstanceLockError>
{
    const auto lock_entry = usage_locks_.find(key);
    if (lock_entry == usage_locks_.cend())
    {
        return ServiceInstanceLockError::kNotFound;
    }

    auto& usage_lock_state = lock_entry->second;
    if (usage_lock_state.exclusive_owner_session_id == provider_context.provider_session_id)
    {
        usage_lock_state.exclusive_owner_session_id = 0U;
    }
    else
    {
        std::size_t remove_index = usage_lock_state.shared_owner_count;
        for (std::size_t index = 0U; index < usage_lock_state.shared_owner_count; ++index)
        {
            if (usage_lock_state.shared_owner_session_ids[index] == provider_context.provider_session_id)
            {
                remove_index = index;
                break;
            }
        }

        if (remove_index == usage_lock_state.shared_owner_count)
        {
            return ServiceInstanceLockError::kOwnershipMismatch;
        }

        for (std::size_t index = remove_index; index + 1U < usage_lock_state.shared_owner_count; ++index)
        {
            usage_lock_state.shared_owner_session_ids[index] = usage_lock_state.shared_owner_session_ids[index + 1U];
        }
        --usage_lock_state.shared_owner_count;
    }

    if (usage_lock_state.exclusive_owner_session_id == 0U && usage_lock_state.shared_owner_count == 0U)
    {
        usage_locks_.erase(lock_entry);
    }

    return std::nullopt;
}

auto ServiceDiscoveryRegistry::RemoveBySession(const std::uint64_t provider_session_id) noexcept -> ServiceKeySet
{
    ServiceKeySet changed_keys{};

    for (auto map_entry = services_.begin(); map_entry != services_.end();)
    {
        auto& registrations = map_entry->second.value;
        const auto service_key = map_entry->first;
        bool removed_any{false};

        std::size_t write_index{0U};
        for (std::size_t read_index = 0U; read_index < registrations.size; ++read_index)
        {
            const auto& registration = registrations.values[read_index];
            if (registration.provider_session_id != provider_session_id)
            {
                if (write_index != read_index)
                {
                    registrations.values[write_index] = registrations.values[read_index];
                }
                ++write_index;
            }
            else
            {
                removed_any = true;
            }
        }

        registrations.size = write_index;
        if (removed_any)
        {
            changed_keys.Push(service_key);
        }
        if (registrations.Empty())
        {
            map_entry = services_.erase(map_entry);
        }
        else
        {
            ++map_entry;
        }
    }

    for (auto lock_entry = creation_locks_.begin(); lock_entry != creation_locks_.end();)
    {
        if (lock_entry->second == provider_session_id)
        {
            lock_entry = creation_locks_.erase(lock_entry);
        }
        else
        {
            ++lock_entry;
        }
    }

    for (auto lock_entry = usage_locks_.begin(); lock_entry != usage_locks_.end();)
    {
        auto& usage_lock_state = lock_entry->second;
        if (usage_lock_state.exclusive_owner_session_id == provider_session_id)
        {
            usage_lock_state.exclusive_owner_session_id = 0U;
        }

        std::size_t write_index{0U};
        for (std::size_t read_index = 0U; read_index < usage_lock_state.shared_owner_count; ++read_index)
        {
            if (usage_lock_state.shared_owner_session_ids[read_index] != provider_session_id)
            {
                usage_lock_state.shared_owner_session_ids[write_index] =
                    usage_lock_state.shared_owner_session_ids[read_index];
                ++write_index;
            }
        }
        usage_lock_state.shared_owner_count = write_index;

        if (usage_lock_state.exclusive_owner_session_id == 0U && usage_lock_state.shared_owner_count == 0U)
        {
            lock_entry = usage_locks_.erase(lock_entry);
        }
        else
        {
            ++lock_entry;
        }
    }

    return changed_keys;
}

auto ServiceDiscoveryRegistry::Resolve(const ServiceKey& key) const noexcept -> RegistrationSet
{
    if (IsAnyInstanceKey(key))
    {
        RegistrationSet result{};
        for (const auto& entry : services_)
        {
            const auto& registered_key = entry.first;
            if (registered_key.service_id != key.service_id)
            {
                continue;
            }

            const auto& registrations = entry.second.value;
            for (std::size_t index = 0U; index < registrations.size; ++index)
            {
                if (!result.Push(registrations.values[index]))
                {
                    return result;
                }
            }
        }

        return result;
    }

    const auto map_entry = services_.find(key);
    if (map_entry == services_.cend())
    {
        return RegistrationSet{};
    }

    return map_entry->second.value;
}

auto ServiceDiscoveryRegistry::ServiceKeyHash::operator()(const ServiceKey& key) const noexcept -> std::size_t
{
    const auto service_hash = std::hash<std::uint64_t>{}(key.service_id);
    const auto instance_hash = std::hash<std::uint32_t>{}(key.instance_id);

    return service_hash ^ (instance_hash << 1U);
}

auto ServiceDiscoveryRegistry::IsIntegrityClaimValid(const Registration& registration) noexcept -> bool
{
    // Prevent a QM process from claiming ASIL-B service quality.
    if (registration.offered_integrity == IntegrityLevel::kAsilB &&
        registration.provider_integrity != IntegrityLevel::kAsilB)
    {
        return false;
    }

    return true;
}

auto ServiceDiscoveryRegistry::ContainsSharedOwner(const UsageLockState& usage_lock_state,
                                                   const std::uint64_t provider_session_id) noexcept -> bool
{
    for (std::size_t index = 0U; index < usage_lock_state.shared_owner_count; ++index)
    {
        if (usage_lock_state.shared_owner_session_ids[index] == provider_session_id)
        {
            return true;
        }
    }

    return false;
}

}  // namespace score::mw::com::service_discovery
