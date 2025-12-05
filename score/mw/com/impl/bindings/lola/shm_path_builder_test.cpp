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
#include "score/mw/com/impl/bindings/lola/shm_path_builder.h"

#include "score/mw/com/impl/bindings/lola/i_shm_path_builder.h"
#include "score/mw/com/impl/bindings/lola/methods/proxy_instance_identifier.h"
#include "score/mw/com/impl/configuration/global_configuration.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"
#include "score/mw/com/impl/configuration/quality_type.h"

#include <gtest/gtest.h>

#include <sys/types.h>
#include <cstdint>
#include <limits>
#include <string>
#include <tuple>

namespace score::mw::com::impl::lola
{
namespace
{

constexpr std::uint16_t kServiceId{4660U};

class ShmPathBuilderControlParamaterizedTestFixture
    : public ::testing::TestWithParam<std::tuple<QualityType, LolaServiceInstanceId::InstanceId, std::string>>
{
};

class ShmPathBuilderDataParamaterizedTestFixture
    : public ::testing::TestWithParam<std::tuple<LolaServiceInstanceId::InstanceId, std::string>>
{
};

class ShmPathBuilderMethodParamaterizedTestFixture
    : public ::testing::TestWithParam<
          std::tuple<LolaServiceInstanceId::InstanceId, ProxyInstanceIdentifier, std::string>>
{
};

TEST_P(ShmPathBuilderControlParamaterizedTestFixture, TestBuildingControlChannelShmNamePath)
{
    const auto [quality_type, instance_id, expected_path] = GetParam();

    // Given a ShmPathBuilder
    ShmPathBuilder builder{kServiceId};

    // When creating the control channel shm name
    const auto actual_path = builder.GetControlChannelShmName(instance_id, quality_type);

    // Then the returned path should be equal to the expected path
    EXPECT_EQ(expected_path, actual_path);
}

INSTANTIATE_TEST_SUITE_P(
    ShmPathBuilderControlTests,
    ShmPathBuilderControlParamaterizedTestFixture,
    ::testing::Values(std::make_tuple(QualityType::kASIL_QM,
                                      LolaServiceInstanceId::InstanceId{1},
                                      "/lola-ctl-0000000000004660-00001"),
                      std::make_tuple(QualityType::kASIL_B,
                                      LolaServiceInstanceId::InstanceId{1},
                                      "/lola-ctl-0000000000004660-00001-b"),

                      std::make_tuple(QualityType::kASIL_QM,
                                      LolaServiceInstanceId::InstanceId{43981},
                                      "/lola-ctl-0000000000004660-43981"),
                      std::make_tuple(QualityType::kASIL_B,
                                      LolaServiceInstanceId::InstanceId{43981},
                                      "/lola-ctl-0000000000004660-43981-b"),

                      std::make_tuple(QualityType::kASIL_QM,
                                      LolaServiceInstanceId::InstanceId{std::numeric_limits<std::uint16_t>::max()},
                                      "/lola-ctl-0000000000004660-65535"),
                      std::make_tuple(QualityType::kASIL_B,
                                      LolaServiceInstanceId::InstanceId{std::numeric_limits<std::uint16_t>::max()},
                                      "/lola-ctl-0000000000004660-65535-b")));

TEST(ShmPathBuilderControlDeathTests, GetControlChannelShmNameDiesWithInvalidQualityType)
{
    constexpr LolaServiceInstanceId::InstanceId instance_id{std::numeric_limits<std::uint16_t>::max()};
    constexpr QualityType invalid_quality_type = QualityType::kInvalid;

    // Given a ShmPathBuilder
    ShmPathBuilder builder{kServiceId};

    // When creating the control channel shm name with and invalid quality type
    // Then we expect it to die
    EXPECT_DEATH(builder.GetControlChannelShmName(instance_id, invalid_quality_type), "");
}

TEST_P(ShmPathBuilderDataParamaterizedTestFixture, TestBuildingDataChannelShmNamePath)
{
    const auto [instance_id, expected_path] = GetParam();

    // Given a ShmPathBuilder
    ShmPathBuilder builder{kServiceId};

    // When creating the data channel shm name
    const auto actual_path = builder.GetDataChannelShmName(instance_id);

    // Then the returned path should be equal to the expected path
    EXPECT_EQ(expected_path, actual_path);
}

INSTANTIATE_TEST_SUITE_P(
    ShmPathBuilderDataTests,
    ShmPathBuilderDataParamaterizedTestFixture,
    ::testing::Values(std::make_tuple(LolaServiceInstanceId::InstanceId{1}, "/lola-data-0000000000004660-00001"),
                      std::make_tuple(LolaServiceInstanceId::InstanceId{43981}, "/lola-data-0000000000004660-43981"),
                      std::make_tuple(LolaServiceInstanceId::InstanceId{std::numeric_limits<std::uint16_t>::max()},
                                      "/lola-data-0000000000004660-65535")));

TEST_P(ShmPathBuilderMethodParamaterizedTestFixture, TestBuildingMethodChannelShmNamePath)
{
    const auto [instance_id, proxy_instance_identifier, expected_path] = GetParam();

    // Given a ShmPathBuilder
    ShmPathBuilder builder{kServiceId};

    // When creating the method channel shm name
    const auto actual_path = builder.GetMethodChannelShmName(instance_id, proxy_instance_identifier);

    // Then the returned path should be equal to the expected path
    EXPECT_EQ(expected_path, actual_path);
}

INSTANTIATE_TEST_SUITE_P(
    ShmPathBuilderMethodTests,
    ShmPathBuilderMethodParamaterizedTestFixture,
    ::testing::Values(std::make_tuple(LolaServiceInstanceId::InstanceId{1},
                                      ProxyInstanceIdentifier{2U, 3U},
                                      "/lola-methods-0000000000004660-00001-00002-00003"),

                      std::make_tuple(LolaServiceInstanceId::InstanceId{43981},
                                      ProxyInstanceIdentifier{12345U, 56789U},
                                      "/lola-methods-0000000000004660-43981-12345-56789"),

                      std::make_tuple(LolaServiceInstanceId::InstanceId{std::numeric_limits<std::uint16_t>::max()},
                                      ProxyInstanceIdentifier{
                                          32768U,
                                          std::numeric_limits<ProxyInstanceIdentifier::ProxyInstanceCounter>::max()},
                                      "/lola-methods-0000000000004660-65535-32768-65535")));

}  // namespace
}  // namespace score::mw::com::impl::lola
