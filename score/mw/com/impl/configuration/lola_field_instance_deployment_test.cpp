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
#include "score/mw/com/impl/configuration/lola_field_instance_deployment.h"

#include "score/mw/com/impl/configuration/test/configuration_test_resources.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl
{
namespace
{

using LolaFieldInstanceDeploymentFixture = ConfigurationStructsFixture;
TEST_F(LolaFieldInstanceDeploymentFixture, CanCreateFromSerializedObjectWithOptionals)
{
    const LolaFieldInstanceDeployment unit{MakeLolaFieldInstanceDeployment()};

    const auto serialized_unit{unit.Serialize()};

    const LolaFieldInstanceDeployment reconstructed_unit{serialized_unit};

    ExpectLolaFieldInstanceDeploymentObjectsEqual(reconstructed_unit, unit);
}

TEST_F(LolaFieldInstanceDeploymentFixture, CanCreateFromSerializedObjectWithoutOptionals)
{
    const std::uint16_t max_samples{12};
    const std::optional<std::uint8_t> max_subscribers{13};
    const std::optional<std::uint8_t> max_concurrent_allocations{};
    const bool enforce_max_samples{true};

    const LolaFieldInstanceDeployment unit{
        MakeLolaFieldInstanceDeployment(max_samples, max_subscribers, max_concurrent_allocations, enforce_max_samples)};

    const auto serialized_unit{unit.Serialize()};

    LolaFieldInstanceDeployment reconstructed_unit{serialized_unit};

    ExpectLolaFieldInstanceDeploymentObjectsEqual(reconstructed_unit, unit);
}

TEST(LolaFieldInstanceDeploymentDeathTest, CreatingFromSerializedObjectWithMismatchedSerializationVersionTerminates)
{
    const LolaFieldInstanceDeployment unit{MakeLolaFieldInstanceDeployment()};

    const auto serialization_version_key = "serializationVersion";
    const std::uint32_t invalid_serialization_version = LolaFieldInstanceDeployment::serializationVersion + 1;

    auto serialized_unit{unit.Serialize()};
    auto it = serialized_unit.find(serialization_version_key);
    ASSERT_NE(it, serialized_unit.end());
    it->second = json::Any{invalid_serialization_version};

    EXPECT_DEATH(LolaFieldInstanceDeployment reconstructed_unit{serialized_unit}, ".*");
}

}  // namespace
}  // namespace score::mw::com::impl
