/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
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

#include "score/mw/com/impl/configuration/test/configuration_test_resources.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl
{
namespace
{

TEST(SomeIpServiceInstanceDeployment, construction)
{
    SomeIpServiceInstanceDeployment unit{SomeIpServiceInstanceId{42U}};

    ASSERT_EQ(unit.instance_id_.value().GetId(), 42U);
}

TEST(SomeIpServiceInstanceDeployment, BothInstancesAnyIsCompatible)
{
    EXPECT_TRUE(areCompatible(SomeIpServiceInstanceDeployment{}, SomeIpServiceInstanceDeployment{}));
}

TEST(SomeIpServiceInstanceDeployment, OneInstancesAnyIsCompatible)
{
    EXPECT_TRUE(areCompatible(SomeIpServiceInstanceDeployment{},
                              SomeIpServiceInstanceDeployment{SomeIpServiceInstanceId{45U}}));
}

TEST(SomeIpServiceInstanceDeployment, OneInstancesAnyIsCompatibleOtherSide)
{
    EXPECT_TRUE(areCompatible(SomeIpServiceInstanceDeployment{SomeIpServiceInstanceId{45U}},
                              SomeIpServiceInstanceDeployment{}));
}

TEST(SomeIpServiceInstanceDeployment, SameInstancesIsCompatibleOtherSide)
{
    EXPECT_TRUE(areCompatible(SomeIpServiceInstanceDeployment{SomeIpServiceInstanceId{45U}},
                              SomeIpServiceInstanceDeployment{SomeIpServiceInstanceId{45U}}));
}

TEST(SomeIpServiceInstanceDeployment, DifferentInstancesIsNotCompatibleOtherSide)
{
    EXPECT_FALSE(areCompatible(SomeIpServiceInstanceDeployment{SomeIpServiceInstanceId{45U}},
                               SomeIpServiceInstanceDeployment{SomeIpServiceInstanceId{44U}}));
}

TEST(SomeIpServiceInstanceDeployment, Equality)
{
    EXPECT_EQ(SomeIpServiceInstanceDeployment{SomeIpServiceInstanceId{45U}},
              SomeIpServiceInstanceDeployment{SomeIpServiceInstanceId{45U}});
}

using SomeIpServiceInstanceDeploymentFixture = ConfigurationStructsFixture;
TEST_F(SomeIpServiceInstanceDeploymentFixture, CanCreateFromSerializedObjectWithOptional)
{
    const SomeIpServiceInstanceDeployment unit{MakeSomeIpServiceInstanceDeployment()};

    const auto serialized_unit{unit.Serialize()};

    const SomeIpServiceInstanceDeployment reconstructed_unit{serialized_unit};

    ExpectSomeIpServiceInstanceDeploymentObjectsEqual(reconstructed_unit, unit);
}

TEST_F(SomeIpServiceInstanceDeploymentFixture, CanCreateFromSerializedObjectWithoutOptional)
{
    const SomeIpServiceInstanceDeployment unit{MakeSomeIpServiceInstanceDeployment({})};

    const auto serialized_unit{unit.Serialize()};

    const SomeIpServiceInstanceDeployment reconstructed_unit{serialized_unit};

    ExpectSomeIpServiceInstanceDeploymentObjectsEqual(reconstructed_unit, unit);
}

TEST(SomeIpServiceInstanceDeploymentDeathTest, CreatingFromSerializedObjectWithMismatchedSerializationVersionTerminates)
{
    const SomeIpServiceInstanceDeployment unit{42U};

    const auto serialization_version_key = "serializationVersion";
    const std::uint32_t invalid_serialization_version = SomeIpServiceInstanceDeployment::serializationVersion + 1;

    auto serialized_unit{unit.Serialize()};
    auto it = serialized_unit.find(serialization_version_key);
    ASSERT_NE(it, serialized_unit.end());
    it->second = json::Any{invalid_serialization_version};

    EXPECT_DEATH(SomeIpServiceInstanceDeployment reconstructed_unit{serialized_unit}, ".*");
}

TEST(LolaServiceInstanceDeploymentLessThan, DeploymentsComparedBasedOnInstanceId)
{
    // Given 2 SomeIpServiceInstanceDeployments containing different values
    const SomeIpServiceInstanceDeployment lhs{1U};
    const SomeIpServiceInstanceDeployment rhs{2U};

    // When comparing the two
    // Then the result is based on the instance IDs
    EXPECT_EQ(lhs < rhs, lhs.instance_id_ < rhs.instance_id_);
    EXPECT_EQ(rhs < lhs, rhs.instance_id_ < lhs.instance_id_);
}

}  // namespace
}  // namespace score::mw::com::impl
