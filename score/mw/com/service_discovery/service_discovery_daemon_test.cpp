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

#include "score/mw/com/service_discovery/service_discovery_registry.h"

#include <gtest/gtest.h>

namespace score::mw::com::service_discovery
{

namespace
{

auto MakeProviderContext(const std::uint32_t provider_uid,
                         const std::uint32_t provider_pid,
                         const std::uint64_t provider_session_id) -> ProviderContext
{
    ProviderContext provider_context{};
    provider_context.provider_uid = provider_uid;
    provider_context.provider_pid = provider_pid;
    provider_context.provider_session_id = provider_session_id;
    return provider_context;
}

auto MakeRequest(const ProtocolOperation operation,
                 const std::uint64_t service_id,
                 const std::uint32_t instance_id,
                 const std::uint32_t uid,
                 const std::uint32_t pid) -> ProtocolRequest
{
    ProtocolRequest request{};
    request.operation = operation;
    request.registration.key.service_id = service_id;
    request.registration.key.instance_id = instance_id;
    request.registration.provider_uid = uid;
    request.registration.provider_pid = pid;
    request.registration.offered_integrity = IntegrityLevel::kAsilQm;
    request.registration.provider_integrity = IntegrityLevel::kAsilQm;
    request.registration.endpoint.address = "/tmp/sd.sock";
    return request;
}

}  // namespace

TEST(ServiceDiscoveryDaemonTest, RegisterThenResolveReturnsRegistration)
{
    ServiceDiscoveryRegistry registry{};
    ServiceDiscoveryDaemon daemon{registry};
    const auto provider_context = MakeProviderContext(101U, 202U, 1U);

    std::array<std::uint8_t, kMaxResponsePayloadSize> response_buffer{};
    std::size_t response_size{0U};

    const auto register_payload = SerializeRequest(MakeRequest(ProtocolOperation::kRegister, 10U, 1U, 100U, 200U));
    ASSERT_TRUE(
        daemon.HandlePayload(score::cpp::span<const std::uint8_t>{register_payload.data(), register_payload.size()},
                             score::cpp::span<std::uint8_t>{response_buffer.data(), response_buffer.size()},
                             provider_context,
                             response_size));
    const auto register_response =
        DeserializeResponse(score::cpp::span<const std::uint8_t>{response_buffer.data(), response_size});

    ASSERT_TRUE(register_response.has_value());
    EXPECT_EQ(register_response->status_code, 0U);

    const auto resolve_payload = SerializeRequest(MakeRequest(ProtocolOperation::kResolve, 10U, 1U, 0U, 0U));
    ASSERT_TRUE(
        daemon.HandlePayload(score::cpp::span<const std::uint8_t>{resolve_payload.data(), resolve_payload.size()},
                             score::cpp::span<std::uint8_t>{response_buffer.data(), response_buffer.size()},
                             provider_context,
                             response_size));
    const auto resolve_response =
        DeserializeResponse(score::cpp::span<const std::uint8_t>{response_buffer.data(), response_size});

    ASSERT_TRUE(resolve_response.has_value());
    EXPECT_EQ(resolve_response->status_code, 0U);
    ASSERT_EQ(resolve_response->registrations.size, 1U);
    EXPECT_EQ(resolve_response->registrations.values[0].provider_uid, 101U);
    EXPECT_EQ(resolve_response->registrations.values[0].provider_pid, 202U);
}

TEST(ServiceDiscoveryDaemonTest, RejectsMalformedRequest)
{
    ServiceDiscoveryRegistry registry{};
    ServiceDiscoveryDaemon daemon{registry};

    const std::vector<std::uint8_t> malformed_payload{1U, 2U, 3U};
    const auto response_payload = daemon.HandlePayload(malformed_payload);
    const auto response = DeserializeResponse(response_payload);

    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->status_code, 1U);
}

TEST(ServiceDiscoveryDaemonTest, CreationLockRequestConflictsUntilOwnerDisconnects)
{
    ServiceDiscoveryRegistry registry{};
    ServiceDiscoveryDaemon daemon{registry};

    std::array<std::uint8_t, kMaxResponsePayloadSize> response_buffer{};
    std::size_t response_size{0U};

    const auto owner_context = MakeProviderContext(101U, 202U, 1U);
    const auto other_context = MakeProviderContext(303U, 404U, 2U);

    const auto lock_payload = SerializeRequest(MakeRequest(ProtocolOperation::kAcquireCreationLock, 10U, 1U, 0U, 0U));
    ASSERT_TRUE(daemon.HandlePayload(score::cpp::span<const std::uint8_t>{lock_payload.data(), lock_payload.size()},
                                     score::cpp::span<std::uint8_t>{response_buffer.data(), response_buffer.size()},
                                     owner_context,
                                     response_size));
    auto lock_response =
        DeserializeResponse(score::cpp::span<const std::uint8_t>{response_buffer.data(), response_size});
    ASSERT_TRUE(lock_response.has_value());
    EXPECT_EQ(lock_response->status_code, 0U);

    ASSERT_TRUE(daemon.HandlePayload(score::cpp::span<const std::uint8_t>{lock_payload.data(), lock_payload.size()},
                                     score::cpp::span<std::uint8_t>{response_buffer.data(), response_buffer.size()},
                                     other_context,
                                     response_size));
    lock_response = DeserializeResponse(score::cpp::span<const std::uint8_t>{response_buffer.data(), response_size});
    ASSERT_TRUE(lock_response.has_value());
    EXPECT_EQ(lock_response->status_code, 5U);

    daemon.OnDisconnected(owner_context);

    ASSERT_TRUE(daemon.HandlePayload(score::cpp::span<const std::uint8_t>{lock_payload.data(), lock_payload.size()},
                                     score::cpp::span<std::uint8_t>{response_buffer.data(), response_buffer.size()},
                                     other_context,
                                     response_size));
    lock_response = DeserializeResponse(score::cpp::span<const std::uint8_t>{response_buffer.data(), response_size});
    ASSERT_TRUE(lock_response.has_value());
    EXPECT_EQ(lock_response->status_code, 0U);
}

}  // namespace score::mw::com::service_discovery
