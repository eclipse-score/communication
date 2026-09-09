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
    SomeIpServiceInstanceDeployment unit{SomeIpServiceInstanceId{1U}, {{"MyEvent", MakeEventInstanceDeployment()}}};

    EXPECT_TRUE(unit.ContainsEvent("MyEvent"));
    EXPECT_FALSE(unit.ContainsEvent("OtherEvent"));
}

TEST(SomeIpServiceInstanceDeploymentTest, ContainsFieldFindsConfiguredField)
{
    SomeIpServiceInstanceDeployment unit{
        SomeIpServiceInstanceId{1U},
        {},
        {{"MyField", SomeIpFieldInstanceDeployment{MakeEventInstanceDeployment(), true, true}}}};

    EXPECT_TRUE(unit.ContainsField("MyField"));
    EXPECT_FALSE(unit.ContainsField("OtherField"));
}

TEST(SomeIpServiceInstanceDeploymentTest, EqualityComparesAllMembers)
{
    const SomeIpServiceInstanceDeployment unit{SomeIpServiceInstanceId{1U},
                                               {{"MyEvent", MakeEventInstanceDeployment()}}};
    const SomeIpServiceInstanceDeployment same{SomeIpServiceInstanceId{1U},
                                               {{"MyEvent", MakeEventInstanceDeployment()}}};
    const SomeIpServiceInstanceDeployment different_instance_id{SomeIpServiceInstanceId{2U},
                                                               {{"MyEvent", MakeEventInstanceDeployment()}}};

    EXPECT_TRUE(unit == same);
    EXPECT_FALSE(unit == different_instance_id);
}

TEST(SomeIpServiceInstanceDeploymentTest, DeploymentsWithoutInstanceIdAreCompatibleWithEverything)
{
    const SomeIpServiceInstanceDeployment without_instance_id{std::optional<SomeIpServiceInstanceId>{}};
    const SomeIpServiceInstanceDeployment with_instance_id{SomeIpServiceInstanceId{1U}};

    EXPECT_TRUE(areCompatible(without_instance_id, with_instance_id));
    EXPECT_TRUE(areCompatible(with_instance_id, without_instance_id));
}

TEST(SomeIpServiceInstanceDeploymentTest, DeploymentsWithDifferentInstanceIdsAreNotCompatible)
{
    const SomeIpServiceInstanceDeployment unit{SomeIpServiceInstanceId{1U}};
    const SomeIpServiceInstanceDeployment other{SomeIpServiceInstanceId{2U}};

    EXPECT_FALSE(areCompatible(unit, other));
    EXPECT_TRUE(areCompatible(unit, unit));
}

TEST(SomeIpServiceInstanceDeploymentTest, CanRoundTripThroughSerialization)
{
    const SomeIpServiceInstanceDeployment unit{
        SomeIpServiceInstanceId{7U},
        {{"MyEvent", MakeEventInstanceDeployment()}},
        {{"MyField", SomeIpFieldInstanceDeployment{MakeEventInstanceDeployment(), true, false}}}};

    const SomeIpServiceInstanceDeployment reconstructed{unit.Serialize()};

    EXPECT_TRUE(unit == reconstructed);
}

TEST(SomeIpEventInstanceDeploymentTest, CanRoundTripThroughSerialization)
{
    const auto unit = MakeEventInstanceDeployment();

    const SomeIpEventInstanceDeployment reconstructed{unit.Serialize()};

    EXPECT_TRUE(unit == reconstructed);
}

TEST(SomeIpEventInstanceDeploymentTest, SetNumberOfSampleSlotsOverwritesTheConfiguredValue)
{
    auto unit = MakeEventInstanceDeployment();
    ASSERT_EQ(unit.GetNumberOfSampleSlots().value(), 5U);

    unit.SetNumberOfSampleSlots(9U);

    EXPECT_EQ(unit.GetNumberOfSampleSlots().value(), 9U);
}

TEST(SomeIpFieldInstanceDeploymentTest, CanRoundTripThroughSerialization)
{
    const SomeIpFieldInstanceDeployment unit{MakeEventInstanceDeployment(), true, false};

    const SomeIpFieldInstanceDeployment reconstructed{unit.Serialize()};

    EXPECT_TRUE(unit == reconstructed);
}

TEST(SomeIpServiceInstanceIdTest, EqualityAndOrderingFollowTheId)
{
    const SomeIpServiceInstanceId unit{10U};
    const SomeIpServiceInstanceId same{10U};
    const SomeIpServiceInstanceId greater{11U};

    EXPECT_TRUE(unit == same);
    EXPECT_TRUE(unit < greater);
    EXPECT_FALSE(greater < unit);
}

TEST(SomeIpServiceInstanceIdTest, CanRoundTripThroughSerialization)
{
    const SomeIpServiceInstanceId unit{10U};

    const SomeIpServiceInstanceId reconstructed{unit.Serialize()};

    EXPECT_TRUE(unit == reconstructed);
    EXPECT_EQ(unit.ToHashString(), reconstructed.ToHashString());
}

}  // namespace
}  // namespace score::mw::com::impl
