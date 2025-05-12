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
#include "score/mw/com/impl/configuration/service_version_type.h"

#include "score/json/json_parser.h"

#include "score/mw/com/impl/configuration/test/configuration_test_resources.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl
{
namespace
{

TEST(ServiceVersionTypeTest, CanBeCopiedAndEqualCompared)
{
    RecordProperty("Verifies", "SCR-13807879");  // SWS_CM_01010
    RecordProperty("Description", "Checks CopyAssignment operator and EqualComparableOperator");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const auto unit = make_ServiceVersionType(42U, 43U);
    const auto unitCopy = unit;

    ASSERT_EQ(unit, unitCopy);
}

TEST(ServiceVersionTypeTest, LessCompareable)
{
    RecordProperty("Verifies", "SCR-13807879");  // SWS_CM_01010
    RecordProperty("Description", "Checks LessComparableOperator");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const auto unit = make_ServiceVersionType(42U, 43U);
    const auto majorLess = make_ServiceVersionType(41U, 44U);
    const auto majorEqualMinorLess = make_ServiceVersionType(42U, 41U);

    ASSERT_LT(majorLess, unit);
    ASSERT_LT(majorEqualMinorLess, unit);
}

using ServiceVersionTypeFixture = ConfigurationStructsFixture;
TEST_F(ServiceVersionTypeFixture, CanCreateFromSerializedObject)
{
    auto unit = make_ServiceVersionType(42U, 43U);

    const auto serialized_unit{unit.Serialize()};

    ServiceVersionType reconstructed_unit{serialized_unit};

    ExpectServiceVersionTypeObjectsEqual(reconstructed_unit, unit);
}

TEST(ServiceVersionTypeTest, StringifiedVersionOfSameServiceVersionTypesAreEqual)
{
    auto unit = make_ServiceVersionType(42U, 43U);
    auto unit2 = make_ServiceVersionType(42U, 43U);

    EXPECT_EQ(unit.ToString(), unit2.ToString());
}

TEST(ServiceVersionTypeDeathTest, CreatingFromSerializedObjectWithMismatchedSerializationVersionTerminates)
{
    auto unit = make_ServiceVersionType(42U, 43U);

    const auto serialization_version_key = "serializationVersion";
    const std::uint32_t invalid_serialization_version = ServiceVersionTypeView::GetSerializationVersion() + 1;

    auto serialized_unit{unit.Serialize()};
    auto it = serialized_unit.find(serialization_version_key);
    ASSERT_NE(it, serialized_unit.end());
    it->second = json::Any{invalid_serialization_version};

    EXPECT_DEATH(ServiceVersionType reconstructed_unit{serialized_unit}, ".*");
}

}  // namespace
}  // namespace score::mw::com::impl
