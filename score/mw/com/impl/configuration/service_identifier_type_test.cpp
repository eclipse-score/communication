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
#include "score/mw/com/impl/configuration/service_identifier_type.h"

#include "score/mw/com/impl/configuration/test/configuration_test_resources.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl
{
namespace
{

const char svTypName1[] = "/bla/blub/1";
const char svTypName2[] = "/bla/blub/2";

TEST(ServiceIdentifierTypeTest, CanBeCopiedAndEqualCompared)
{
    RecordProperty("Verifies", "SCR-13807879");  // SWS_CM_01010
    RecordProperty("Description", "Checks CopyAssignment operator and EqualComparableOperator");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const auto unit = make_ServiceIdentifierType(svTypName1);
    const auto unitCopy = unit;

    ASSERT_EQ(unit, unitCopy);
}

TEST(ServiceIdentifierTypeTest, LessCompareable)
{
    RecordProperty("Verifies", "SCR-13807879");  // SWS_CM_01010
    RecordProperty("Description", "Checks LessComparableOperator");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const auto unit = make_ServiceIdentifierType(svTypName2);
    const auto less = make_ServiceIdentifierType(svTypName1);

    ASSERT_LT(less, unit);
}

TEST(ServiceIdentifierTypeTest, LessCompareableVersion)
{
    RecordProperty("Verifies", "SCR-13807879");  // SWS_CM_01010
    RecordProperty("Description", "Checks LessComparableOperator");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const auto unit = make_ServiceIdentifierType(svTypName2, 2, 0);
    const auto less = make_ServiceIdentifierType(svTypName2, 1, 0);

    ASSERT_LT(less, unit);
}

using ServiceInstanceDeploymentFixture = ConfigurationStructsFixture;
TEST_F(ServiceInstanceDeploymentFixture, CanCreateFromSerializedObject)
{
    auto unit = make_ServiceIdentifierType(svTypName2, 2U, 1U);

    const auto serialized_unit{unit.Serialize()};

    ServiceIdentifierType reconstructed_unit{serialized_unit};

    ExpectServiceIdentifierTypeObjectsEqual(reconstructed_unit, unit);
}

TEST(ServiceIdentifierTypeTest, StringifiedVersionOfSameServiceIdentifierTypesAreEqual)
{
    auto unit = make_ServiceIdentifierType(svTypName2, 2U, 1U);

    const auto serialized_unit{unit.Serialize()};

    ServiceIdentifierType reconstructed_unit{serialized_unit};

    EXPECT_EQ(reconstructed_unit.ToString(), unit.ToString());
}

TEST(ServiceIdentifierTypeDeathTest, CreatingFromSerializedObjectWithMismatchedSerializationVersionTerminates)
{
    auto unit = make_ServiceIdentifierType(svTypName2, 2U, 1U);

    const auto serialization_version_key = "serializationVersion";
    const std::uint32_t invalid_serialization_version = ServiceIdentifierTypeView::GetSerializationVersion() + 1;

    auto serialized_unit{unit.Serialize()};
    auto it = serialized_unit.find(serialization_version_key);
    ASSERT_NE(it, serialized_unit.end());
    it->second = json::Any{invalid_serialization_version};

    EXPECT_DEATH(ServiceIdentifierType reconstructed_unit{serialized_unit}, ".*");
}

TEST(ServiceIdentifierTypeHashTest, CanHash)
{
    static auto unit = std::hash<ServiceIdentifierType>{}(make_ServiceIdentifierType(svTypName1));
    ASSERT_NE(unit, 0);
}

TEST(ServiceIdentifierTypeHashTest, HashesOfTheSameServiceIdentifierTypeAreEqual)
{
    const auto unit = make_ServiceIdentifierType("test_name", 2U, 0U);
    const auto unit_2 = make_ServiceIdentifierType("test_name", 2U, 0U);

    static auto hash_value = std::hash<ServiceIdentifierType>{}(unit);
    static auto hash_value_2 = std::hash<ServiceIdentifierType>{}(unit_2);

    ASSERT_EQ(hash_value, hash_value_2);
}

class ServiceIdentifierTypeHashFixture : public ::testing::TestWithParam<std::array<ServiceIdentifierType, 2>>
{
};

TEST_P(ServiceIdentifierTypeHashFixture, HashesOfTheDifferentServiceIdentifierTypeAreNotEqual)
{
    const auto service_identifier_types = GetParam();

    // Given 2 ServiceIdentifierTypes containing different values
    const auto service_identifier_type = service_identifier_types.at(0);
    const auto service_identifier_type_2 = service_identifier_types.at(1);

    // When calculating the hash of the ServiceIdentifierTypes
    auto hash_value = std::hash<ServiceIdentifierType>{}(service_identifier_type);
    auto hash_value_2 = std::hash<ServiceIdentifierType>{}(service_identifier_type_2);

    // Then the hash value should be different
    ASSERT_NE(hash_value, hash_value_2);
}

INSTANTIATE_TEST_CASE_P(
    ServiceIdentifierTypeHashDifferentKeys,
    ServiceIdentifierTypeHashFixture,
    ::testing::Values(std::array<ServiceIdentifierType, 2>{make_ServiceIdentifierType("test_name", 2U, 0U),
                                                           make_ServiceIdentifierType("test_name2", 2U, 0U)},
                      std::array<ServiceIdentifierType, 2>{make_ServiceIdentifierType("test_name", 1U, 0U),
                                                           make_ServiceIdentifierType("test_name", 2U, 0U)},
                      std::array<ServiceIdentifierType, 2>{make_ServiceIdentifierType("test_name", 2U, 1U),
                                                           make_ServiceIdentifierType("test_name", 2U, 0U)}));

}  // namespace
}  // namespace score::mw::com::impl
