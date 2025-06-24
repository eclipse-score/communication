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
#include "score/mw/com/impl/configuration/service_type_deployment.h"

#include "lola_service_id.h"
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

ServiceIdentifierType dummy_service = make_ServiceIdentifierType("foo", 1U, 0U);
const LolaServiceId kLolaServiceId{123U};

TEST(ServiceTypeDeploymentTest, CanConstructFromLolaServiceTypeDeployment)
{
    ServiceTypeDeployment unit{MakeLolaServiceTypeDeployment()};

    const auto* const binding_ptr = std::get_if<LolaServiceTypeDeployment>(&unit.binding_info_);
    ASSERT_NE(binding_ptr, nullptr);
}

TEST(ServiceTypeDeploymentTest, CanConstructFromBlankBindingDeployment)
{
    ServiceTypeDeployment unit{score::cpp::blank{}};

    const auto* const binding_ptr = std::get_if<score::cpp::blank>(&unit.binding_info_);
    ASSERT_NE(binding_ptr, nullptr);
}

using ServiceTypeDeploymentFixture = ConfigurationStructsFixture;
TEST_F(ServiceTypeDeploymentFixture, CanCreateFromSerializedLolaObject)
{
    ServiceTypeDeployment unit{MakeLolaServiceTypeDeployment()};

    const auto serialized_unit{unit.Serialize()};

    ServiceTypeDeployment reconstructed_unit{serialized_unit};

    ExpectServiceTypeDeploymentObjectsEqual(reconstructed_unit, unit);
}

TEST_F(ServiceTypeDeploymentFixture, ComparingSameDeploymentsReturnsTrue)
{
    // When comparing two ServiceTypeDeployments containing the same data
    const ServiceTypeDeployment unit{MakeLolaServiceTypeDeployment(kLolaServiceId)};
    const ServiceTypeDeployment unit2{MakeLolaServiceTypeDeployment(kLolaServiceId)};
    const auto are_equal = unit == unit2;

    // Then the result is true
    EXPECT_TRUE(are_equal);
}

TEST_F(ServiceTypeDeploymentFixture, ComparingDifferentDeploymentsReturnsFalse)
{
    // When comparing two ServiceTypeDeployments containing different data
    const ServiceTypeDeployment unit{MakeLolaServiceTypeDeployment(kLolaServiceId)};
    const ServiceTypeDeployment unit2{MakeLolaServiceTypeDeployment(kLolaServiceId + 1U)};
    const auto are_equal = unit == unit2;

    // Then the result is false
    EXPECT_FALSE(are_equal);
}

TEST(ServiceTypeDeploymentTest, CanCreateFromSerializedBlankObject)
{
    ServiceTypeDeployment unit{score::cpp::blank{}};

    const auto serialized_unit{unit.Serialize()};

    ServiceTypeDeployment reconstructed_unit{serialized_unit};
}

TEST(ServiceTypeDeploymentDeathTest, CreatingFromSerializedObjectWithMismatchedSerializationVersionTerminates)
{
    const ServiceTypeDeployment unit{MakeLolaServiceTypeDeployment()};

    const auto serialization_version_key = "serializationVersion";
    const std::uint32_t invalid_serialization_version = ServiceTypeDeployment::serializationVersion + 1;

    auto serialized_unit{unit.Serialize()};
    auto it = serialized_unit.find(serialization_version_key);
    ASSERT_NE(it, serialized_unit.end());
    it->second = json::Any{invalid_serialization_version};

    EXPECT_DEATH(ServiceTypeDeployment reconstructed_unit{serialized_unit}, ".*");
}

TEST_F(ServiceTypeDeploymentFixture, CanGetLolaBindingFromServiceTypeDeploymentContainingLolaBinding)
{
    // Given a ServiceTypeDeployment containing a Lola binding
    const auto& lola_service_type_deployment = MakeLolaServiceTypeDeployment();
    ServiceTypeDeployment service_type_deployment{lola_service_type_deployment};

    // When getting the LolaServiceTypeDeployment
    const auto& returned_service_type_deployment_binding =
        GetServiceTypeDeploymentBinding<LolaServiceTypeDeployment>(service_type_deployment);

    // Then the lola binding of the ServiceTypeDeployment is returned
    EXPECT_EQ(lola_service_type_deployment, returned_service_type_deployment_binding);
}

TEST_F(ServiceTypeDeploymentFixture, CanGetBlankBindingFromServiceTypeDeploymentContainingBlankBinding)
{
    // Given a ServiceTypeDeployment containing a blank binding
    ServiceTypeDeployment service_type_deployment{score::cpp::blank{}};

    // When getting the blank service type binding
    const auto returned_service_type_deployment_binding =
        GetServiceTypeDeploymentBinding<score::cpp::blank>(service_type_deployment);

    // Then a blank binding is returned
    EXPECT_EQ(score::cpp::blank{}, returned_service_type_deployment_binding);
}

TEST_F(ServiceTypeDeploymentFixture, GettingLolaBindingFromServiceTypeDeploymentNotContainingLolaBindingTerminates)
{
    // Given a ServiceTypeDeployment containing a blank binding
    ServiceTypeDeployment service_type_deployment{score::cpp::blank{}};

    // When getting the LolaServiceTypeDeployment
    // Then the program terminates
    EXPECT_DEATH(score::cpp::ignore = GetServiceTypeDeploymentBinding<LolaServiceTypeDeployment>(service_type_deployment),
                 ".*");
}

TEST_F(ServiceTypeDeploymentFixture, GettingBlankBindingFromServiceTypeDeploymentNotContainingBlankBindingTerminates)
{
    // Given a ServiceTypeDeployment containing a Lola binding
    const auto& lola_service_type_deployment = MakeLolaServiceTypeDeployment();
    ServiceTypeDeployment service_type_deployment{lola_service_type_deployment};

    // When getting a blank binding
    // Then the program terminates
    EXPECT_DEATH(score::cpp::ignore = GetServiceTypeDeploymentBinding<score::cpp::blank>(service_type_deployment), ".*");
}

class ServiceTypeDeploymentHashFixture
    : public ::testing::TestWithParam<std::tuple<ServiceTypeDeployment, std::string_view>>
{
};

TEST_P(ServiceTypeDeploymentHashFixture, ToHashString)
{
    const auto param_tuple = GetParam();

    const auto unit = std::get<ServiceTypeDeployment>(param_tuple);
    const auto expected_hash_string = std::get<std::string_view>(param_tuple);

    const auto actual_hash_string = unit.ToHashString();

    EXPECT_EQ(actual_hash_string, expected_hash_string);
    EXPECT_EQ(actual_hash_string.size(), ServiceTypeDeployment::hashStringSize);
}

const std::vector<std::tuple<ServiceTypeDeployment, std::string_view>> type_id_to_hash_string_variations{
    {ServiceTypeDeployment{LolaServiceTypeDeployment{0U}}, "00000"},
    {ServiceTypeDeployment{LolaServiceTypeDeployment{1U}}, "00001"},
    {ServiceTypeDeployment{LolaServiceTypeDeployment{10U}}, "0000a"},
    {ServiceTypeDeployment{LolaServiceTypeDeployment{255U}}, "000ff"},
    {ServiceTypeDeployment{LolaServiceTypeDeployment{std::numeric_limits<LolaServiceId>::max()}}, "0ffff"}};
INSTANTIATE_TEST_SUITE_P(ServiceTypeDeploymentHashFixture,
                         ServiceTypeDeploymentHashFixture,
                         ::testing::ValuesIn(type_id_to_hash_string_variations));

}  // namespace
}  // namespace score::mw::com::impl
