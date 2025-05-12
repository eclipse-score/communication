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
#include "score/mw/com/impl/configuration/service_instance_deployment.h"

#include "score/mw/com/impl/configuration/test/configuration_test_resources.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <variant>

namespace score::mw::com::impl
{
namespace
{

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

const ServiceIdentifierType kDummyService{make_ServiceIdentifierType("foo", 1U, 0U)};
const InstanceSpecifier kInstanceSpecifier{InstanceSpecifier::Create("my_dummy_instance_specifier").value()};

TEST(ServiceInstanceDeploymentTest, DifferentBindingsAreNotCompatible)
{
    ASSERT_FALSE(areCompatible(
        ServiceInstanceDeployment{
            kDummyService, LolaServiceInstanceDeployment{}, QualityType::kASIL_QM, kInstanceSpecifier},
        ServiceInstanceDeployment{
            kDummyService, SomeIpServiceInstanceDeployment{}, QualityType::kASIL_QM, kInstanceSpecifier}));
}

TEST(ServiceInstanceDeploymentTest, DifferentShmBindingsAreCompatible)
{
    ASSERT_TRUE(areCompatible(
        ServiceInstanceDeployment{
            kDummyService, LolaServiceInstanceDeployment{}, QualityType::kASIL_QM, kInstanceSpecifier},
        ServiceInstanceDeployment{
            kDummyService, LolaServiceInstanceDeployment{}, QualityType::kASIL_QM, kInstanceSpecifier}));
}

TEST(ServiceInstanceDeploymentTest, DifferentSomeIpBindingsAreCompatible)
{
    ASSERT_TRUE(areCompatible(
        ServiceInstanceDeployment{
            kDummyService, SomeIpServiceInstanceDeployment{}, QualityType::kASIL_QM, kInstanceSpecifier},
        ServiceInstanceDeployment{
            kDummyService, SomeIpServiceInstanceDeployment{}, QualityType::kASIL_QM, kInstanceSpecifier}));
}

TEST(ServiceInstanceDeploymentTest, Equality)
{
    const auto unit1 = ServiceInstanceDeployment{
        kDummyService, SomeIpServiceInstanceDeployment{}, QualityType::kASIL_QM, kInstanceSpecifier};
    const auto unit2 = ServiceInstanceDeployment{
        kDummyService, SomeIpServiceInstanceDeployment{}, QualityType::kASIL_QM, kInstanceSpecifier};
    EXPECT_EQ(unit1, unit2);
}

TEST(ServiceInstanceDeploymentTest, Less)
{
    const auto unit1 = ServiceInstanceDeployment{
        kDummyService, SomeIpServiceInstanceDeployment{}, QualityType::kASIL_QM, kInstanceSpecifier};
    const auto unit2 = ServiceInstanceDeployment{
        kDummyService, SomeIpServiceInstanceDeployment{}, QualityType::kASIL_QM, kInstanceSpecifier};
    EXPECT_FALSE(unit1 < unit2);

    const auto unit3 = ServiceInstanceDeployment{
        kDummyService, LolaServiceInstanceDeployment{}, QualityType::kASIL_QM, kInstanceSpecifier};
    const auto unit4 = ServiceInstanceDeployment{
        kDummyService, LolaServiceInstanceDeployment{}, QualityType::kASIL_QM, kInstanceSpecifier};
    EXPECT_FALSE(unit3 < unit4);
}

using ServiceInstanceDeploymentFixture = ConfigurationStructsFixture;
TEST_F(ServiceInstanceDeploymentFixture, CanConstructFromLolaServiceInstanceDeployment)
{
    ServiceInstanceDeployment unit{
        kDummyService, MakeLolaServiceInstanceDeployment(), QualityType::kASIL_QM, kInstanceSpecifier};

    ASSERT_EQ(unit.asilLevel_, QualityType::kASIL_QM);
    ExpectServiceIdentifierTypeObjectsEqual(unit.service_, kDummyService);
    const auto* const binding_ptr = std::get_if<LolaServiceInstanceDeployment>(&unit.bindingInfo_);
    ASSERT_NE(binding_ptr, nullptr);
}

TEST_F(ServiceInstanceDeploymentFixture, CanConstructFromSomeIpServiceInstanceDeployment)
{
    ServiceInstanceDeployment unit{
        kDummyService, SomeIpServiceInstanceDeployment{16U}, QualityType::kASIL_QM, kInstanceSpecifier};

    ASSERT_EQ(unit.asilLevel_, QualityType::kASIL_QM);
    ExpectServiceIdentifierTypeObjectsEqual(unit.service_, kDummyService);
    const auto* const binding_ptr = std::get_if<SomeIpServiceInstanceDeployment>(&unit.bindingInfo_);
    ASSERT_NE(binding_ptr, nullptr);
}

TEST_F(ServiceInstanceDeploymentFixture, CanConstructFromBlankInstanceDeployment)
{
    ServiceInstanceDeployment unit{kDummyService, score::cpp::blank{}, QualityType::kASIL_QM, kInstanceSpecifier};

    ASSERT_EQ(unit.asilLevel_, QualityType::kASIL_QM);
    ExpectServiceIdentifierTypeObjectsEqual(unit.service_, kDummyService);
    const auto* const binding_ptr = std::get_if<score::cpp::blank>(&unit.bindingInfo_);
    ASSERT_NE(binding_ptr, nullptr);
}

TEST_F(ServiceInstanceDeploymentFixture, CanCreateFromSerializedLolaObject)
{
    LolaServiceInstanceDeployment lola_service_instance_deployment{MakeLolaServiceInstanceDeployment()};

    ServiceInstanceDeployment unit{
        kDummyService, lola_service_instance_deployment, QualityType::kASIL_B, kInstanceSpecifier};

    const auto serialized_unit{unit.Serialize()};

    ServiceInstanceDeployment reconstructed_unit{serialized_unit};

    ExpectServiceInstanceDeploymentObjectsEqual(reconstructed_unit, unit);
}

TEST_F(ServiceInstanceDeploymentFixture, CanCreateFromSerializedSomeIpObject)
{
    const std::uint16_t instance_id{123U};

    SomeIpServiceInstanceDeployment someip_service_instance_deployment{instance_id};

    ServiceInstanceDeployment unit{
        kDummyService, someip_service_instance_deployment, QualityType::kASIL_B, kInstanceSpecifier};

    const auto serialized_unit{unit.Serialize()};

    ServiceInstanceDeployment reconstructed_unit{serialized_unit};

    ExpectServiceInstanceDeploymentObjectsEqual(reconstructed_unit, unit);
}

TEST_F(ServiceInstanceDeploymentFixture, CanCreateFromSerializedBlankObject)
{
    ServiceInstanceDeployment unit{kDummyService, score::cpp::blank{}, QualityType::kASIL_B, kInstanceSpecifier};

    const auto serialized_unit{unit.Serialize()};

    ServiceInstanceDeployment reconstructed_unit{serialized_unit};
}

TEST(ServiceInstanceDeploymentDeathTest, CreatingFromSerializedObjectWithMismatchedSerializationVersionTerminates)
{
    const ServiceInstanceDeployment unit{kDummyService,
                                         LolaServiceInstanceDeployment{MakeLolaServiceInstanceDeployment()},
                                         QualityType::kASIL_QM,
                                         kInstanceSpecifier};

    const auto serialization_version_key = "serializationVersion";
    const std::uint32_t invalid_serialization_version = ServiceInstanceDeployment::serializationVersion + 1;

    auto serialized_unit{unit.Serialize()};
    auto it = serialized_unit.find(serialization_version_key);
    ASSERT_NE(it, serialized_unit.end());
    it->second = json::Any{invalid_serialization_version};

    EXPECT_DEATH(ServiceInstanceDeployment reconstructed_unit{serialized_unit}, ".*");
}

TEST(ServiceInstanceDeploymentTest, GetBindingTypeReturnsSomeIpForSomeIpBinding)
{
    // Given a ServiceInstanceDeployment
    const auto unit = ServiceInstanceDeployment{
        kDummyService, SomeIpServiceInstanceDeployment{16U}, QualityType::kASIL_QM, kInstanceSpecifier};

    // When getting the binding type
    // Then it should return BindingType::kSomeIp
    EXPECT_EQ(unit.GetBindingType(), BindingType::kSomeIp);
}

TEST(ServiceInstanceDeploymentTest, GetBindingTypeReturnsFakeForBlankBinding)
{
    // Given a ServiceInstanceDeployment
    const auto unit = ServiceInstanceDeployment{kDummyService, score::cpp::blank{}, QualityType::kASIL_QM, kInstanceSpecifier};

    // When getting the binding type
    // Then it should return BindingType::kFake
    EXPECT_EQ(unit.GetBindingType(), BindingType::kFake);
}

TEST(ServiceInstanceDeploymentTest, LessOperatorWhenOnlyLhsHasSomeIpBinding)
{
    // Given the ServiceInstanceDeployment
    const auto lhs = ServiceInstanceDeployment{
        kDummyService, SomeIpServiceInstanceDeployment{16U}, QualityType::kASIL_QM, kInstanceSpecifier};
    const auto rhs = ServiceInstanceDeployment{
        kDummyService, LolaServiceInstanceDeployment{}, QualityType::kASIL_QM, kInstanceSpecifier};

    // When comparing lhs and rhs using operator<
    // Then comparison should be based on their asilLevel_
    EXPECT_EQ(lhs < rhs, lhs.asilLevel_ < rhs.asilLevel_);
}

TEST(ServiceInstanceDeploymentTest, LessOperatorWhenOnlyRhsHasSomeIpBinding)
{
    // Given the ServiceInstanceDeployment
    const auto lhs = ServiceInstanceDeployment{
        kDummyService, LolaServiceInstanceDeployment{}, QualityType::kASIL_QM, kInstanceSpecifier};
    const auto rhs = ServiceInstanceDeployment{
        kDummyService, SomeIpServiceInstanceDeployment{16U}, QualityType::kASIL_QM, kInstanceSpecifier};

    // When comparing lhs and rhs using operator<
    // Then comparison should be based on their asilLevel_
    EXPECT_EQ(lhs < rhs, lhs.asilLevel_ < rhs.asilLevel_);
}

TEST(ServiceInstanceDeploymentTest, LessOperatorWhenNeitherHasSomeIpBinding)
{
    // Given the ServiceInstanceDeployment
    const auto lhs = ServiceInstanceDeployment{
        kDummyService, LolaServiceInstanceDeployment{}, QualityType::kASIL_QM, kInstanceSpecifier};
    const auto rhs = ServiceInstanceDeployment{kDummyService, score::cpp::blank{}, QualityType::kASIL_QM, kInstanceSpecifier};

    // When comparing lhs and rhs using operator<
    // Then comparison should be based on their asilLevel_
    EXPECT_EQ(lhs < rhs, lhs.asilLevel_ < rhs.asilLevel_);
}

}  // namespace
}  // namespace score::mw::com::impl
