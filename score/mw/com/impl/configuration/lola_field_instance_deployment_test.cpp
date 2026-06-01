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

#include <utility>

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

enum class Getter
{
    ENABLED,
    DISABLED
};

enum class Setter
{
    ENABLED,
    DISABLED
};

constexpr bool IsEnabled(const Getter getter) noexcept
{
    return getter == Getter::ENABLED;
}

constexpr bool IsEnabled(const Setter setter) noexcept
{
    return setter == Setter::ENABLED;
}
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

class LolaFieldInstanceDeploymentUseGetSetPairParamFixture
    : public LolaFieldInstanceDeploymentFixture,
      public ::testing::WithParamInterface<std::pair<Getter, Setter>>
{
};

TEST_P(LolaFieldInstanceDeploymentUseGetSetPairParamFixture,
       UseGetAndUseSetFlagsArePreservedAfterRoundTripSerialisation)
{
    const auto [getter, setter] = GetParam();
    const auto use_get_if_available = IsEnabled(getter);
    const auto use_set_if_available = IsEnabled(setter);

    const LolaFieldInstanceDeployment unit{MakeLolaFieldInstanceDeployment(kMaxSamples,
                                                                           kMaxSubscribers,
                                                                           kMaxConcurrentAllocations,
                                                                           kEnforceMaxSamples,
                                                                           kNumberOfTracingSlots,
                                                                           use_get_if_available,
                                                                           use_set_if_available)};

    const auto serialized_unit{unit.Serialize()};
    const LolaFieldInstanceDeployment reconstructed_unit{serialized_unit};

    EXPECT_EQ(reconstructed_unit.use_get_if_available_, std::optional<bool>{use_get_if_available});
    EXPECT_EQ(reconstructed_unit.use_set_if_available_, std::optional<bool>{use_set_if_available});
}

INSTANTIATE_TEST_SUITE_P(UseGetAndSetCombinations,
                         LolaFieldInstanceDeploymentUseGetSetPairParamFixture,
                         ::testing::Values(std::pair<Getter, Setter>{Getter::ENABLED, Setter::ENABLED},
                                           std::pair<Getter, Setter>{Getter::ENABLED, Setter::DISABLED},
                                           std::pair<Getter, Setter>{Getter::DISABLED, Setter::ENABLED},
                                           std::pair<Getter, Setter>{Getter::DISABLED, Setter::DISABLED}));

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
