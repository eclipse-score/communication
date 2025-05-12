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
#include "score/mw/com/impl/configuration/someip_field_instance_deployment.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace score::mw::com::impl
{
namespace
{

TEST(SomeIpFieldInstanceDeploymentTest, DefaultConstructedObjectsAreEqual)
{
    // Given two default-constructed SomeIpFieldInstanceDeployment objects
    const SomeIpFieldInstanceDeployment lhs;
    const SomeIpFieldInstanceDeployment rhs;

    // When comparing them for equality
    // Then they should be equal
    EXPECT_TRUE(lhs == rhs);
}

TEST(SomeIpFieldInstanceDeploymentTest, JsonConstructedObjectsAreEqual)
{
    // Given a default-constructed JSON object
    const score::json::Object json_object;
    const SomeIpFieldInstanceDeployment lhs{json_object};
    const SomeIpFieldInstanceDeployment rhs{json_object};

    // When comparing them for equality
    // Then they should be equal
    EXPECT_TRUE(lhs == rhs);
}

TEST(SomeIpFieldInstanceDeploymentTest, SerializeReturnsEmptyObject)
{
    // Given a default-constructed SomeIpFieldInstanceDeployment object
    const SomeIpFieldInstanceDeployment deployment;

    // When serializing the object
    const auto serialized = deployment.Serialize();

    // Then the serialized result expected be empty
    EXPECT_TRUE(serialized.empty());
}

}  // namespace
}  // namespace score::mw::com::impl
