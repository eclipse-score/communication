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
#ifndef SCORE_MW_COM_SERVICE_DISCOVERY_SERVICE_DISCOVERY_PROTOCOL_H
#define SCORE_MW_COM_SERVICE_DISCOVERY_SERVICE_DISCOVERY_PROTOCOL_H

#include "score/mw/com/service_discovery/service_discovery_types.h"

#include <score/span.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace score::mw::com::service_discovery
{

enum class ProtocolOperation : std::uint8_t
{
    kRegister = 1U,
    kUnregister = 2U,
    kResolve = 3U,
    kStartFindService = 4U,
    kStopFindService = 5U,
    kAcquireCreationLock = 6U,
    kReleaseCreationLock = 7U,
    kAcquireUsageSharedLock = 8U,
    kAcquireUsageExclusiveLock = 9U,
    kReleaseUsageLock = 10U,
};

struct ProtocolRequest
{
    ProtocolOperation operation{ProtocolOperation::kResolve};
    Registration registration{};
    std::uint64_t search_handle{0U};
};

constexpr std::size_t kMaxEndpointAddressSize{256U};
constexpr std::size_t kMaxRegistrationsPerResponse{kMaxRegistrationsPerService};
constexpr std::size_t kRegistrationFixedSerializedSize{8U + 4U + 1U + 1U + 4U + 4U + 4U};
constexpr std::size_t kMaxRequestPayloadSize{1U + 8U + kRegistrationFixedSerializedSize + kMaxEndpointAddressSize};
constexpr std::size_t kMaxResponsePayloadSize{
    1U + 8U + 4U + (kMaxRegistrationsPerResponse * (kRegistrationFixedSerializedSize + kMaxEndpointAddressSize))};
constexpr std::size_t kMaxNotificationPayloadSize{8U + 8U + 4U + kMaxResponsePayloadSize};

struct ProtocolResponse
{
    std::uint8_t status_code{0U};
    std::uint64_t search_handle{0U};
    RegistrationSet registrations{};

    auto PushRegistration(const Registration& registration) noexcept -> bool
    {
        return registrations.Push(registration);
    }
};

struct ProtocolNotification
{
    std::uint64_t search_handle{0U};
    ServiceKey key{};
    ProtocolResponse response{};
};

auto SerializeRequest(const ProtocolRequest& request,
                      score::cpp::span<std::uint8_t> output,
                      std::size_t& serialized_size) noexcept -> bool;
[[nodiscard]] auto DeserializeRequest(score::cpp::span<const std::uint8_t> payload) noexcept
    -> std::optional<ProtocolRequest>;

auto SerializeResponse(const ProtocolResponse& response,
                       score::cpp::span<std::uint8_t> output,
                       std::size_t& serialized_size) noexcept -> bool;
[[nodiscard]] auto DeserializeResponse(score::cpp::span<const std::uint8_t> payload) noexcept
    -> std::optional<ProtocolResponse>;

auto SerializeNotification(const ProtocolNotification& notification,
                           score::cpp::span<std::uint8_t> output,
                           std::size_t& serialized_size) noexcept -> bool;
[[nodiscard]] auto DeserializeNotification(score::cpp::span<const std::uint8_t> payload) noexcept
    -> std::optional<ProtocolNotification>;

[[nodiscard]] auto SerializeRequest(const ProtocolRequest& request) -> std::vector<std::uint8_t>;
[[nodiscard]] auto DeserializeRequest(const std::vector<std::uint8_t>& payload) -> std::optional<ProtocolRequest>;

[[nodiscard]] auto SerializeResponse(const ProtocolResponse& response) -> std::vector<std::uint8_t>;
[[nodiscard]] auto DeserializeResponse(const std::vector<std::uint8_t>& payload) -> std::optional<ProtocolResponse>;

[[nodiscard]] auto SerializeNotification(const ProtocolNotification& notification) -> std::vector<std::uint8_t>;
[[nodiscard]] auto DeserializeNotification(const std::vector<std::uint8_t>& payload)
    -> std::optional<ProtocolNotification>;

}  // namespace score::mw::com::service_discovery

#endif  // SCORE_MW_COM_SERVICE_DISCOVERY_SERVICE_DISCOVERY_PROTOCOL_H
