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
#include "score/mw/com/impl/instance_identifier.h"

#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/configuration/test/configuration_test_resources.h"

#include "score/json/json_writer.h"

#include <score/utility.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <type_traits>

namespace score::mw::com::impl
{

class InstanceIdentifierAttorney
{
  public:
    static void SetConfiguration(Configuration* const configuration) noexcept
    {
        InstanceIdentifier::configuration_ = configuration;
    }
};

class ConfigurationGuard
{
  public:
    ConfigurationGuard()
    {
        InstanceIdentifierAttorney::SetConfiguration(&configuration);
    }
    ~ConfigurationGuard()
    {
        InstanceIdentifierAttorney::SetConfiguration(nullptr);
    }

  private:
    Configuration configuration{Configuration::ServiceTypeDeployments{},
                                Configuration::ServiceInstanceDeployments{},
                                GlobalConfiguration{},
                                TracingConfiguration{}};
};

namespace
{

using ::testing::Return;
using ::testing::StrEq;

const auto kServiceTypeName1 = "/bla/blub/service1";
const auto kServiceTypeName2 = "/bla/blub/service2";
const auto kInstanceSpecifier1 = InstanceSpecifier::Create(kServiceTypeName1).value();
const auto kInstanceSpecifier2 = InstanceSpecifier::Create(kServiceTypeName2).value();
const auto kService1 = make_ServiceIdentifierType(kServiceTypeName1);
const auto kService2 = make_ServiceIdentifierType(kServiceTypeName2);
const LolaServiceInstanceDeployment kServiceInstanceDeployment1{LolaServiceInstanceId{16U}};
const LolaServiceInstanceDeployment kServiceInstanceDeployment2{LolaServiceInstanceId{17U}};
const ServiceTypeDeployment kTestTypeDeployment1{score::cpp::blank{}};
const ServiceTypeDeployment kTestTypeDeployment2{LolaServiceTypeDeployment{18U}};

TEST(InstanceIdentifierTest, Copyable)
{
    RecordProperty("Verifies", "SCR-18563675");
    RecordProperty("Description", "Checks copy semantics for InstanceIdentifier");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(std::is_copy_constructible<InstanceIdentifier>::value, "Is not copy constructible");
    static_assert(std::is_copy_assignable<InstanceIdentifier>::value, "Is not copy assignable");
}

TEST(InstanceIdentifier, Moveable)
{
    RecordProperty("Verifies", "SCR-21353341");
    RecordProperty("Description", "Checks move semantics for InstanceIdentifier");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(std::is_move_constructible<InstanceIdentifier>::value, "Is not move constructible");
    static_assert(std::is_move_assignable<InstanceIdentifier>::value, "Is not move assignable");
}

TEST(InstanceIdentifierTest, constructable)
{
    make_InstanceIdentifier({kService1,
                             LolaServiceInstanceDeployment{LolaServiceInstanceId{16U}},
                             QualityType::kASIL_QM,
                             kInstanceSpecifier1},
                            kTestTypeDeployment1);
}

TEST(InstanceIdentifierTest, CanBeCopiedAndEqualCompared)
{
    RecordProperty("Verifies", "SCR-17556907");
    RecordProperty("Description", "Checks CopyAssignment operator and EqualComparableOperator");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const ServiceInstanceDeployment instance_deployment{kService1,
                                                        LolaServiceInstanceDeployment{LolaServiceInstanceId{16U}},
                                                        QualityType::kASIL_QM,
                                                        kInstanceSpecifier1};

    const auto unit = make_InstanceIdentifier(instance_deployment, kTestTypeDeployment1);
    const auto unitCopy = unit;

    ASSERT_EQ(unit, unitCopy);
}

TEST(InstanceIdentifierTest, LessCompareable)
{
    RecordProperty("Verifies", "SCR-17556907");
    RecordProperty("Description", "Checks LessComparableOperator");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const ServiceInstanceDeployment instance_deployment_1{kService1,
                                                          LolaServiceInstanceDeployment{LolaServiceInstanceId{16U}},
                                                          QualityType::kASIL_QM,
                                                          kInstanceSpecifier1};
    const ServiceInstanceDeployment instance_deployment_2{kService2,
                                                          LolaServiceInstanceDeployment{LolaServiceInstanceId{16U}},
                                                          QualityType::kASIL_QM,
                                                          kInstanceSpecifier1};

    const auto unit = make_InstanceIdentifier(instance_deployment_2, kTestTypeDeployment1);
    const auto less = make_InstanceIdentifier(instance_deployment_1, kTestTypeDeployment1);

    ASSERT_LT(less, unit);
}

TEST(InstanceIdentifierTest, LessComparableBasedOnInstanceDeployment)
{
    // Given: Two ServiceInstanceDeployment objects with different ASIL levels QM < B and InstanceId 15 < 16
    const ServiceInstanceDeployment instance_deployment_1{kService1,
                                                          LolaServiceInstanceDeployment{LolaServiceInstanceId{15U}},
                                                          QualityType::kASIL_QM,
                                                          kInstanceSpecifier1};
    const ServiceInstanceDeployment instance_deployment_2{kService1,
                                                          LolaServiceInstanceDeployment{LolaServiceInstanceId{16U}},
                                                          QualityType::kASIL_B,
                                                          kInstanceSpecifier1};

    // When: Creating InstanceIdentifier objects from these deployments
    const InstanceIdentifier less = make_InstanceIdentifier(instance_deployment_1, kTestTypeDeployment1);
    const InstanceIdentifier unit = make_InstanceIdentifier(instance_deployment_2, kTestTypeDeployment1);

    // Then: Verify that the first identifier is less than the second one
    ASSERT_LT(less, unit);
}

template <typename DeploymentT, typename ServiceInstanceIdT>
struct DeploymentPair
{
    using DeploymentType = DeploymentT;
    using ServiceInstanceIdType = ServiceInstanceIdT;
};

using DeploymentTypes = ::testing::Types<DeploymentPair<LolaServiceInstanceDeployment, LolaServiceInstanceId>,
                                         DeploymentPair<SomeIpServiceInstanceDeployment, SomeIpServiceInstanceId>>;

template <typename T>
class InstanceIdentifierViewTypedTest : public ::testing::Test
{
};

TYPED_TEST_SUITE(InstanceIdentifierViewTypedTest, DeploymentTypes, );
TYPED_TEST(InstanceIdentifierViewTypedTest, InstanceIdCorrectlyStored)
{
    // Given: Setup a deployment with a non-empty instance ID
    using DeploymentType = typename TypeParam::DeploymentType;
    using ServiceInstanceIdType = typename TypeParam::ServiceInstanceIdType;
    const ServiceInstanceIdType instanceId{16U};
    const ServiceInstanceDeployment instance_deployment{
        kService1, DeploymentType{instanceId}, QualityType::kASIL_QM, kInstanceSpecifier1};

    // When: Create an identifier from the deployment
    const auto identifier = make_InstanceIdentifier(instance_deployment, kTestTypeDeployment1);

    // Then: Create view and verify that it returns correct instance ID value
    const auto unit = InstanceIdentifierView{identifier};
    ASSERT_TRUE(unit.GetServiceInstanceId().has_value());
    EXPECT_EQ(ServiceInstanceId(instanceId), unit.GetServiceInstanceId().value());
}

TYPED_TEST(InstanceIdentifierViewTypedTest, EmptyInstanceIdCorrectlyStored)
{
    // Given: Setup a deployment with an empty instance ID
    using DeploymentType = typename TypeParam::DeploymentType;
    const ServiceInstanceDeployment instance_deployment{
        kService1, DeploymentType{}, QualityType::kASIL_QM, kInstanceSpecifier1};

    // When: Create an identifier from the deployment
    const auto identifier = make_InstanceIdentifier(instance_deployment, kTestTypeDeployment1);

    // Then: Create view and verify that it returns correct instance ID value
    const auto unit = InstanceIdentifierView{identifier};
    EXPECT_FALSE(unit.GetServiceInstanceId().has_value());
}

TEST(InstanceIdentifierViewTest, ExposeDeploymentInformation)
{
    ServiceInstanceDeployment deployment{kService1,
                                         LolaServiceInstanceDeployment{LolaServiceInstanceId{1U}},
                                         QualityType::kASIL_QM,
                                         kInstanceSpecifier1};
    const auto identifier = make_InstanceIdentifier(deployment, kTestTypeDeployment1);
    const auto unit = InstanceIdentifierView{identifier};

    EXPECT_EQ(unit.GetServiceInstanceDeployment(), deployment);
}

TEST(InstanceIdentifierViewTest, SameInstanceIsCompatible)
{
    ServiceInstanceDeployment deployment{kService1,
                                         LolaServiceInstanceDeployment{LolaServiceInstanceId{1U}},
                                         QualityType::kASIL_QM,
                                         kInstanceSpecifier1};
    const auto identifier = make_InstanceIdentifier(deployment, kTestTypeDeployment1);
    const auto unit = InstanceIdentifierView{identifier};

    EXPECT_TRUE(unit.isCompatibleWith(identifier));
}

TEST(InstanceIdentifierViewTest, SameInstanceViewIsCompatible)
{
    ServiceInstanceDeployment deployment{kService1,
                                         LolaServiceInstanceDeployment{LolaServiceInstanceId{1U}},
                                         QualityType::kASIL_QM,
                                         kInstanceSpecifier1};
    const auto identifier = make_InstanceIdentifier(deployment, kTestTypeDeployment1);
    const auto unit = InstanceIdentifierView{identifier};

    EXPECT_TRUE(unit.isCompatibleWith(unit));
}

using InstanceIdentifierFixture = ConfigurationStructsFixture;
TEST_F(InstanceIdentifierFixture, CanCreateFromSerializedObject)
{
    RecordProperty("Verifies", "SCR-18448357, SCR-18448382");
    RecordProperty("Description",
                   "Checks creating InstanceIdentifier from valid serialized InstanceIdentifier from "
                   "InstanceIdentifier::ToString");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    ConfigurationGuard configuration_guard{};

    const ServiceIdentifierType dummy_service = make_ServiceIdentifierType("foo", 1U, 0U);

    const ServiceInstanceDeployment service_instance_deployment{
        dummy_service, MakeLolaServiceInstanceDeployment(), QualityType::kASIL_B, kInstanceSpecifier1};

    const ServiceTypeDeployment service_type_deployment{MakeLolaServiceTypeDeployment()};

    const auto identifier = make_InstanceIdentifier(service_instance_deployment, service_type_deployment);

    const auto serialized_identifier = std::string_view{identifier.ToString().data(), identifier.ToString().size()};

    const auto reconstructed_identifier_result = InstanceIdentifier::Create(serialized_identifier);
    ASSERT_TRUE(reconstructed_identifier_result.has_value());
    const auto& reconstructed_identifier = reconstructed_identifier_result.value();

    ExpectServiceInstanceDeploymentObjectsEqual(
        InstanceIdentifierView{reconstructed_identifier}.GetServiceInstanceDeployment(),
        InstanceIdentifierView{identifier}.GetServiceInstanceDeployment());
    ExpectServiceTypeDeploymentObjectsEqual(InstanceIdentifierView{reconstructed_identifier}.GetServiceTypeDeployment(),
                                            InstanceIdentifierView{identifier}.GetServiceTypeDeployment());
}

TEST_F(InstanceIdentifierFixture, CreatingFromInvalidSerializedObjectReturnsError)
{
    RecordProperty("Verifies", "SCR-18448357");
    RecordProperty("Description", "Checks creating InstanceIdentifier from invalid serialized InstanceIdentifier");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    ConfigurationGuard configuration_guard{};

    const std::string invalid_serialized_identifier{"invalid identifier"};
    const auto reconstructed_identifier_result = InstanceIdentifier::Create(invalid_serialized_identifier);
    ASSERT_FALSE(reconstructed_identifier_result.has_value());
    EXPECT_EQ(reconstructed_identifier_result.error(), ComErrc::kInvalidInstanceIdentifierString);
}

TEST_F(InstanceIdentifierFixture, CreatingWithoutInitializingConfigurationReturnsError)
{
    const std::string serialized_identifier{""};
    const auto reconstructed_identifier_result = InstanceIdentifier::Create(serialized_identifier);
    ASSERT_FALSE(reconstructed_identifier_result.has_value());
    EXPECT_EQ(reconstructed_identifier_result.error(), ComErrc::kInvalidConfiguration);
}

class InstanceIdentifierUtilityFixture : public ::testing::Test
{
  protected:
    json::Object GenerateInstanceIdentifierJson(const ServiceIdentifierType& identifier_type,
                                                const InstanceSpecifier& instance_specifier)
    {
        const ServiceInstanceDeployment instance_deployment{
            identifier_type, MakeLolaServiceInstanceDeployment(), QualityType::kASIL_QM, instance_specifier};

        const auto unit = make_InstanceIdentifier(instance_deployment, kTestTypeDeployment1);
        return InstanceIdentifierView{unit}.Serialize();
    }

    std::string GenerateSerializedInstanceIdentifier(const ServiceIdentifierType& identifier_type,
                                                     const InstanceSpecifier& instance_specifier)
    {
        const auto buffer = GenerateInstanceIdentifierJson(identifier_type, instance_specifier);
        score::json::JsonWriter writer{};
        return writer.ToBuffer(buffer).value();
    }

    std::unique_ptr<ConfigurationGuard> configuration_guard_{std::make_unique<ConfigurationGuard>()};
};

using InstanceIdentifierDeathTest = InstanceIdentifierUtilityFixture;
TEST_F(InstanceIdentifierDeathTest, DeathOnCreationWithDuplicateServiceIdentifierType)
{
    // Given a serialized InstanceIdentifier
    const auto serialized_string_form = GenerateSerializedInstanceIdentifier(kService1, kInstanceSpecifier1);

    // and that an InstanceIdentifier has been created from the serialized InstanceIdentifier
    score::cpp::ignore = InstanceIdentifier::Create(serialized_string_form);

    // When creating a second InstanceIdentifier with the same ServiceIdentifierType
    // Then the program terminates
    EXPECT_DEATH(InstanceIdentifier::Create(serialized_string_form), ".*");
}

TEST_F(InstanceIdentifierDeathTest, CreatingFromSerializedObjectWithMismatchedSerializationVersionTerminates)
{
    // Given: JSON-serialized InstanceIdentified with correct serialization version
    auto serialized_unit{GenerateInstanceIdentifierJson(kService1, kInstanceSpecifier1)};
    const auto serialization_version_key = "serializationVersion";
    const std::uint32_t invalid_serialization_version = InstanceIdentifierView::GetSerializationVersion() + 1;

    // When: The serialization version is deliberately corrupted with an invalid value
    auto it = serialized_unit.find(serialization_version_key);
    ASSERT_NE(it, serialized_unit.end());
    it->second = json::Any{invalid_serialization_version};
    score::json::JsonWriter writer{};
    const std::string serialized_string_form = writer.ToBuffer(serialized_unit).value();

    // Then: Verify creation of identifier with mismatched serialization version causes program termination
    EXPECT_DEATH(InstanceIdentifier::Create(serialized_string_form), ".*");
}

TEST_F(InstanceIdentifierDeathTest, DeathOnCreationWithDuplicateInstanceSpecifier)
{
    // Given: Two serialized instance identifiers with the same instance specifier
    const std::string serialized_form_service1 = GenerateSerializedInstanceIdentifier(kService1, kInstanceSpecifier1);
    const std::string serialized_form_service2 = GenerateSerializedInstanceIdentifier(kService2, kInstanceSpecifier1);

    // And: An instance identifier has been created from the first serialized form
    score::cpp::ignore = InstanceIdentifier::Create(serialized_form_service1);

    // When: Creating a second instance identifier with the same instance specifier
    // Then: The program terminates
    EXPECT_DEATH(InstanceIdentifier::Create(serialized_form_service2), ".*");
}

TEST_F(InstanceIdentifierFixture, HashInstanceIdentifierComparesEqual)
{
    ServiceInstanceDeployment deployment{kService1,
                                         LolaServiceInstanceDeployment{LolaServiceInstanceId{1U}},
                                         QualityType::kASIL_QM,
                                         kInstanceSpecifier1};
    const auto identifier_1 = make_InstanceIdentifier(deployment, kTestTypeDeployment1);
    const auto identifier_2 = identifier_1;
    const auto hash_1 = std::hash<InstanceIdentifier>{}.operator()(identifier_1);
    const auto hash_2 = std::hash<InstanceIdentifier>{}.operator()(identifier_2);
    EXPECT_EQ(hash_1, hash_2);
}

class InstanceIdentifierHashFixture : public ::testing::TestWithParam<std::array<InstanceIdentifier, 2>>
{
};

TEST_P(InstanceIdentifierHashFixture, HashesOfDifferentInstanceIdentifiersCompareInequal)
{
    const auto identifiers = GetParam();
    const auto identifier_1 = identifiers.at(0);
    const auto identifier_2 = identifiers.at(1);

    const auto hash_1 = std::hash<InstanceIdentifier>{}.operator()(identifier_1);
    const auto hash_2 = std::hash<InstanceIdentifier>{}.operator()(identifier_2);
    EXPECT_NE(hash_1, hash_2);
}

// Test that each element that should be used in the hashing algorithm is used by changing them one at a time.
INSTANTIATE_TEST_SUITE_P(
    InstanceIdentifierHashDifferentKeys,
    InstanceIdentifierHashFixture,
    ::testing::Values(
        std::array<InstanceIdentifier, 2>{make_InstanceIdentifier(ServiceInstanceDeployment{kService1,
                                                                                            kServiceInstanceDeployment1,
                                                                                            QualityType::kASIL_QM,
                                                                                            kInstanceSpecifier1},
                                                                  kTestTypeDeployment1),
                                          make_InstanceIdentifier(ServiceInstanceDeployment{kService2,
                                                                                            kServiceInstanceDeployment1,
                                                                                            QualityType::kASIL_QM,
                                                                                            kInstanceSpecifier1},
                                                                  kTestTypeDeployment1)},

        std::array<InstanceIdentifier, 2>{make_InstanceIdentifier(ServiceInstanceDeployment{kService1,
                                                                                            kServiceInstanceDeployment1,
                                                                                            QualityType::kASIL_QM,
                                                                                            kInstanceSpecifier1},
                                                                  kTestTypeDeployment1),
                                          make_InstanceIdentifier(ServiceInstanceDeployment{kService1,
                                                                                            kServiceInstanceDeployment2,
                                                                                            QualityType::kASIL_QM,
                                                                                            kInstanceSpecifier1},
                                                                  kTestTypeDeployment1)},

        std::array<InstanceIdentifier, 2>{make_InstanceIdentifier(ServiceInstanceDeployment{kService1,
                                                                                            kServiceInstanceDeployment1,
                                                                                            QualityType::kASIL_QM,
                                                                                            kInstanceSpecifier1},
                                                                  kTestTypeDeployment1),
                                          make_InstanceIdentifier(ServiceInstanceDeployment{kService1,
                                                                                            kServiceInstanceDeployment1,
                                                                                            QualityType::kASIL_B,
                                                                                            kInstanceSpecifier1},
                                                                  kTestTypeDeployment1)},

        std::array<InstanceIdentifier, 2>{make_InstanceIdentifier(ServiceInstanceDeployment{kService1,
                                                                                            kServiceInstanceDeployment1,
                                                                                            QualityType::kASIL_QM,
                                                                                            kInstanceSpecifier1},
                                                                  kTestTypeDeployment1),
                                          make_InstanceIdentifier(ServiceInstanceDeployment{kService1,
                                                                                            kServiceInstanceDeployment1,
                                                                                            QualityType::kASIL_QM,
                                                                                            kInstanceSpecifier2},
                                                                  kTestTypeDeployment1)},

        std::array<InstanceIdentifier, 2>{make_InstanceIdentifier(ServiceInstanceDeployment{kService1,
                                                                                            kServiceInstanceDeployment1,
                                                                                            QualityType::kASIL_QM,
                                                                                            kInstanceSpecifier1},
                                                                  kTestTypeDeployment1),
                                          make_InstanceIdentifier(ServiceInstanceDeployment{kService1,
                                                                                            kServiceInstanceDeployment1,
                                                                                            QualityType::kASIL_QM,
                                                                                            kInstanceSpecifier1},
                                                                  kTestTypeDeployment2)}));

}  // namespace
}  // namespace score::mw::com::impl
