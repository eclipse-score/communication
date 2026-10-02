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

#include "score/mw/com/impl/configuration/someip_event_instance_deployment.h"
#include "score/mw/com/impl/configuration/someip_field_instance_deployment.h"
#include "score/mw/com/impl/configuration/someip_service_instance_id.h"

#include <gtest/gtest.h>

#include <optional>

namespace score::mw::com::impl
{
namespace
{

SomeIpEventInstanceDeployment MakeEventInstanceDeployment()
{
    return SomeIpEventInstanceDeployment{5U, 2U, std::optional<std::uint8_t>{1U}, true};
}

TEST(SomeIpServiceInstanceDeploymentTest, ContainsEventFindsConfiguredEvent)
{
    // Given a service with a configured event
    SomeIpServiceInstanceDeployment unit{SomeIpServiceInstanceId{1U}, {{"MyEvent", MakeEventInstanceDeployment()}}};

    // When checking configured and missing events
    // Then only the configured event is found
    EXPECT_TRUE(unit.ContainsEvent("MyEvent"));
    EXPECT_FALSE(unit.ContainsEvent("OtherEvent"));
}

TEST(SomeIpServiceInstanceDeploymentTest, ContainsFieldFindsConfiguredField)
{
    // Given a service with a configured field
    SomeIpServiceInstanceDeployment unit{
        SomeIpServiceInstanceId{1U}, {}, {{"MyField", SomeIpFieldInstanceDeployment{MakeEventInstanceDeployment()}}}};

    // When checking configured and missing fields
    // Then only the configured field is found
    EXPECT_TRUE(unit.ContainsField("MyField"));
    EXPECT_FALSE(unit.ContainsField("OtherField"));
}

TEST(SomeIpServiceInstanceDeploymentTest, EqualityComparesAllMembers)
{
    // Given deployments with matching and different instance IDs
    const SomeIpServiceInstanceDeployment unit{SomeIpServiceInstanceId{1U},
                                               {{"MyEvent", MakeEventInstanceDeployment()}}};
    const SomeIpServiceInstanceDeployment same{SomeIpServiceInstanceId{1U},
                                               // When comparing the deployments
                                               // Then only the matching deployment is equal
                                               {{"MyEvent", MakeEventInstanceDeployment()}}};
    const SomeIpServiceInstanceDeployment different_instance_id{SomeIpServiceInstanceId{2U},
                                                                {{"MyEvent", MakeEventInstanceDeployment()}}};

    EXPECT_TRUE(unit == same);
    EXPECT_FALSE(unit == different_instance_id);
}

TEST(SomeIpServiceInstanceDeploymentTest, DeploymentsWithoutInstanceIdAreCompatibleWithEverything)
{
    // Given deployments with and without an instance ID
    const SomeIpServiceInstanceDeployment without_instance_id{std::optional<SomeIpServiceInstanceId>{}};
    const SomeIpServiceInstanceDeployment with_instance_id{SomeIpServiceInstanceId{1U}};

    // When checking compatibility
    // Then they are compatible in either order
    EXPECT_TRUE(areCompatible(without_instance_id, with_instance_id));
    EXPECT_TRUE(areCompatible(with_instance_id, without_instance_id));
}

TEST(SomeIpServiceInstanceDeploymentTest, DeploymentsWithDifferentInstanceIdsAreNotCompatible)
{
    // Given deployments with different instance IDs
    const SomeIpServiceInstanceDeployment unit{SomeIpServiceInstanceId{1U}};
    const SomeIpServiceInstanceDeployment other{SomeIpServiceInstanceId{2U}};

    // When checking compatibility
    // Then different IDs are incompatible and an instance is compatible with itself
    EXPECT_FALSE(areCompatible(unit, other));
    EXPECT_TRUE(areCompatible(unit, unit));
}

TEST(SomeIpServiceInstanceDeploymentTest, CanRoundTripThroughSerialization)
{
    // Given a service deployment with an event and field
    const SomeIpServiceInstanceDeployment unit{
        SomeIpServiceInstanceId{7U},
        {{"MyEvent", MakeEventInstanceDeployment()}},
        {{"MyField", SomeIpFieldInstanceDeployment{MakeEventInstanceDeployment()}}}};

    // When serializing and reconstructing it
    const SomeIpServiceInstanceDeployment reconstructed{unit.Serialize()};

    // Then the reconstructed deployment matches
    EXPECT_TRUE(unit == reconstructed);
}

TEST(SomeIpEventInstanceDeploymentTest, CanRoundTripThroughSerialization)
{
    // Given an event deployment
    const auto unit = MakeEventInstanceDeployment();

    // When serializing and reconstructing it
    const SomeIpEventInstanceDeployment reconstructed{unit.Serialize()};

    // Then the reconstructed deployment matches
    EXPECT_TRUE(unit == reconstructed);
}

TEST(SomeIpEventInstanceDeploymentTest, SetNumberOfSampleSlotsOverwritesTheConfiguredValue)
{
    // Given an event deployment with five sample slots
    auto unit = MakeEventInstanceDeployment();
    ASSERT_EQ(unit.GetNumberOfSampleSlots().value(), 5U);

    // When setting the sample slot count to nine
    unit.SetNumberOfSampleSlots(9U);

    // Then the configured count is updated
    EXPECT_EQ(unit.GetNumberOfSampleSlots().value(), 9U);
}

TEST(SomeIpFieldInstanceDeploymentTest, CanRoundTripThroughSerialization)
{
    // Given a field deployment
    const SomeIpFieldInstanceDeployment unit{MakeEventInstanceDeployment()};

    // When serializing and reconstructing it
    const SomeIpFieldInstanceDeployment reconstructed{unit.Serialize()};

    // Then the reconstructed deployment matches
    EXPECT_TRUE(unit == reconstructed);
}

TEST(SomeIpServiceInstanceIdTest, EqualityAndOrderingFollowTheId)
{
    // Given IDs with equal and increasing values
    const SomeIpServiceInstanceId unit{10U};
    const SomeIpServiceInstanceId same{10U};
    const SomeIpServiceInstanceId greater{11U};

    // When comparing IDs
    // Then equality and ordering follow their values
    EXPECT_TRUE(unit == same);
    EXPECT_TRUE(unit < greater);
    EXPECT_FALSE(greater < unit);
}

TEST(SomeIpServiceInstanceIdTest, CanRoundTripThroughSerialization)
{
    // Given a service instance ID
    const SomeIpServiceInstanceId unit{10U};

    // When serializing and reconstructing it
    const SomeIpServiceInstanceId reconstructed{unit.Serialize()};

    // Then the ID and its hash string are preserved
    EXPECT_TRUE(unit == reconstructed);
    EXPECT_EQ(unit.ToHashString(), reconstructed.ToHashString());
}

}  // namespace
}  // namespace score::mw::com::impl
