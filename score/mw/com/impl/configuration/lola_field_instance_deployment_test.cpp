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

namespace
{
constexpr std::uint16_t kMaxSamples{12U};
constexpr std::uint8_t kMaxSubscribers{13U};
constexpr std::uint8_t kMaxConcurrentAllocations{14U};
constexpr bool kEnforceMaxSamples{true};
constexpr std::uint8_t kNumberOfTracingSlots{1U};
}  // namespace
TEST_F(LolaFieldInstanceDeploymentFixture, CanCreateFromSerializedObjectWithOptionals)
{
    const LolaFieldInstanceDeployment unit{MakeLolaFieldInstanceDeployment()};

    const auto serialized_unit{unit.Serialize()};

    const LolaFieldInstanceDeployment reconstructed_unit{serialized_unit};

    ExpectLolaFieldInstanceDeploymentObjectsEqual(reconstructed_unit, unit);
}

TEST_F(LolaFieldInstanceDeploymentFixture, CanCreateFromSerializedObjectWithoutOptionals)
{

    const std::optional<std::uint8_t> kNoMaxConcurrentAllocations{};
    constexpr std::uint8_t kNoTracingSlots{0U};

    const LolaFieldInstanceDeployment unit{MakeLolaFieldInstanceDeployment(kMaxSamples,
                                                                           kMaxSubscribers,
                                                                           kNoMaxConcurrentAllocations,
                                                                           kEnforceMaxSamples,
                                                                           kNoTracingSlots,
                                                                           /*use_get*/ false,
                                                                           /*use_set*/ false)};

    const auto serialized_unit{unit.Serialize()};

    LolaFieldInstanceDeployment reconstructed_unit{serialized_unit};

    ExpectLolaFieldInstanceDeploymentObjectsEqual(reconstructed_unit, unit);
}

TEST_F(LolaFieldInstanceDeploymentFixture, UseGetIfAvailableIsTrueAfterRoundTripSerialisation)
{
    // Given a field deployment with use_get_if_available set to true
    constexpr bool kUseGetIfAvailable{true};
    constexpr bool kUseSetIfAvailable{false};
    const LolaFieldInstanceDeployment unit{MakeLolaFieldInstanceDeployment(kMaxSamples,
                                                                           kMaxSubscribers,
                                                                           kMaxConcurrentAllocations,
                                                                           kEnforceMaxSamples,
                                                                           kNumberOfTracingSlots,
                                                                           kUseGetIfAvailable,
                                                                           kUseSetIfAvailable)};

    // When serialising and deserialising
    const auto serialized_unit{unit.Serialize()};
    const LolaFieldInstanceDeployment reconstructed_unit{serialized_unit};

    // Then use_get_if_available is preserved and use_set_if_available is unaffected
    EXPECT_EQ(reconstructed_unit.use_get_if_available_, kUseGetIfAvailable);
    EXPECT_EQ(reconstructed_unit.use_set_if_available_, kUseSetIfAvailable);
}

TEST_F(LolaFieldInstanceDeploymentFixture, UseSetIfAvailableIsTrueAfterRoundTripSerialisation)
{
    // Given a field deployment with use_set_if_available set to true
    constexpr bool kUseGetIfAvailable{false};
    constexpr bool kUseSetIfAvailable{true};
    const LolaFieldInstanceDeployment unit{MakeLolaFieldInstanceDeployment(kMaxSamples,
                                                                           kMaxSubscribers,
                                                                           kMaxConcurrentAllocations,
                                                                           kEnforceMaxSamples,
                                                                           kNumberOfTracingSlots,
                                                                           kUseGetIfAvailable,
                                                                           kUseSetIfAvailable)};

    // When serialising and deserialising
    const auto serialized_unit{unit.Serialize()};
    const LolaFieldInstanceDeployment reconstructed_unit{serialized_unit};

    // Then use_set_if_available is preserved and use_get_if_available is unaffected
    EXPECT_EQ(reconstructed_unit.use_get_if_available_, kUseGetIfAvailable);
    EXPECT_EQ(reconstructed_unit.use_set_if_available_, kUseSetIfAvailable);
}

TEST_F(LolaFieldInstanceDeploymentFixture, BothFlagsDefaultToFalseAfterRoundTripSerialisation)
{
    // Given a field deployment constructed without specifying the new boolean flags
    const LolaFieldInstanceDeployment unit{MakeLolaFieldInstanceDeployment()};

    // When serialising and deserialising
    const auto serialized_unit{unit.Serialize()};
    const LolaFieldInstanceDeployment reconstructed_unit{serialized_unit};

    // Then both flags default to false
    EXPECT_FALSE(reconstructed_unit.use_get_if_available_);
    EXPECT_FALSE(reconstructed_unit.use_set_if_available_);
}

TEST_F(LolaFieldInstanceDeploymentFixture, BothFlagsSetToTrueAfterRoundTripSerialisation)
{
    // Given a field deployment with both flags set to true
    constexpr bool kUseGetIfAvailable{true};
    constexpr bool kUseSetIfAvailable{true};
    const LolaFieldInstanceDeployment unit{MakeLolaFieldInstanceDeployment(kMaxSamples,
                                                                           kMaxSubscribers,
                                                                           kMaxConcurrentAllocations,
                                                                           kEnforceMaxSamples,
                                                                           kNumberOfTracingSlots,
                                                                           kUseGetIfAvailable,
                                                                           kUseSetIfAvailable)};

    // When serialising and deserialising
    const auto serialized_unit{unit.Serialize()};
    const LolaFieldInstanceDeployment reconstructed_unit{serialized_unit};

    // Then both flags are preserved as true
    EXPECT_EQ(reconstructed_unit.use_get_if_available_, kUseGetIfAvailable);
    EXPECT_EQ(reconstructed_unit.use_set_if_available_, kUseSetIfAvailable);
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
