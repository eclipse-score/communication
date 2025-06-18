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
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"

#include "score/mw/com/impl/configuration/test/configuration_test_resources.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace score::mw::com::impl
{
namespace
{

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

TEST(LolaServiceInstanceDeployment, construction)
{
    LolaServiceInstanceDeployment unit{LolaServiceInstanceId{LolaServiceInstanceId{42U}}};

    ASSERT_EQ(unit.instance_id_.value(), LolaServiceInstanceId{42U});
}

TEST(LolaServiceInstanceDeployment, InstanceIsOptional)
{
    LolaServiceInstanceDeployment unit{};

    ASSERT_FALSE(unit.instance_id_.has_value());
}

TEST(LolaServiceInstanceDeployment, SameServiceIdBothInstancesAnyIsCompatible)
{
    EXPECT_TRUE(areCompatible(LolaServiceInstanceDeployment{LolaServiceInstanceId{43U}},
                              LolaServiceInstanceDeployment{LolaServiceInstanceId{43U}}));
}

TEST(LolaServiceInstanceDeployment, DifferentServiceIdBothInstancesAnyIsNotCompatible)
{
    EXPECT_FALSE(areCompatible(LolaServiceInstanceDeployment{LolaServiceInstanceId{43U}},
                               LolaServiceInstanceDeployment{LolaServiceInstanceId{44U}}));
}

TEST(LolaServiceInstanceDeployment, SameServiceIdOneInstancesAnyIsCompatible)
{
    EXPECT_TRUE(
        areCompatible(LolaServiceInstanceDeployment{LolaServiceInstanceId{43U}}, LolaServiceInstanceDeployment{}));
}

TEST(LolaServiceInstanceDeployment, Equality)
{
    EXPECT_EQ(LolaServiceInstanceDeployment{LolaServiceInstanceId{43U}},
              LolaServiceInstanceDeployment{LolaServiceInstanceId{43U}});
}

TEST(LolaServiceInstanceDeployment, ContainsEventReturnsTrueIfEventPresent)
{
    LolaServiceInstanceDeployment unit{LolaServiceInstanceId{2U}};
    auto temp = MakeDefaultLolaEventInstanceDeployment();
    unit.events_.emplace(std::make_pair("abc", temp));
    EXPECT_TRUE(unit.ContainsEvent("abc"));
}

TEST(LolaServiceInstanceDeployment, ContainsEventReturnsFalseIfEventMissing)
{
    LolaServiceInstanceDeployment unit{LolaServiceInstanceId{2U}};
    auto temp = MakeDefaultLolaEventInstanceDeployment();
    unit.events_.emplace(std::make_pair("abc", temp));
    EXPECT_FALSE(unit.ContainsEvent("def"));
}

TEST(LolaServiceInstanceDeployment, ContainsFieldReturnsTrueIfEventPresent)
{
    LolaServiceInstanceDeployment unit{LolaServiceInstanceId{LolaServiceInstanceId{2U}}};
    auto temp = MakeDefaultLolaEventInstanceDeployment();
    unit.fields_.emplace(std::make_pair("abc", temp));
    EXPECT_TRUE(unit.ContainsField("abc"));
}

TEST(LolaServiceInstanceDeployment, ContainsFieldReturnsFalseIfEventMissing)
{
    LolaServiceInstanceDeployment unit{LolaServiceInstanceId{2U}};
    auto temp = MakeDefaultLolaEventInstanceDeployment();
    unit.fields_.emplace(std::make_pair("abc", temp));
    EXPECT_FALSE(unit.ContainsField("def"));
}

using LolaServiceInstanceDeploymentFixture = ConfigurationStructsFixture;
TEST_F(LolaServiceInstanceDeploymentFixture, CanCreateFromSerializedObjectWithOptionals)
{
    const LolaServiceInstanceDeployment unit{MakeLolaServiceInstanceDeployment()};

    const auto serialized_unit{unit.Serialize()};

    LolaServiceInstanceDeployment reconstructed_unit{serialized_unit};

    ExpectLolaServiceInstanceDeploymentObjectsEqual(reconstructed_unit, unit);
}

TEST_F(LolaServiceInstanceDeploymentFixture, CanCreateFromSerializedObjectWithSetStrict)
{
    LolaServiceInstanceDeployment unit{MakeLolaServiceInstanceDeployment()};
    unit.strict_permissions_ = true;

    const auto serialized_unit{unit.Serialize()};

    LolaServiceInstanceDeployment reconstructed_unit{serialized_unit};

    ExpectLolaServiceInstanceDeploymentObjectsEqual(reconstructed_unit, unit);
}

TEST_F(LolaServiceInstanceDeploymentFixture, CanCreateFromSerializedObjectWithoutOptionals)
{
    const LolaServiceInstanceDeployment unit{MakeLolaServiceInstanceDeployment({}, {})};

    const auto serialized_unit{unit.Serialize()};

    LolaServiceInstanceDeployment reconstructed_unit{serialized_unit};

    ExpectLolaServiceInstanceDeploymentObjectsEqual(reconstructed_unit, unit);
}

TEST(LolaServiceInstanceDeploymentDeathTest, CreatingFromSerializedObjectWithMismatchedSerializationVersionTerminates)
{
    const LolaServiceInstanceDeployment unit{LolaServiceInstanceId{42U}};

    const auto serialization_version_key = "serializationVersion";
    const std::uint32_t invalid_serialization_version = LolaServiceInstanceDeployment::serializationVersion + 1;

    auto serialized_unit{unit.Serialize()};
    auto it = serialized_unit.find(serialization_version_key);
    ASSERT_NE(it, serialized_unit.end());
    it->second = json::Any{invalid_serialization_version};

    EXPECT_DEATH(LolaServiceInstanceDeployment reconstructed_unit{serialized_unit}, ".*");
}

}  // namespace
}  // namespace score::mw::com::impl
