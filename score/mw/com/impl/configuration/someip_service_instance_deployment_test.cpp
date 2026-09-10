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
#include "score/mw/com/impl/configuration/someip_service_instance_deployment.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>

namespace score::mw::com::impl
{
namespace
{

TEST(SomeIpServiceInstanceDeploymentTest, CanCreateFromSerializedObjectContainingInstanceId)
{
    const SomeIpServiceInstanceDeployment unit{SomeIpServiceInstanceId{10U}};

    const auto serialized_unit{unit.Serialize()};

    const SomeIpServiceInstanceDeployment reconstructed_unit{serialized_unit};

    EXPECT_EQ(reconstructed_unit, unit);
    ASSERT_TRUE(reconstructed_unit.instance_id_.has_value());
    EXPECT_EQ(reconstructed_unit.instance_id_->GetId(), 10U);
}

TEST(SomeIpServiceInstanceDeploymentTest, CanCreateFromSerializedObjectWithoutInstanceId)
{
    const SomeIpServiceInstanceDeployment unit{std::optional<SomeIpServiceInstanceId>{}};

    const auto serialized_unit{unit.Serialize()};

    const SomeIpServiceInstanceDeployment reconstructed_unit{serialized_unit};

    EXPECT_EQ(reconstructed_unit, unit);
    EXPECT_FALSE(reconstructed_unit.instance_id_.has_value());
}

TEST(SomeIpServiceInstanceDeploymentDeathTest, CreatingFromSerializedObjectWithMismatchedSerializationVersionTerminates)
{
    const SomeIpServiceInstanceDeployment unit{SomeIpServiceInstanceId{10U}};

    const auto serialization_version_key = "serializationVersion";
    const std::uint32_t invalid_serialization_version = SomeIpServiceInstanceDeployment::serializationVersion + 1U;

    auto serialized_unit{unit.Serialize()};
    auto it = serialized_unit.find(serialization_version_key);
    ASSERT_NE(it, serialized_unit.end());
    it->second = json::Any{invalid_serialization_version};

    EXPECT_DEATH(const SomeIpServiceInstanceDeployment reconstructed_unit{serialized_unit}, ".*");
}

TEST(SomeIpServiceInstanceDeploymentTest, DeploymentsWithTheSameInstanceIdAreEqual)
{
    const SomeIpServiceInstanceDeployment unit{SomeIpServiceInstanceId{10U}};
    const SomeIpServiceInstanceDeployment unit_2{SomeIpServiceInstanceId{10U}};

    EXPECT_EQ(unit, unit_2);
}

TEST(SomeIpServiceInstanceDeploymentTest, DeploymentsWithDifferentInstanceIdsAreNotEqual)
{
    const SomeIpServiceInstanceDeployment unit{SomeIpServiceInstanceId{10U}};
    const SomeIpServiceInstanceDeployment unit_2{SomeIpServiceInstanceId{11U}};

    EXPECT_FALSE(unit == unit_2);
    EXPECT_TRUE(unit < unit_2);
    EXPECT_FALSE(unit_2 < unit);
}

TEST(SomeIpServiceInstanceDeploymentTest, DeploymentsWithTheSameInstanceIdAreCompatible)
{
    const SomeIpServiceInstanceDeployment unit{SomeIpServiceInstanceId{10U}};
    const SomeIpServiceInstanceDeployment unit_2{SomeIpServiceInstanceId{10U}};

    EXPECT_TRUE(areCompatible(unit, unit_2));
}

TEST(SomeIpServiceInstanceDeploymentTest, DeploymentsWithDifferentInstanceIdsAreNotCompatible)
{
    const SomeIpServiceInstanceDeployment unit{SomeIpServiceInstanceId{10U}};
    const SomeIpServiceInstanceDeployment unit_2{SomeIpServiceInstanceId{11U}};

    EXPECT_FALSE(areCompatible(unit, unit_2));
}

TEST(SomeIpServiceInstanceDeploymentTest, DeploymentWithoutInstanceIdIsCompatibleWithAnyInstanceId)
{
    const SomeIpServiceInstanceDeployment find_any{std::optional<SomeIpServiceInstanceId>{}};
    const SomeIpServiceInstanceDeployment concrete{SomeIpServiceInstanceId{10U}};

    EXPECT_TRUE(areCompatible(find_any, concrete));
    EXPECT_TRUE(areCompatible(concrete, find_any));
}

}  // namespace
}  // namespace score::mw::com::impl
