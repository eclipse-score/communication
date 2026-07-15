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
#ifndef SCORE_MW_COM_SERVICE_DISCOVERY_TYPES_H
#define SCORE_MW_COM_SERVICE_DISCOVERY_TYPES_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace score::mw::com::service_discovery
{

constexpr std::uint32_t kAnyInstanceId{0xFFFFFFFFU};

enum class IntegrityLevel : std::uint8_t
{
    kAsilQm,
    kAsilB,
};

struct Endpoint
{
    std::string address{};
};

struct ServiceKey
{
    std::uint64_t service_id{0U};
    std::uint32_t instance_id{0U};

    friend auto operator==(const ServiceKey& lhs, const ServiceKey& rhs) noexcept -> bool
    {
        return lhs.service_id == rhs.service_id && lhs.instance_id == rhs.instance_id;
    }
};

[[nodiscard]] inline auto IsAnyInstanceKey(const ServiceKey& key) noexcept -> bool
{
    return key.instance_id == kAnyInstanceId;
}

struct Registration
{
    ServiceKey key{};
    Endpoint endpoint{};
    IntegrityLevel offered_integrity{IntegrityLevel::kAsilQm};
    IntegrityLevel provider_integrity{IntegrityLevel::kAsilQm};
    std::uint32_t provider_uid{0U};
    std::uint32_t provider_pid{0U};
    std::uint64_t provider_session_id{0U};
};

struct ProviderContext
{
    std::uint32_t provider_uid{0U};
    std::uint32_t provider_pid{0U};
    std::uint64_t provider_session_id{0U};
};

constexpr std::size_t kMaxRegistrationsPerService{32U};

struct RegistrationSet
{
    std::array<Registration, kMaxRegistrationsPerService> values{};
    std::size_t size{0U};

    auto Push(const Registration& registration) noexcept -> bool
    {
        if (size >= values.size())
        {
            return false;
        }

        values[size] = registration;
        ++size;
        return true;
    }

    [[nodiscard]] auto Empty() const noexcept -> bool
    {
        return size == 0U;
    }
};

struct ServiceKeySet
{
    std::array<ServiceKey, kMaxRegistrationsPerService> values{};
    std::size_t size{0U};

    auto Push(const ServiceKey& key) noexcept -> bool
    {
        for (std::size_t index = 0U; index < size; ++index)
        {
            if (values[index] == key)
            {
                return true;
            }
        }

        if (size >= values.size())
        {
            return false;
        }

        values[size] = key;
        ++size;
        return true;
    }
};

}  // namespace score::mw::com::service_discovery

#endif  // SCORE_MW_COM_SERVICE_DISCOVERY_TYPES_H
