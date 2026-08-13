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

#include <gtest/gtest.h>

namespace score::mw::com::service_discovery
{

namespace
{

auto MakeRegistration() -> Registration
{
    Registration registration{};
    registration.key.service_id = 42U;
    registration.key.instance_id = 7U;
    registration.offered_integrity = IntegrityLevel::kAsilB;
    registration.provider_integrity = IntegrityLevel::kAsilB;
    registration.provider_uid = 123U;
    registration.provider_pid = 456U;
    registration.endpoint.address = "/mw_com/message_passing/sd_daemon";
    return registration;
}

}  // namespace

TEST(ServiceDiscoveryProtocolTest, SerializesAndDeserializesRequest)
{
    ::testing::Test::RecordProperty("lobster-tracing", "ServiceDiscovery.ProtocolEncodesRegisterUnregisterResolve");
    ProtocolRequest request{};
    request.operation = ProtocolOperation::kRegister;
    request.registration = MakeRegistration();
    request.search_handle = 55U;

    const auto payload = SerializeRequest(request);
    const auto decoded = DeserializeRequest(payload);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->operation, ProtocolOperation::kRegister);
    EXPECT_EQ(decoded->search_handle, 55U);
    EXPECT_EQ(decoded->registration.key.service_id, 42U);
    EXPECT_EQ(decoded->registration.key.instance_id, 7U);
    EXPECT_EQ(decoded->registration.provider_uid, 123U);
    EXPECT_EQ(decoded->registration.provider_pid, 456U);
    EXPECT_EQ(decoded->registration.endpoint.address, "/mw_com/message_passing/sd_daemon");
}

TEST(ServiceDiscoveryProtocolTest, SerializesAndDeserializesResponse)
{
    ::testing::Test::RecordProperty("lobster-tracing", "ServiceDiscovery.RegistryResolveApi");
    ProtocolResponse response{};
    response.status_code = 9U;
    response.search_handle = 88U;
    ASSERT_TRUE(response.PushRegistration(MakeRegistration()));

    const auto payload = SerializeResponse(response);
    const auto decoded = DeserializeResponse(payload);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->status_code, 9U);
    EXPECT_EQ(decoded->search_handle, 88U);
    ASSERT_EQ(decoded->registrations.size, 1U);
    EXPECT_EQ(decoded->registrations.values[0].endpoint.address, "/mw_com/message_passing/sd_daemon");
}

TEST(ServiceDiscoveryProtocolTest, RejectsTruncatedPayload)
{
    ::testing::Test::RecordProperty("lobster-tracing", "ServiceDiscovery.ProtocolEncodesRegisterUnregisterResolve");
    const std::vector<std::uint8_t> payload{static_cast<std::uint8_t>(ProtocolOperation::kResolve), 1U, 2U};
    EXPECT_FALSE(DeserializeRequest(payload).has_value());
}

TEST(ServiceDiscoveryProtocolTest, SerializesAndDeserializesNotification)
{
    ::testing::Test::RecordProperty("lobster-tracing", "ServiceDiscovery.ProtocolEncodesAsyncDiscoveryUpdates");
    ProtocolNotification notification{};
    notification.search_handle = 44U;
    notification.key.service_id = 42U;
    notification.key.instance_id = 7U;
    notification.response.status_code = 0U;
    ASSERT_TRUE(notification.response.PushRegistration(MakeRegistration()));

    const auto payload = SerializeNotification(notification);
    const auto decoded = DeserializeNotification(payload);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->search_handle, 44U);
    EXPECT_EQ(decoded->key.service_id, 42U);
    EXPECT_EQ(decoded->key.instance_id, 7U);
    ASSERT_EQ(decoded->response.registrations.size, 1U);
    EXPECT_EQ(decoded->response.registrations.values[0].provider_uid, 123U);
}

TEST(ServiceDiscoveryProtocolTest, SerializesStartAndStopFindServiceRequests)
{
    ::testing::Test::RecordProperty("lobster-tracing", "ServiceDiscovery.ProtocolEncodesStartAndStopFindService");

    ProtocolRequest start_find_request{};
    start_find_request.operation = ProtocolOperation::kStartFindService;
    start_find_request.registration.key.service_id = 99U;
    start_find_request.registration.key.instance_id = 7U;

    const auto start_payload = SerializeRequest(start_find_request);
    const auto start_decoded = DeserializeRequest(start_payload);
    ASSERT_TRUE(start_decoded.has_value());
    EXPECT_EQ(start_decoded->operation, ProtocolOperation::kStartFindService);
    EXPECT_EQ(start_decoded->registration.key.service_id, 99U);
    EXPECT_EQ(start_decoded->registration.key.instance_id, 7U);

    ProtocolRequest stop_find_request{};
    stop_find_request.operation = ProtocolOperation::kStopFindService;
    stop_find_request.search_handle = 55U;

    const auto stop_payload = SerializeRequest(stop_find_request);
    const auto stop_decoded = DeserializeRequest(stop_payload);
    ASSERT_TRUE(stop_decoded.has_value());
    EXPECT_EQ(stop_decoded->operation, ProtocolOperation::kStopFindService);
    EXPECT_EQ(stop_decoded->search_handle, 55U);
}

TEST(ServiceDiscoveryProtocolTest, SerializesDaemonManagedLockRequestOperations)
{
    ::testing::Test::RecordProperty("lobster-tracing", "ServiceDiscovery.ProtocolEncodesDaemonManagedLockOperations");

    const std::array<ProtocolOperation, 5U> lock_operations{
        ProtocolOperation::kAcquireCreationLock,
        ProtocolOperation::kReleaseCreationLock,
        ProtocolOperation::kAcquireUsageSharedLock,
        ProtocolOperation::kAcquireUsageExclusiveLock,
        ProtocolOperation::kReleaseUsageLock,
    };

    for (const auto operation : lock_operations)
    {
        ProtocolRequest request{};
        request.operation = operation;
        request.registration.key.service_id = 123U;
        request.registration.key.instance_id = 8U;

        const auto payload = SerializeRequest(request);
        const auto decoded = DeserializeRequest(payload);
        ASSERT_TRUE(decoded.has_value());
        EXPECT_EQ(decoded->operation, operation);
        EXPECT_EQ(decoded->registration.key.service_id, 123U);
        EXPECT_EQ(decoded->registration.key.instance_id, 8U);
    }
}

}  // namespace score::mw::com::service_discovery
