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
#include "score/mw/com/service_discovery/service_discovery_protocol.h"

#include <cstddef>
#include <vector>

namespace score::mw::com::service_discovery
{

namespace
{

auto IsValidOperation(const ProtocolOperation operation) noexcept -> bool
{
    return operation == ProtocolOperation::kRegister || operation == ProtocolOperation::kUnregister ||
           operation == ProtocolOperation::kResolve || operation == ProtocolOperation::kStartFindService ||
           operation == ProtocolOperation::kStopFindService || operation == ProtocolOperation::kAcquireCreationLock ||
           operation == ProtocolOperation::kReleaseCreationLock ||
           operation == ProtocolOperation::kAcquireUsageSharedLock ||
           operation == ProtocolOperation::kAcquireUsageExclusiveLock ||
           operation == ProtocolOperation::kReleaseUsageLock;
}

auto IsValidIntegrity(const IntegrityLevel integrity_level) noexcept -> bool
{
    return integrity_level == IntegrityLevel::kAsilQm || integrity_level == IntegrityLevel::kAsilB;
}

auto AppendByte(score::cpp::span<std::uint8_t> output, std::size_t& cursor, const std::uint8_t value) noexcept -> bool
{
    if (cursor >= output.size())
    {
        return false;
    }

    output[cursor] = value;
    ++cursor;
    return true;
}

auto AppendU32(score::cpp::span<std::uint8_t> output, std::size_t& cursor, const std::uint32_t value) noexcept -> bool
{
    return AppendByte(output, cursor, static_cast<std::uint8_t>(value & 0xFFU)) &&
           AppendByte(output, cursor, static_cast<std::uint8_t>((value >> 8U) & 0xFFU)) &&
           AppendByte(output, cursor, static_cast<std::uint8_t>((value >> 16U) & 0xFFU)) &&
           AppendByte(output, cursor, static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
}

auto AppendU64(score::cpp::span<std::uint8_t> output, std::size_t& cursor, const std::uint64_t value) noexcept -> bool
{
    for (std::uint32_t byte_index = 0U; byte_index < 8U; ++byte_index)
    {
        if (!AppendByte(output, cursor, static_cast<std::uint8_t>((value >> (byte_index * 8U)) & 0xFFU)))
        {
            return false;
        }
    }

    return true;
}

auto ReadU32(score::cpp::span<const std::uint8_t> input, std::size_t& cursor, std::uint32_t& value) noexcept -> bool
{
    if (cursor + 4U > input.size())
    {
        return false;
    }

    value = static_cast<std::uint32_t>(input[cursor]) | (static_cast<std::uint32_t>(input[cursor + 1U]) << 8U) |
            (static_cast<std::uint32_t>(input[cursor + 2U]) << 16U) |
            (static_cast<std::uint32_t>(input[cursor + 3U]) << 24U);
    cursor += 4U;
    return true;
}

auto ReadU64(score::cpp::span<const std::uint8_t> input, std::size_t& cursor, std::uint64_t& value) noexcept -> bool
{
    if (cursor + 8U > input.size())
    {
        return false;
    }

    value = 0U;
    for (std::uint32_t byte_index = 0U; byte_index < 8U; ++byte_index)
    {
        value |= static_cast<std::uint64_t>(input[cursor + byte_index]) << (byte_index * 8U);
    }

    cursor += 8U;
    return true;
}

auto AppendRegistration(score::cpp::span<std::uint8_t> output,
                        std::size_t& cursor,
                        const Registration& registration) noexcept -> bool
{
    if (registration.endpoint.address.size() > kMaxEndpointAddressSize)
    {
        return false;
    }

    if (!AppendU64(output, cursor, registration.key.service_id) ||
        !AppendU32(output, cursor, registration.key.instance_id) ||
        !AppendByte(output, cursor, static_cast<std::uint8_t>(registration.offered_integrity)) ||
        !AppendByte(output, cursor, static_cast<std::uint8_t>(registration.provider_integrity)) ||
        !AppendU32(output, cursor, registration.provider_uid) || !AppendU32(output, cursor, registration.provider_pid))
    {
        return false;
    }

    const auto address_size = static_cast<std::uint32_t>(registration.endpoint.address.size());
    if (!AppendU32(output, cursor, address_size) || (cursor + address_size > output.size()))
    {
        return false;
    }

    for (const char value : registration.endpoint.address)
    {
        output[cursor] = static_cast<std::uint8_t>(value);
        ++cursor;
    }

    return true;
}

auto ReadRegistration(score::cpp::span<const std::uint8_t> input,
                      std::size_t& cursor,
                      Registration& registration) noexcept -> bool
{
    if (!ReadU64(input, cursor, registration.key.service_id))
    {
        return false;
    }

    if (!ReadU32(input, cursor, registration.key.instance_id))
    {
        return false;
    }

    if (cursor + 2U > input.size())
    {
        return false;
    }

    registration.offered_integrity = static_cast<IntegrityLevel>(input[cursor]);
    registration.provider_integrity = static_cast<IntegrityLevel>(input[cursor + 1U]);
    if (!IsValidIntegrity(registration.offered_integrity) || !IsValidIntegrity(registration.provider_integrity))
    {
        return false;
    }
    cursor += 2U;

    if (!ReadU32(input, cursor, registration.provider_uid) || !ReadU32(input, cursor, registration.provider_pid))
    {
        return false;
    }

    std::uint32_t address_size{0U};
    if (!ReadU32(input, cursor, address_size))
    {
        return false;
    }

    if (address_size > kMaxEndpointAddressSize)
    {
        return false;
    }

    if (cursor + address_size > input.size())
    {
        return false;
    }

    registration.endpoint.address.assign(reinterpret_cast<const char*>(&input[cursor]), address_size);
    cursor += address_size;

    return true;
}

}  // namespace

auto SerializeRequest(const ProtocolRequest& request,
                      score::cpp::span<std::uint8_t> output,
                      std::size_t& serialized_size) noexcept -> bool
{
    serialized_size = 0U;
    if (!IsValidOperation(request.operation))
    {
        return false;
    }

    std::size_t cursor{0U};
    if (!AppendByte(output, cursor, static_cast<std::uint8_t>(request.operation)) ||
        !AppendU64(output, cursor, request.search_handle) || !AppendRegistration(output, cursor, request.registration))
    {
        return false;
    }

    serialized_size = cursor;
    return true;
}

auto SerializeRequest(const ProtocolRequest& request) -> std::vector<std::uint8_t>
{
    std::vector<std::uint8_t> output(kMaxRequestPayloadSize);
    std::size_t serialized_size{0U};
    if (!SerializeRequest(request, score::cpp::span<std::uint8_t>{output.data(), output.size()}, serialized_size))
    {
        return {};
    }

    output.resize(serialized_size);
    return output;
}

auto DeserializeRequest(score::cpp::span<const std::uint8_t> payload) noexcept -> std::optional<ProtocolRequest>
{
    if (payload.empty() || payload.size() > kMaxRequestPayloadSize)
    {
        return std::nullopt;
    }

    std::size_t cursor{0U};
    ProtocolRequest request{};

    request.operation = static_cast<ProtocolOperation>(payload[cursor]);
    if (!IsValidOperation(request.operation))
    {
        return std::nullopt;
    }
    ++cursor;

    if (!ReadU64(payload, cursor, request.search_handle))
    {
        return std::nullopt;
    }

    if (!ReadRegistration(payload, cursor, request.registration))
    {
        return std::nullopt;
    }

    if (cursor != payload.size())
    {
        return std::nullopt;
    }

    return request;
}

auto DeserializeRequest(const std::vector<std::uint8_t>& payload) -> std::optional<ProtocolRequest>
{
    return DeserializeRequest(score::cpp::span<const std::uint8_t>{payload.data(), payload.size()});
}

auto SerializeResponse(const ProtocolResponse& response,
                       score::cpp::span<std::uint8_t> output,
                       std::size_t& serialized_size) noexcept -> bool
{
    serialized_size = 0U;
    if (response.registrations.size > kMaxRegistrationsPerResponse)
    {
        return false;
    }

    std::size_t cursor{0U};
    if (!AppendByte(output, cursor, response.status_code))
    {
        return false;
    }

    if (!AppendU64(output, cursor, response.search_handle))
    {
        return false;
    }

    const auto count = static_cast<std::uint32_t>(response.registrations.size);
    if (!AppendU32(output, cursor, count))
    {
        return false;
    }

    for (std::size_t registration_index = 0U; registration_index < response.registrations.size; ++registration_index)
    {
        const auto& registration = response.registrations.values[registration_index];
        if (!AppendRegistration(output, cursor, registration))
        {
            return false;
        }
    }

    serialized_size = cursor;
    return true;
}

auto SerializeResponse(const ProtocolResponse& response) -> std::vector<std::uint8_t>
{
    std::vector<std::uint8_t> output(kMaxResponsePayloadSize);
    std::size_t serialized_size{0U};
    if (!SerializeResponse(response, score::cpp::span<std::uint8_t>{output.data(), output.size()}, serialized_size))
    {
        return {};
    }

    output.resize(serialized_size);
    return output;
}

auto DeserializeResponse(score::cpp::span<const std::uint8_t> payload) noexcept -> std::optional<ProtocolResponse>
{
    if (payload.size() < 13U || payload.size() > kMaxResponsePayloadSize)
    {
        return std::nullopt;
    }

    std::size_t cursor{0U};
    ProtocolResponse response{};

    response.status_code = payload[cursor];
    ++cursor;

    if (!ReadU64(payload, cursor, response.search_handle))
    {
        return std::nullopt;
    }

    std::uint32_t count{0U};
    if (!ReadU32(payload, cursor, count))
    {
        return std::nullopt;
    }

    if (count > kMaxRegistrationsPerResponse)
    {
        return std::nullopt;
    }

    for (std::uint32_t registration_index = 0U; registration_index < count; ++registration_index)
    {
        Registration registration{};
        if (!ReadRegistration(payload, cursor, registration))
        {
            return std::nullopt;
        }
        if (!response.PushRegistration(registration))
        {
            return std::nullopt;
        }
    }

    if (cursor != payload.size())
    {
        return std::nullopt;
    }

    return response;
}

auto DeserializeResponse(const std::vector<std::uint8_t>& payload) -> std::optional<ProtocolResponse>
{
    return DeserializeResponse(score::cpp::span<const std::uint8_t>{payload.data(), payload.size()});
}

auto SerializeNotification(const ProtocolNotification& notification,
                           score::cpp::span<std::uint8_t> output,
                           std::size_t& serialized_size) noexcept -> bool
{
    serialized_size = 0U;
    std::size_t cursor{0U};

    if (!AppendU64(output, cursor, notification.key.service_id) ||
        !AppendU64(output, cursor, notification.search_handle) ||
        !AppendU32(output, cursor, notification.key.instance_id))
    {
        return false;
    }

    std::size_t response_size{0U};
    if (!SerializeResponse(notification.response, output.subspan(cursor), response_size))
    {
        return false;
    }

    serialized_size = cursor + response_size;
    return true;
}

auto SerializeNotification(const ProtocolNotification& notification) -> std::vector<std::uint8_t>
{
    std::vector<std::uint8_t> output(kMaxNotificationPayloadSize);
    std::size_t serialized_size{0U};
    if (!SerializeNotification(
            notification, score::cpp::span<std::uint8_t>{output.data(), output.size()}, serialized_size))
    {
        return {};
    }

    output.resize(serialized_size);
    return output;
}

auto DeserializeNotification(score::cpp::span<const std::uint8_t> payload) noexcept
    -> std::optional<ProtocolNotification>
{
    if (payload.size() < 20U || payload.size() > kMaxNotificationPayloadSize)
    {
        return std::nullopt;
    }

    ProtocolNotification notification{};
    std::size_t cursor{0U};
    if (!ReadU64(payload, cursor, notification.key.service_id) ||
        !ReadU64(payload, cursor, notification.search_handle) ||
        !ReadU32(payload, cursor, notification.key.instance_id))
    {
        return std::nullopt;
    }

    const auto response = DeserializeResponse(payload.subspan(cursor));
    if (!response.has_value())
    {
        return std::nullopt;
    }

    notification.response = *response;
    return notification;
}

auto DeserializeNotification(const std::vector<std::uint8_t>& payload) -> std::optional<ProtocolNotification>
{
    return DeserializeNotification(score::cpp::span<const std::uint8_t>{payload.data(), payload.size()});
}

}  // namespace score::mw::com::service_discovery
