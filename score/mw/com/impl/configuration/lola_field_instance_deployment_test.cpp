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
    const LolaFieldInstanceDeployment unit{MakeLolaFieldInstanceDeployment(kMaxSamples,
                                                                           kMaxSubscribers,
                                                                           kMaxConcurrentAllocations,
                                                                           kEnforceMaxSamples,
                                                                           kNumberOfTracingSlots,
                                                                           /*use_get*/ true,
                                                                           /*use_set*/ false)};

    // When serialising and deserialising
    const auto serialized_unit{unit.Serialize()};
    const LolaFieldInstanceDeployment reconstructed_unit{serialized_unit};

    // Then use_get_if_available is preserved and use_set_if_available is unaffected
    EXPECT_TRUE(reconstructed_unit.use_get_if_available_);
    EXPECT_FALSE(reconstructed_unit.use_set_if_available_);
}

TEST_F(LolaFieldInstanceDeploymentFixture, UseSetIfAvailableIsTrueAfterRoundTripSerialisation)
{
    // Given a field deployment with use_set_if_available set to true
    const LolaFieldInstanceDeployment unit{MakeLolaFieldInstanceDeployment(kMaxSamples,
                                                                           kMaxSubscribers,
                                                                           kMaxConcurrentAllocations,
                                                                           kEnforceMaxSamples,
                                                                           kNumberOfTracingSlots,
                                                                           /*use_get*/ false,
                                                                           /*use_set*/ true)};

    // When serialising and deserialising
    const auto serialized_unit{unit.Serialize()};
    const LolaFieldInstanceDeployment reconstructed_unit{serialized_unit};

    // Then use_set_if_available is preserved and use_get_if_available is unaffected
    EXPECT_FALSE(reconstructed_unit.use_get_if_available_);
    EXPECT_TRUE(reconstructed_unit.use_set_if_available_);
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

TEST_F(LolaFieldInstanceDeploymentFixture, UseGetAndUseSetFlagsAreIndependentOfEachOther)
{
    // Given four deployments covering all flag combinations
    const LolaFieldInstanceDeployment both_false{MakeLolaFieldInstanceDeployment(kMaxSamples,
                                                                                 kMaxSubscribers,
                                                                                 kMaxConcurrentAllocations,
                                                                                 kEnforceMaxSamples,
                                                                                 kNumberOfTracingSlots,
                                                                                 false,
                                                                                 false)};
    const LolaFieldInstanceDeployment get_only{MakeLolaFieldInstanceDeployment(kMaxSamples,
                                                                               kMaxSubscribers,
                                                                               kMaxConcurrentAllocations,
                                                                               kEnforceMaxSamples,
                                                                               kNumberOfTracingSlots,
                                                                               true,
                                                                               false)};
    const LolaFieldInstanceDeployment set_only{MakeLolaFieldInstanceDeployment(kMaxSamples,
                                                                               kMaxSubscribers,
                                                                               kMaxConcurrentAllocations,
                                                                               kEnforceMaxSamples,
                                                                               kNumberOfTracingSlots,
                                                                               false,
                                                                               true)};
    const LolaFieldInstanceDeployment both_true{MakeLolaFieldInstanceDeployment(kMaxSamples,
                                                                                kMaxSubscribers,
                                                                                kMaxConcurrentAllocations,
                                                                                kEnforceMaxSamples,
                                                                                kNumberOfTracingSlots,
                                                                                true,
                                                                                true)};

    // Then each combination holds exactly the values set, independently
    EXPECT_FALSE(both_false.use_get_if_available_);
    EXPECT_FALSE(both_false.use_set_if_available_);

    EXPECT_TRUE(get_only.use_get_if_available_);
    EXPECT_FALSE(get_only.use_set_if_available_);

    EXPECT_FALSE(set_only.use_get_if_available_);
    EXPECT_TRUE(set_only.use_set_if_available_);

    EXPECT_TRUE(both_true.use_get_if_available_);
    EXPECT_TRUE(both_true.use_set_if_available_);
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
