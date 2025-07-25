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
#include "score/mw/com/impl/tracing/configuration/service_element_identifier_view.h"

#include "score/mw/com/impl/service_element_type.h"

#include <gtest/gtest.h>
#include <utility>

namespace score::mw::com::impl::tracing
{
namespace
{

const std::string kServiceTypeName{"type_name"};
const std::string kServiceElementName{"element_name"};
const ServiceElementType kServiceElementType{ServiceElementType::EVENT};

TEST(ServiceElementIdentifierViewHashTest, CanHash)
{
    // Given a ServiceElementIdentifierView
    const ServiceElementIdentifierView service_element_identifier{
        kServiceTypeName, kServiceElementName, kServiceElementType};

    // When calculating the hash of a ServiceElementIdentifierView
    auto hash_value = std::hash<ServiceElementIdentifierView>{}(service_element_identifier);

    // Then the hash value should be non-zero
    ASSERT_NE(hash_value, 0);
}

TEST(ServiceElementIdentifierViewHashTest, CanUseAsKeyInMap)
{
    // Given a ServiceElementIdentifierView
    const ServiceElementIdentifierView service_element_identifier{
        kServiceTypeName, kServiceElementName, kServiceElementType};

    // When using a ServiceElementIdentifierView as a key in a map
    std::unordered_map<ServiceElementIdentifierView, int> my_map{std::make_pair(service_element_identifier, 10)};

    // Then we compile and don't crash
}

TEST(ServiceElementIdentifierViewHashTest, HashesOfTheSameServiceElementIdentifiersAreEqual)
{
    // Given 2 ServiceElementIdentifiers with containing the same values
    const ServiceElementIdentifierView service_element_identifier{
        "service_type_name", "service_element_name", kServiceElementType};
    const ServiceElementIdentifierView service_element_identifier_2{
        "service_type_name", "service_element_name", kServiceElementType};

    // When calculating the hash of the ServiceElementIdentifiers
    auto hash_value = std::hash<ServiceElementIdentifierView>{}(service_element_identifier);
    auto hash_value_2 = std::hash<ServiceElementIdentifierView>{}(service_element_identifier_2);

    // Then the hash value should be equal
    ASSERT_EQ(hash_value, hash_value_2);
}

TEST(ServiceElementIdentifierHashDeathTest, HashingServiceElementIdentifierWithTooLongStringsTerminates)
{
    constexpr std::size_t max_buffer_size{1024U};

    std::string service_type_name(max_buffer_size, 'a');
    std::string service_element_name(max_buffer_size, 'b');

    // Given 2 ServiceElementIdentifiers with containing the same values
    const ServiceElementIdentifierView service_element_identifier{
        service_type_name, service_element_name, kServiceElementType};

    // When calculating the hash of the ServiceElementIdentifierView
    EXPECT_DEATH(std::hash<ServiceElementIdentifierView>{}(service_element_identifier), ".*");
}

class ServiceElementIdentifierViewEqualityFixture
    : public ::testing::TestWithParam<std::pair<ServiceElementIdentifierView, ServiceElementIdentifierView>>
{
};

TEST_P(ServiceElementIdentifierViewEqualityFixture, HashesOfTheDifferentServiceElementIdentifiersAreNotEqual)
{
    const auto service_element_identifiers = GetParam();

    // Given 2 ServiceElementIdentifiers containing different values
    const auto service_element_identifier = service_element_identifiers.first;
    const auto service_element_identifier_2 = service_element_identifiers.second;

    // When calculating the hash of the ServiceElementIdentifiers
    auto hash_value = std::hash<ServiceElementIdentifierView>{}(service_element_identifier);
    auto hash_value_2 = std::hash<ServiceElementIdentifierView>{}(service_element_identifier_2);

    // Then the hash value should be different
    ASSERT_NE(hash_value, hash_value_2);
}

TEST_P(ServiceElementIdentifierViewEqualityFixture, DifferentServiceElementIdentifierViewsAreNotEqual)
{
    const auto service_element_identifiers = GetParam();

    // Given 2 ServiceElementIdentifierViews containing different values
    const auto service_element_identifier_view = service_element_identifiers.first;
    const auto service_element_identifier_view_2 = service_element_identifiers.second;

    // Then the equality operator should return false
    ASSERT_FALSE(service_element_identifier_view == service_element_identifier_view_2);
}

INSTANTIATE_TEST_CASE_P(
    ServiceElementIdentifierViewEqualityFixture,
    ServiceElementIdentifierViewEqualityFixture,
    ::testing::Values(
        std::make_pair(
            ServiceElementIdentifierView{"same_type_name", "same_element_name", ServiceElementType::EVENT},
            ServiceElementIdentifierView{"different_type_name", "same_element_name", ServiceElementType::EVENT}),
        std::make_pair(
            ServiceElementIdentifierView{"same_type_name", "same_element_name", ServiceElementType::EVENT},
            ServiceElementIdentifierView{"same_type_name", "different_element_name", ServiceElementType::EVENT}),
        std::make_pair(
            ServiceElementIdentifierView{"same_type_name", "same_element_name", ServiceElementType::EVENT},
            ServiceElementIdentifierView{"same_type_name", "same_element_name", ServiceElementType::FIELD})));

TEST(ServiceElementIdentifierViewComparisonTest, ComparingTheSameServiceElementIdentifierViewReturnsFalse)
{
    // Given an ServiceElementIdentifierView
    ServiceElementIdentifierView service_element_identifier_view{"a", "b", static_cast<ServiceElementType>(1U)};

    // Then the comparing the same ServiceElementIdentifierView should return false
    ASSERT_FALSE(service_element_identifier_view < service_element_identifier_view);
}

class ServiceElementIdentifierViewComparisonFixture
    : public ::testing::TestWithParam<std::pair<ServiceElementIdentifierView, ServiceElementIdentifierView>>
{
};

TEST_P(ServiceElementIdentifierViewComparisonFixture, ServiceElementIdentifierViewComparisonReturnsCorrectResult)
{
    const auto service_element_identifiers = GetParam();

    // Given 2 ServiceElementIdentifierViews where the first value is smaller than the second value
    const auto service_element_identifier_view = service_element_identifiers.first;
    const auto service_element_identifier_view_2 = service_element_identifiers.second;

    // Then the comparison operator should return true
    ASSERT_TRUE(service_element_identifier_view < service_element_identifier_view_2);
}

INSTANTIATE_TEST_CASE_P(
    ServiceElementIdentifierViewComparisonFixture,
    ServiceElementIdentifierViewComparisonFixture,
    ::testing::Values(std::make_pair(ServiceElementIdentifierView{"a", "c", static_cast<ServiceElementType>(1U)},
                                     ServiceElementIdentifierView{"b", "b", static_cast<ServiceElementType>(0U)}),
                      std::make_pair(ServiceElementIdentifierView{"a", "b", static_cast<ServiceElementType>(1U)},
                                     ServiceElementIdentifierView{"a", "c", static_cast<ServiceElementType>(0U)}),
                      std::make_pair(ServiceElementIdentifierView{"a", "b", static_cast<ServiceElementType>(0U)},
                                     ServiceElementIdentifierView{"a", "b", static_cast<ServiceElementType>(1U)})));

}  // namespace
}  // namespace score::mw::com::impl::tracing
