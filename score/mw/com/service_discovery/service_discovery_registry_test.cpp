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

auto MakeRegistration(const std::uint64_t service_id,
                      const std::uint32_t instance_id,
                      const std::uint32_t provider_uid,
                      const std::uint32_t provider_pid,
                      const IntegrityLevel offered_integrity,
                      const IntegrityLevel provider_integrity) -> Registration
{
    Registration registration{};
    registration.key.service_id = service_id;
    registration.key.instance_id = instance_id;
    registration.provider_uid = provider_uid;
    registration.provider_pid = provider_pid;
    registration.offered_integrity = offered_integrity;
    registration.provider_integrity = provider_integrity;
    registration.endpoint.address = "/tmp/sd.sock";

    return registration;
}

}  // namespace

TEST(ServiceDiscoveryRegistryTest, RegistersAndResolvesSingleInstance)
{
    ServiceDiscoveryRegistry unit{};
    const auto registration = MakeRegistration(100U, 1U, 10U, 1000U, IntegrityLevel::kAsilQm, IntegrityLevel::kAsilQm);
    const auto provider_context = MakeProviderContext(10U, 1000U, 1U);

    EXPECT_EQ(unit.Register(registration, provider_context), std::nullopt);

    const auto resolved = unit.Resolve(registration.key);
    ASSERT_EQ(resolved.size, 1U);
    EXPECT_EQ(resolved.values[0].provider_uid, 10U);
    EXPECT_EQ(resolved.values[0].provider_session_id, 1U);
}

TEST(ServiceDiscoveryRegistryTest, RejectsDuplicateRegistrationFromSameProvider)
{
    ServiceDiscoveryRegistry unit{};
    const auto registration = MakeRegistration(100U, 1U, 10U, 1000U, IntegrityLevel::kAsilQm, IntegrityLevel::kAsilQm);
    const auto provider_context = MakeProviderContext(10U, 1000U, 1U);

    EXPECT_EQ(unit.Register(registration, provider_context), std::nullopt);
    EXPECT_EQ(unit.Register(registration, provider_context), RegisterError::kAlreadyRegistered);
}

TEST(ServiceDiscoveryRegistryTest, RejectsInvalidAsilBClaimFromQmProvider)
{
    ServiceDiscoveryRegistry unit{};
    const auto registration = MakeRegistration(100U, 1U, 10U, 1000U, IntegrityLevel::kAsilB, IntegrityLevel::kAsilQm);
    const auto provider_context = MakeProviderContext(10U, 1000U, 1U);

    EXPECT_EQ(unit.Register(registration, provider_context), RegisterError::kInvalidIntegrityClaim);
    EXPECT_TRUE(unit.Resolve(registration.key).Empty());
}

TEST(ServiceDiscoveryRegistryTest, UnregistersByProviderIdentity)
{
    ServiceDiscoveryRegistry unit{};
    const auto registration = MakeRegistration(100U, 1U, 10U, 1000U, IntegrityLevel::kAsilQm, IntegrityLevel::kAsilQm);
    const auto provider_context = MakeProviderContext(10U, 1000U, 1U);

    ASSERT_EQ(unit.Register(registration, provider_context), std::nullopt);

    EXPECT_EQ(unit.Unregister(registration.key, provider_context), std::nullopt);
    EXPECT_TRUE(unit.Resolve(registration.key).Empty());
}

TEST(ServiceDiscoveryRegistryTest, RejectsUnregisterFromDifferentProvider)
{
    ServiceDiscoveryRegistry unit{};
    const auto registration = MakeRegistration(100U, 1U, 10U, 1000U, IntegrityLevel::kAsilQm, IntegrityLevel::kAsilQm);
    const auto provider_context = MakeProviderContext(10U, 1000U, 1U);
    const auto other_provider_context = MakeProviderContext(10U, 1000U, 2U);

    ASSERT_EQ(unit.Register(registration, provider_context), std::nullopt);

    EXPECT_EQ(unit.Unregister(registration.key, other_provider_context), UnregisterError::kOwnershipMismatch);
    EXPECT_EQ(unit.Resolve(registration.key).size, 1U);
}

TEST(ServiceDiscoveryRegistryTest, RemoveBySessionRemovesAllRegistrationsOwnedByDisconnectedSession)
{
    ServiceDiscoveryRegistry unit{};
    const auto first_registration =
        MakeRegistration(100U, 1U, 10U, 1000U, IntegrityLevel::kAsilQm, IntegrityLevel::kAsilQm);
    const auto second_registration =
        MakeRegistration(100U, 2U, 10U, 1000U, IntegrityLevel::kAsilQm, IntegrityLevel::kAsilQm);
    const auto first_provider_context = MakeProviderContext(10U, 1000U, 1U);
    const auto second_provider_context = MakeProviderContext(10U, 1000U, 2U);

    ASSERT_EQ(unit.Register(first_registration, first_provider_context), std::nullopt);
    ASSERT_EQ(unit.Register(second_registration, second_provider_context), std::nullopt);

    unit.RemoveBySession(1U);

    EXPECT_TRUE(unit.Resolve(first_registration.key).Empty());
    EXPECT_EQ(unit.Resolve(second_registration.key).size, 1U);
}

TEST(ServiceDiscoveryRegistryTest, ResolveAnyInstanceReturnsAllMatchingServiceInstances)
{
    ServiceDiscoveryRegistry unit{};
    const auto first_registration =
        MakeRegistration(100U, 1U, 10U, 1000U, IntegrityLevel::kAsilQm, IntegrityLevel::kAsilQm);
    const auto second_registration =
        MakeRegistration(100U, 2U, 11U, 1001U, IntegrityLevel::kAsilQm, IntegrityLevel::kAsilQm);
    const auto other_service_registration =
        MakeRegistration(200U, 1U, 12U, 1002U, IntegrityLevel::kAsilQm, IntegrityLevel::kAsilQm);

    ASSERT_EQ(unit.Register(first_registration, MakeProviderContext(10U, 1000U, 1U)), std::nullopt);
    ASSERT_EQ(unit.Register(second_registration, MakeProviderContext(11U, 1001U, 2U)), std::nullopt);
    ASSERT_EQ(unit.Register(other_service_registration, MakeProviderContext(12U, 1002U, 3U)), std::nullopt);

    ServiceKey any_instance_key{};
    any_instance_key.service_id = 100U;
    any_instance_key.instance_id = kAnyInstanceId;

    const auto resolved = unit.Resolve(any_instance_key);
    ASSERT_EQ(resolved.size, 2U);
}

TEST(ServiceDiscoveryRegistryTest, CreationLockIsExclusiveAndReleasedByUnregister)
{
    ServiceDiscoveryRegistry unit{};
    const auto key = ServiceKey{100U, 1U};
    const auto owner_context = MakeProviderContext(10U, 1000U, 1U);
    const auto other_context = MakeProviderContext(11U, 1001U, 2U);
    const auto registration = MakeRegistration(100U, 1U, 10U, 1000U, IntegrityLevel::kAsilQm, IntegrityLevel::kAsilQm);

    EXPECT_EQ(unit.AcquireCreationLock(key, owner_context), std::nullopt);
    EXPECT_EQ(unit.AcquireCreationLock(key, other_context), ServiceInstanceLockError::kConflict);

    ASSERT_EQ(unit.Register(registration, owner_context), std::nullopt);
    EXPECT_EQ(unit.Unregister(key, owner_context), std::nullopt);
    EXPECT_EQ(unit.AcquireCreationLock(key, other_context), std::nullopt);
}

TEST(ServiceDiscoveryRegistryTest, UsageSharedAndExclusiveLocksConflictAsExpected)
{
    ServiceDiscoveryRegistry unit{};
    const auto key = ServiceKey{100U, 1U};
    const auto shared_owner_a = MakeProviderContext(10U, 1000U, 1U);
    const auto shared_owner_b = MakeProviderContext(11U, 1001U, 2U);
    const auto exclusive_owner = MakeProviderContext(12U, 1002U, 3U);

    EXPECT_EQ(unit.AcquireUsageSharedLock(key, shared_owner_a), std::nullopt);
    EXPECT_EQ(unit.AcquireUsageSharedLock(key, shared_owner_b), std::nullopt);
    EXPECT_EQ(unit.AcquireUsageExclusiveLock(key, exclusive_owner), ServiceInstanceLockError::kConflict);

    EXPECT_EQ(unit.ReleaseUsageLock(key, shared_owner_a), std::nullopt);
    EXPECT_EQ(unit.ReleaseUsageLock(key, shared_owner_b), std::nullopt);
    EXPECT_EQ(unit.AcquireUsageExclusiveLock(key, exclusive_owner), std::nullopt);
    EXPECT_EQ(unit.AcquireUsageSharedLock(key, shared_owner_a), ServiceInstanceLockError::kConflict);
}

TEST(ServiceDiscoveryRegistryTest, DisconnectCleanupReleasesCreationAndUsageLocks)
{
    ServiceDiscoveryRegistry unit{};
    const auto key = ServiceKey{100U, 1U};
    const auto owner_context = MakeProviderContext(10U, 1000U, 1U);
    const auto other_context = MakeProviderContext(11U, 1001U, 2U);

    EXPECT_EQ(unit.AcquireCreationLock(key, owner_context), std::nullopt);
    EXPECT_EQ(unit.AcquireUsageExclusiveLock(key, owner_context), std::nullopt);

    unit.RemoveBySession(owner_context.provider_session_id);

    EXPECT_EQ(unit.AcquireCreationLock(key, other_context), std::nullopt);
    EXPECT_EQ(unit.AcquireUsageExclusiveLock(key, other_context), std::nullopt);
}

}  // namespace score::mw::com::service_discovery
