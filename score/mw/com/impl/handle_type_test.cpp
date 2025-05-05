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
#include "score/mw/com/impl/handle_type.h"

#include <gtest/gtest.h>
#include <unordered_map>

namespace score::mw::com::impl
{
namespace
{

const auto kInstanceSpecifier = InstanceSpecifier::Create("/my_dummy_specifier").value();
const auto kInstanceSpecifier2 = InstanceSpecifier::Create("/my_dummy_specifier2").value();
const auto kService1 = make_ServiceIdentifierType("/bla/blub/service1", 1U, 0U);
const auto kService2 = make_ServiceIdentifierType("/bla/blub/service2", 1U, 0U);
const ServiceTypeDeployment kTestTypeDeployment{LolaServiceTypeDeployment{1U}};
const ServiceTypeDeployment kTestTypeDeployment2{LolaServiceTypeDeployment{2U}};
const LolaServiceInstanceId kLolaInstanceId{16U};
const LolaServiceInstanceId kLolaInstanceId2{15U};
LolaServiceInstanceDeployment kLolaServiceInstanceDeployment{kLolaInstanceId};
LolaServiceInstanceDeployment kLolaServiceInstanceDeployment2{kLolaInstanceId2};
const SomeIpServiceInstanceId kSomeIpInstanceId{16U};
const SomeIpServiceInstanceId kSomeIpInstanceId2{15U};
const SomeIpServiceInstanceDeployment kSomeIpServiceInstanceDeployment{kSomeIpInstanceId};
const SomeIpServiceInstanceDeployment kSomeIpServiceInstanceDeployment2{kSomeIpInstanceId2};
const ServiceInstanceDeployment kServiceInstanceDeployment1{kService1,
                                                            kLolaServiceInstanceDeployment,
                                                            QualityType::kASIL_QM,
                                                            kInstanceSpecifier};
const ServiceInstanceDeployment kServiceInstanceDeployment2{kService2,
                                                            kLolaServiceInstanceDeployment2,
                                                            QualityType::kASIL_QM,
                                                            kInstanceSpecifier2};

const ServiceInstanceDeployment kService1InstanceDeploymentNoInstanceId{kService1,
                                                                        LolaServiceInstanceDeployment{},
                                                                        QualityType::kASIL_QM,
                                                                        kInstanceSpecifier};
const ServiceInstanceDeployment kService2InstanceDeploymentNoInstanceId{kService2,
                                                                        LolaServiceInstanceDeployment{},
                                                                        QualityType::kASIL_QM,
                                                                        kInstanceSpecifier2};

TEST(HandleTypeTest, CopyAssignableAndCopyConstructible)
{
    RecordProperty("Verifies", "SCR-14116410");
    RecordProperty("Description", "Checks CopyAssignment operator and CopyConstructor");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(std::is_copy_assignable<HandleType>::value, "Is not copy assignable");
    static_assert(std::is_copy_constructible<HandleType>::value, "Is not copy constructible");
}

TEST(HandleTypeTest, MoveAssignableAndMoveConstructible)
{
    RecordProperty("Verifies", "SCR-14116410");
    RecordProperty("Description", "Checks CopyAssignment operator and CopyConstructor");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(std::is_move_assignable<HandleType>::value, "Is not copy assignable");
    static_assert(std::is_move_constructible<HandleType>::value, "Is not copy constructible");
}

TEST(HandleTypeTest, CanUseAsKeyInMap)
{
    const auto instanceIdentifier = make_InstanceIdentifier(kServiceInstanceDeployment1, kTestTypeDeployment);
    const auto unit = make_HandleType(instanceIdentifier);

    std::unordered_map<HandleType, int> my_map{std::make_pair(unit, 10)};
}

TEST(HandleTypeWithProvidedInstanceIdTest, CanBeCopiedAndEqualCompared)
{
    RecordProperty("Verifies", "SCR-14116410");
    RecordProperty("Description", "Checks CopyAssignment operator and EqualComparableOperator");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const auto instanceIdentifier =
        make_InstanceIdentifier(kService1InstanceDeploymentNoInstanceId, kTestTypeDeployment);
    ServiceInstanceId service_instance_id{LolaServiceInstanceId{16U}};
    const auto unit = make_HandleType(instanceIdentifier, service_instance_id);
    const auto unitCopy = unit;

    ASSERT_EQ(unit, unitCopy);
}

TEST(HandleTypeWithProvidedInstanceIdTest, LessCompareable)
{
    RecordProperty("Verifies", "SCR-14116410");
    RecordProperty("Description", "Checks LessComparableOperator");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const auto instance = make_InstanceIdentifier(kService2InstanceDeploymentNoInstanceId, kTestTypeDeployment);

    const auto lessInstance = make_InstanceIdentifier(kService1InstanceDeploymentNoInstanceId, kTestTypeDeployment);

    ServiceInstanceId service_instance_id{LolaServiceInstanceId{16U}};
    ServiceInstanceId less_service_instance_id{LolaServiceInstanceId{15U}};

    const auto unit = make_HandleType(instance, service_instance_id);
    const auto less = make_HandleType(lessInstance, less_service_instance_id);

    ASSERT_LT(less, unit);
}

TEST(HandleTypeWithProvidedInstanceIdTest, CanGetInstance)
{
    RecordProperty("Verifies", "SCR-14116410");
    RecordProperty("Description", "Checks if the underlying instance is correctly retrieved.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const auto instanceIdentifier =
        make_InstanceIdentifier(kService1InstanceDeploymentNoInstanceId, kTestTypeDeployment);
    ServiceInstanceId service_instance_id{LolaServiceInstanceId{16U}};

    const auto& unit = make_HandleType(instanceIdentifier, service_instance_id);

    ASSERT_EQ(unit.GetInstanceIdentifier(), instanceIdentifier);
}

TEST(HandleTypeWithProvidedInstanceIdTest, CreatingHandleTypeWithInstanceIdStoresProvidedInstanceId)
{
    LolaServiceInstanceId config_instance_id{10U};
    LolaServiceInstanceId provided_instance_id{15U};
    const ServiceInstanceDeployment service_instance_deployment{
        kService1, LolaServiceInstanceDeployment{config_instance_id}, QualityType::kASIL_QM, kInstanceSpecifier};

    const auto instanceIdentifier = make_InstanceIdentifier(service_instance_deployment, kTestTypeDeployment);
    const auto unit = make_HandleType(instanceIdentifier, ServiceInstanceId{provided_instance_id});

    ASSERT_EQ(unit.GetInstanceId(), ServiceInstanceId{provided_instance_id});
}

TEST(HandleTypeWithProvidedInstanceIdTest, HashesOfTheSameHandleTypeAreEqual)
{
    ServiceInstanceId service_instance_id{LolaServiceInstanceId{16U}};
    const auto instanceIdentifier =
        make_InstanceIdentifier(kService1InstanceDeploymentNoInstanceId, kTestTypeDeployment);

    // Given 2 handle types with containing the same values
    const auto& unit = make_HandleType(instanceIdentifier, service_instance_id);
    const auto& unit_2 = make_HandleType(instanceIdentifier, service_instance_id);

    // When calculating the hash of the handle types
    auto hash_value = std::hash<HandleType>{}(unit);
    auto hash_value_2 = std::hash<HandleType>{}(unit_2);

    // Then the hash value should be equal
    ASSERT_EQ(hash_value, hash_value_2);
}

TEST(HandleTypeWithInstanceIdFromConfigTest, CanBeCopiedAndEqualCompared)
{
    RecordProperty("Verifies", "SCR-14116410");
    RecordProperty("Description", "Checks CopyAssignment operator and EqualComparableOperator");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const auto instanceIdentifier = make_InstanceIdentifier(kServiceInstanceDeployment1, kTestTypeDeployment);
    const auto unit = make_HandleType(instanceIdentifier);
    const auto unitCopy = unit;

    ASSERT_EQ(unit, unitCopy);
}

TEST(HandleTypeWithInstanceIdFromConfigTest, LessCompareable)
{
    RecordProperty("Verifies", "SCR-14116410");
    RecordProperty("Description", "Checks LessComparableOperator");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const auto lessInstance = make_InstanceIdentifier(kServiceInstanceDeployment1, kTestTypeDeployment);
    const auto instance = make_InstanceIdentifier(kServiceInstanceDeployment2, kTestTypeDeployment);
    ASSERT_LT(lessInstance, instance);

    const auto unit = make_HandleType(instance);
    const auto less = make_HandleType(lessInstance);

    ASSERT_LT(less, unit);
}

TEST(HandleTypeWithInstanceIdFromConfigTest, CanGetInstance)
{
    RecordProperty("Verifies", "SCR-14116410");
    RecordProperty("Description", "Checks if the underlying instance is correctly retrieved.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const auto instanceIdentifier = make_InstanceIdentifier(kServiceInstanceDeployment1, kTestTypeDeployment);
    const auto& unit = make_HandleType(instanceIdentifier);

    ASSERT_EQ(unit.GetInstanceIdentifier(), instanceIdentifier);
}

TEST(HandleTypeWithInstanceIdFromConfigTest, CreatingHandleTypeWithoutInstanceIdStoresInstanceIdFromInstanceIdentifier)
{
    LolaServiceInstanceId config_instance_id{10U};
    const ServiceInstanceDeployment service_instance_deployment{
        kService1, LolaServiceInstanceDeployment{config_instance_id}, QualityType::kASIL_QM, kInstanceSpecifier};

    const auto instanceIdentifier = make_InstanceIdentifier(service_instance_deployment, kTestTypeDeployment);
    const auto unit = make_HandleType(instanceIdentifier);

    ASSERT_EQ(unit.GetInstanceId(), ServiceInstanceId{config_instance_id});
}

TEST(HandleTypeWithInstanceIdFromConfigDeathTest,
     CreatingHandleTypeWithoutProvidedInstanceIdOrInstanceIdInConfigurationTerminates)
{
    LolaServiceInstanceId config_instance_id{10U};
    const ServiceInstanceDeployment service_instance_deployment{
        kService1, LolaServiceInstanceDeployment{}, QualityType::kASIL_QM, kInstanceSpecifier};

    const auto instance_identifier_no_instance_id =
        make_InstanceIdentifier(kService1InstanceDeploymentNoInstanceId, kTestTypeDeployment);
    EXPECT_DEATH(make_HandleType(instance_identifier_no_instance_id), ".*");
}

TEST(HandleTypeWithInstanceIdFromConfigTest, HashesOfTheSameHandleTypeAreEqual)
{
    const auto instanceIdentifier = make_InstanceIdentifier(kServiceInstanceDeployment1, kTestTypeDeployment);
    const auto instanceIdentifier_2 = make_InstanceIdentifier(kServiceInstanceDeployment1, kTestTypeDeployment);

    // Given 2 handle types with containing the same values
    const auto& unit = make_HandleType(instanceIdentifier);
    const auto& unit_2 = make_HandleType(instanceIdentifier_2);

    // When calculating the hash of the handle types
    auto hash_value = std::hash<HandleType>{}(unit);
    auto hash_value_2 = std::hash<HandleType>{}(unit_2);

    // Then the hash value should be equal
    ASSERT_EQ(hash_value, hash_value_2);
}

TEST(HandleTypeHashTest, CanHash)
{
    const auto instanceIdentifier = make_InstanceIdentifier(kServiceInstanceDeployment1, kTestTypeDeployment);

    // Given a handle type
    const auto& unit = make_HandleType(instanceIdentifier);

    // When calculating the hash of a handle type
    auto hash_value = std::hash<HandleType>{}(unit);

    // Then the hash value should be non-zero
    ASSERT_NE(hash_value, 0);
}

class HandleTypeHashFixture : public ::testing::TestWithParam<std::array<HandleType, 2>>
{
};

TEST_P(HandleTypeHashFixture, HashesOfDifferentHandleTypesAreNotEqual)
{
    const auto handle_types = GetParam();

    // Given 2 handle types containing different values
    const auto handle_type = handle_types.at(0);
    const auto handle_type_2 = handle_types.at(1);

    // When calculating the hash of the handle types
    auto hash_value = std::hash<HandleType>{}(handle_type);
    auto hash_value_2 = std::hash<HandleType>{}(handle_type_2);

    // Then the hash value should be different
    ASSERT_NE(hash_value, hash_value_2);
}

// Test that each element that should be used in the hashing algorithm is used by changing them one at a time.
INSTANTIATE_TEST_CASE_P(
    HandleTypeHashDifferentKeys,
    HandleTypeHashFixture,
    ::testing::Values(
        std::array<HandleType, 2>{
            make_HandleType(make_InstanceIdentifier(ServiceInstanceDeployment{kService1,
                                                                              kLolaServiceInstanceDeployment,
                                                                              QualityType::kASIL_QM,
                                                                              kInstanceSpecifier},
                                                    kTestTypeDeployment)),
            make_HandleType(make_InstanceIdentifier(ServiceInstanceDeployment{kService1,
                                                                              kLolaServiceInstanceDeployment,
                                                                              QualityType::kASIL_QM,
                                                                              kInstanceSpecifier},
                                                    kTestTypeDeployment2))},

        std::array<HandleType, 2>{
            make_HandleType(make_InstanceIdentifier(ServiceInstanceDeployment{kService1,
                                                                              kLolaServiceInstanceDeployment,
                                                                              QualityType::kASIL_QM,
                                                                              kInstanceSpecifier},
                                                    kTestTypeDeployment)),
            make_HandleType(make_InstanceIdentifier(ServiceInstanceDeployment{kService1,
                                                                              kLolaServiceInstanceDeployment2,
                                                                              QualityType::kASIL_QM,
                                                                              kInstanceSpecifier},
                                                    kTestTypeDeployment))},

        std::array<HandleType, 2>{
            make_HandleType(make_InstanceIdentifier(ServiceInstanceDeployment{kService1,
                                                                              kSomeIpServiceInstanceDeployment,
                                                                              QualityType::kASIL_QM,
                                                                              kInstanceSpecifier},
                                                    kTestTypeDeployment)),
            make_HandleType(make_InstanceIdentifier(ServiceInstanceDeployment{kService1,
                                                                              kSomeIpServiceInstanceDeployment2,
                                                                              QualityType::kASIL_QM,
                                                                              kInstanceSpecifier},
                                                    kTestTypeDeployment))},

        std::array<HandleType, 2>{
            make_HandleType(make_InstanceIdentifier(ServiceInstanceDeployment{kService1,
                                                                              kSomeIpServiceInstanceDeployment,
                                                                              QualityType::kASIL_QM,
                                                                              kInstanceSpecifier},
                                                    kTestTypeDeployment)),
            make_HandleType(make_InstanceIdentifier(ServiceInstanceDeployment{kService1,
                                                                              kLolaServiceInstanceDeployment,
                                                                              QualityType::kASIL_QM,
                                                                              kInstanceSpecifier},
                                                    kTestTypeDeployment))}));

}  // namespace
}  // namespace score::mw::com::impl
