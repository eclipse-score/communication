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
#include "score/mw/com/service_discovery/service_discovery_daemon.h"

namespace score::mw::com::service_discovery
{

namespace
{

constexpr std::uint8_t kStatusOk{0U};
constexpr std::uint8_t kStatusMalformedRequest{1U};
constexpr std::uint8_t kStatusRegisterFailed{2U};
constexpr std::uint8_t kStatusUnregisterFailed{3U};
constexpr std::uint8_t kStatusResolveResultTooLarge{4U};
constexpr std::uint8_t kStatusLockFailed{5U};

}  // namespace

ServiceDiscoveryDaemon::ServiceDiscoveryDaemon(IServiceDiscoveryRegistry& registry) noexcept : registry_{registry} {}

auto ServiceDiscoveryDaemon::OnDisconnected(const ProviderContext provider_context) noexcept -> ServiceKeySet
{
    return registry_.RemoveBySession(provider_context.provider_session_id);
}

auto ServiceDiscoveryDaemon::Resolve(const ServiceKey& key) const noexcept -> RegistrationSet
{
    return registry_.Resolve(key);
}

auto ServiceDiscoveryDaemon::HandlePayload(score::cpp::span<const std::uint8_t> request_payload,
                                           score::cpp::span<std::uint8_t> response_payload,
                                           const ProviderContext provider_context,
                                           std::size_t& serialized_response_size) noexcept -> bool
{
    const auto request = DeserializeRequest(request_payload);
    if (!request.has_value())
    {
        ProtocolResponse response{};
        response.status_code = kStatusMalformedRequest;
        return SerializeResponse(response, response_payload, serialized_response_size);
    }

    switch (request->operation)
    {
        case ProtocolOperation::kRegister:
        {
            const auto result = registry_.Register(request->registration, provider_context);
            const auto status = result.has_value() ? kStatusRegisterFailed : kStatusOk;
            ProtocolResponse response{};
            response.status_code = status;
            return SerializeResponse(response, response_payload, serialized_response_size);
        }
        case ProtocolOperation::kUnregister:
        {
            const auto result = registry_.Unregister(request->registration.key, provider_context);
            const auto status = result.has_value() ? kStatusUnregisterFailed : kStatusOk;
            ProtocolResponse response{};
            response.status_code = status;
            return SerializeResponse(response, response_payload, serialized_response_size);
        }
        case ProtocolOperation::kResolve:
        {
            const auto registrations = registry_.Resolve(request->registration.key);

            ProtocolResponse response{};
            response.status_code = kStatusOk;
            for (std::size_t index = 0U; index < registrations.size; ++index)
            {
                const auto& registration = registrations.values[index];
                if (!response.PushRegistration(registration))
                {
                    response.status_code = kStatusResolveResultTooLarge;
                    response.registrations.size = 0U;
                    break;
                }
            }

            return SerializeResponse(response, response_payload, serialized_response_size);
        }
        case ProtocolOperation::kAcquireCreationLock:
        {
            const auto result = registry_.AcquireCreationLock(request->registration.key, provider_context);
            ProtocolResponse response{};
            response.status_code = result.has_value() ? kStatusLockFailed : kStatusOk;
            return SerializeResponse(response, response_payload, serialized_response_size);
        }
        case ProtocolOperation::kReleaseCreationLock:
        {
            const auto result = registry_.ReleaseCreationLock(request->registration.key, provider_context);
            ProtocolResponse response{};
            response.status_code = result.has_value() ? kStatusLockFailed : kStatusOk;
            return SerializeResponse(response, response_payload, serialized_response_size);
        }
        case ProtocolOperation::kAcquireUsageSharedLock:
        {
            const auto result = registry_.AcquireUsageSharedLock(request->registration.key, provider_context);
            ProtocolResponse response{};
            response.status_code = result.has_value() ? kStatusLockFailed : kStatusOk;
            return SerializeResponse(response, response_payload, serialized_response_size);
        }
        case ProtocolOperation::kAcquireUsageExclusiveLock:
        {
            const auto result = registry_.AcquireUsageExclusiveLock(request->registration.key, provider_context);
            ProtocolResponse response{};
            response.status_code = result.has_value() ? kStatusLockFailed : kStatusOk;
            return SerializeResponse(response, response_payload, serialized_response_size);
        }
        case ProtocolOperation::kReleaseUsageLock:
        {
            const auto result = registry_.ReleaseUsageLock(request->registration.key, provider_context);
            ProtocolResponse response{};
            response.status_code = result.has_value() ? kStatusLockFailed : kStatusOk;
            return SerializeResponse(response, response_payload, serialized_response_size);
        }
        case ProtocolOperation::kStartFindService:
        case ProtocolOperation::kStopFindService:
        {
            ProtocolResponse response{};
            response.status_code = kStatusMalformedRequest;
            return SerializeResponse(response, response_payload, serialized_response_size);
        }
    }

    ProtocolResponse response{};
    response.status_code = kStatusMalformedRequest;
    return SerializeResponse(response, response_payload, serialized_response_size);
}

auto ServiceDiscoveryDaemon::HandlePayload(const std::vector<std::uint8_t>& request_payload)
    -> std::vector<std::uint8_t>
{
    std::vector<std::uint8_t> response_payload(kMaxResponsePayloadSize);
    std::size_t serialized_response_size{0U};

    const ProviderContext provider_context{0U, 0U, 0U};
    const auto success =
        HandlePayload(score::cpp::span<const std::uint8_t>{request_payload.data(), request_payload.size()},
                      score::cpp::span<std::uint8_t>{response_payload.data(), response_payload.size()},
                      provider_context,
                      serialized_response_size);
    if (!success)
    {
        return {};
    }

    response_payload.resize(serialized_response_size);
    return response_payload;
}

}  // namespace score::mw::com::service_discovery
