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
#include "score/mw/com/impl/tracing/configuration/service_element_identifier.h"

#include "score/mw/com/impl/service_element_type.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <unordered_map>

namespace score::mw::com::impl::tracing
{
namespace
{

const std::string kServiceTypeName{"type_name"};
const std::string kServiceElementName{"element_name"};
const ServiceElementType kServiceElementType{ServiceElementType::EVENT};

TEST(ServiceElementIdentifierHashTest, CanHash)
{
    // Given a ServiceElementIdentifier
    const ServiceElementIdentifier service_element_identifier{
        kServiceTypeName, kServiceElementName, kServiceElementType};

    // When calculating the hash of a ServiceElementIdentifier
    auto hash_value = std::hash<ServiceElementIdentifier>{}(service_element_identifier);

    // Then the hash value should be non-zero
    ASSERT_NE(hash_value, 0);
}

TEST(ServiceElementIdentifierHashTest, CanUseAsKeyInMap)
{
    // Given a ServiceElementIdentifier
    const ServiceElementIdentifier service_element_identifier{
        kServiceTypeName, kServiceElementName, kServiceElementType};

    // When using a ServiceElementIdentifier as a key in a map
    std::unordered_map<ServiceElementIdentifier, int> my_map{std::make_pair(service_element_identifier, 10)};

    // Then we compile and don't crash
}

TEST(ServiceElementIdentifierHashTest, HashesOfTheSameServiceElementIdentifiersAreEqual)
{
    // Given 2 ServiceElementIdentifiers with containing the same values
    const ServiceElementIdentifier service_element_identifier{
        "service_type_name", "service_element_name", kServiceElementType};
    const ServiceElementIdentifier service_element_identifier_2{
        "service_type_name", "service_element_name", kServiceElementType};

    // When calculating the hash of the ServiceElementIdentifiers
    auto hash_value = std::hash<ServiceElementIdentifier>{}(service_element_identifier);
    auto hash_value_2 = std::hash<ServiceElementIdentifier>{}(service_element_identifier_2);

    // Then the hash value should be equal
    ASSERT_EQ(hash_value, hash_value_2);
}

TEST(ServiceElementIdentifierHashDeathTest, HashingServiceElementIdentifierWithTooLongStringsTerminates)
{
    constexpr std::size_t max_buffer_size{1024U};

    std::string service_type_name(max_buffer_size, 'a');
    std::string service_element_name(max_buffer_size, 'b');

    // Given 2 ServiceElementIdentifiers with containing the same values
    const ServiceElementIdentifier service_element_identifier{
        service_type_name, service_element_name, kServiceElementType};

    // When calculating the hash of the ServiceElementIdentifier
    EXPECT_DEATH(std::hash<ServiceElementIdentifier>{}(service_element_identifier), ".*");
}

class ServiceElementIdentifierEqualityFixture
    : public ::testing::TestWithParam<std::pair<ServiceElementIdentifier, ServiceElementIdentifier>>
{
};

TEST_P(ServiceElementIdentifierEqualityFixture, HashesOfTheDifferentServiceElementIdentifiersAreNotEqual)
{
    const auto service_element_identifiers = GetParam();

    // Given 2 ServiceElementIdentifiers containing different values
    const auto service_element_identifier = service_element_identifiers.first;
    const auto service_element_identifier_2 = service_element_identifiers.second;

    // When calculating the hash of the ServiceElementIdentifiers
    auto hash_value = std::hash<ServiceElementIdentifier>{}(service_element_identifier);
    auto hash_value_2 = std::hash<ServiceElementIdentifier>{}(service_element_identifier_2);

    // Then the hash value should be different
    ASSERT_NE(hash_value, hash_value_2);
}

TEST_P(ServiceElementIdentifierEqualityFixture, DifferentServiceElementIdentifiersAreNotEqual)
{
    const auto service_element_identifiers = GetParam();

    // Given 2 ServiceElementIdentifiers containing different values
    const auto service_element_identifier = service_element_identifiers.first;
    const auto service_element_identifier_2 = service_element_identifiers.second;

    // Then the equality operator should return false
    ASSERT_FALSE(service_element_identifier == service_element_identifier_2);
}

INSTANTIATE_TEST_CASE_P(
    ServiceElementIdentifierEqualityFixture,
    ServiceElementIdentifierEqualityFixture,
    ::testing::Values(
        std::make_pair(ServiceElementIdentifier{"same_type_name", "same_element_name", ServiceElementType::EVENT},
                       ServiceElementIdentifier{"different_type_name", "same_element_name", ServiceElementType::EVENT}),
        std::make_pair(ServiceElementIdentifier{"same_type_name", "same_element_name", ServiceElementType::EVENT},
                       ServiceElementIdentifier{"same_type_name", "different_element_name", ServiceElementType::EVENT}),
        std::make_pair(ServiceElementIdentifier{"same_type_name", "same_element_name", ServiceElementType::EVENT},
                       ServiceElementIdentifier{"same_type_name", "same_element_name", ServiceElementType::FIELD})));

TEST(ServiceElementIdentifierComparisonTest, ComparingTheSameServiceElementIdentifierReturnsFalse)
{
    // Given an ServiceElementIdentifier
    ServiceElementIdentifier service_element_identifier_view{"a", "b", static_cast<ServiceElementType>(1U)};

    // Then the comparing the same ServiceElementIdentifier should return false
    ASSERT_FALSE(service_element_identifier_view < service_element_identifier_view);
}

class ServiceElementIdentifierComparisonFixture
    : public ::testing::TestWithParam<std::pair<ServiceElementIdentifier, ServiceElementIdentifier>>
{
};

TEST_P(ServiceElementIdentifierComparisonFixture, ServiceElementIdentifierComparisonReturnsCorrectResult)
{
    const auto service_element_identifiers = GetParam();

    // Given 2 ServiceElementIdentifiers where the first value is smaller than the second value
    const auto service_element_identifier_view = service_element_identifiers.first;
    const auto service_element_identifier_view_2 = service_element_identifiers.second;

    // Then the comparison operator should return true
    ASSERT_TRUE(service_element_identifier_view < service_element_identifier_view_2);
}

INSTANTIATE_TEST_CASE_P(
    ServiceElementIdentifierComparisonFixture,
    ServiceElementIdentifierComparisonFixture,
    ::testing::Values(std::make_pair(ServiceElementIdentifier{"a", "c", static_cast<ServiceElementType>(1U)},
                                     ServiceElementIdentifier{"b", "b", static_cast<ServiceElementType>(0U)}),
                      std::make_pair(ServiceElementIdentifier{"a", "b", static_cast<ServiceElementType>(1U)},
                                     ServiceElementIdentifier{"a", "c", static_cast<ServiceElementType>(0U)}),
                      std::make_pair(ServiceElementIdentifier{"a", "b", static_cast<ServiceElementType>(0U)},
                                     ServiceElementIdentifier{"a", "b", static_cast<ServiceElementType>(1U)})));

TEST(ServiceElementIdentifierStreamTest, OperatorStreamOutputsServiceElementData)
{
    // Given a ServiceElementIdentifier
    const ServiceElementIdentifier service_element_identifier{"TestType", "TestElement", ServiceElementType::EVENT};

    testing::internal::CaptureStdout();

    // When streaming the ServiceElementIdentifier to a log
    score::mw::log::LogFatal("test") << service_element_identifier;

    std::string output = testing::internal::GetCapturedStdout();

    // Then the output should contain the formatted service type, service element name, and service element type
    EXPECT_THAT(output, testing::HasSubstr("service type:  TestType"));
    EXPECT_THAT(output, testing::HasSubstr("service element:  TestElement"));
    EXPECT_THAT(output, testing::HasSubstr("service element type:  EVENT"));
}

}  // namespace
}  // namespace score::mw::com::impl::tracing
