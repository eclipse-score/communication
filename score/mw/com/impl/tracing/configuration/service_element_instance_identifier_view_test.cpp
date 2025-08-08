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
#include "score/mw/com/impl/tracing/configuration/service_element_instance_identifier_view.h"

#include <gtest/gtest.h>
#include <unordered_map>

namespace score::mw::com::impl::tracing
{
namespace
{

const ServiceElementIdentifierView kServiceElementIdentifier{"my_service_type",
                                                             "my_service_element",
                                                             ServiceElementType::EVENT};
const std::string_view kInstanceSpecifier = "my_instance_specifier";

TEST(ServiceElementInstanceIdentifierHashTest, CanHash)
{
    // Given a ServiceElementInstanceIdentifierView
    const ServiceElementInstanceIdentifierView service_element_instance_identifier_view{kServiceElementIdentifier,
                                                                                        kInstanceSpecifier};

    // When calculating the hash of a ServiceElementInstanceIdentifierView
    auto hash_value = std::hash<ServiceElementInstanceIdentifierView>{}(service_element_instance_identifier_view);

    // Then the hash value should be non-zero
    ASSERT_NE(hash_value, 0);
}

TEST(ServiceElementInstanceIdentifierHashTest, CanUseAsKeyInMap)
{
    // Given a ServiceElementInstanceIdentifierView
    const ServiceElementInstanceIdentifierView service_element_instance_identifier_view{kServiceElementIdentifier,
                                                                                        kInstanceSpecifier};

    // When using a ServiceElementInstanceIdentifierView as a key in a map
    std::unordered_map<ServiceElementInstanceIdentifierView, int> my_map{
        std::make_pair(service_element_instance_identifier_view, 10)};

    // Then we compile and don't crash
}

TEST(ServiceElementInstanceIdentifierHashTest, HashesOfTheSameServiceElementInstanceIdentifiersAreEqual)
{
    // Given 2 ServiceElementInstanceIdentifiers with containing the same values
    const ServiceElementInstanceIdentifierView service_element_instance_identifier_view{kServiceElementIdentifier,
                                                                                        kInstanceSpecifier};
    const ServiceElementInstanceIdentifierView service_element_instance_identifier_2{kServiceElementIdentifier,
                                                                                     kInstanceSpecifier};

    // When calculating the hash of the ServiceElementInstanceIdentifiers
    auto hash_value = std::hash<ServiceElementInstanceIdentifierView>{}(service_element_instance_identifier_view);
    auto hash_value_2 = std::hash<ServiceElementInstanceIdentifierView>{}(service_element_instance_identifier_2);

    // Then the hash value should be equal
    ASSERT_EQ(hash_value, hash_value_2);
}

class ServiceElementInstanceIdentifierComparisonFixture
    : public ::testing::TestWithParam<
          std::pair<ServiceElementInstanceIdentifierView, ServiceElementInstanceIdentifierView>>
{
};

TEST_P(ServiceElementInstanceIdentifierComparisonFixture,
       HashesOfTheDifferentServiceElementInstanceIdentifiersAreNotEqual)
{
    const auto service_element_instance_identifiers = GetParam();

    // Given 2 ServiceElementInstanceIdentifiers containing different values
    const auto service_element_instance_identifier_view = service_element_instance_identifiers.first;
    const auto service_element_instance_identifier_2 = service_element_instance_identifiers.second;

    // When calculating the hash of the ServiceElementInstanceIdentifiers
    auto hash_value = std::hash<ServiceElementInstanceIdentifierView>{}(service_element_instance_identifier_view);
    auto hash_value_2 = std::hash<ServiceElementInstanceIdentifierView>{}(service_element_instance_identifier_2);

    // Then the hash value should be different
    ASSERT_NE(hash_value, hash_value_2);
}

TEST_P(ServiceElementInstanceIdentifierComparisonFixture, DifferentServiceElementInstanceIdentifiersAreNotEqual)
{
    const auto service_element_instance_identifiers = GetParam();

    // Given 2 ServiceElementInstanceIdentifiers containing different values
    const auto service_element_instance_identifier_view = service_element_instance_identifiers.first;
    const auto service_element_instance_identifier_2 = service_element_instance_identifiers.second;

    // Then the equality operator should return false
    ASSERT_FALSE(service_element_instance_identifier_view == service_element_instance_identifier_2);
}

// Test that each element that should be used in the hashing algorithm is used by changing them one at a time.
INSTANTIATE_TEST_CASE_P(
    ServiceElementInstanceIdentifierComparisonFixture,
    ServiceElementInstanceIdentifierComparisonFixture,
    ::testing::Values(
        std::make_pair(ServiceElementInstanceIdentifierView{{"same_type", "same_element", ServiceElementType::EVENT},
                                                            "same_specifier"},
                       ServiceElementInstanceIdentifierView{
                           {"different_type", "same_element", ServiceElementType::EVENT},
                           "same_specifier"}),

        std::make_pair(ServiceElementInstanceIdentifierView{{"same_type", "same_element", ServiceElementType::EVENT},
                                                            "same_specifier"},
                       ServiceElementInstanceIdentifierView{
                           {"same_type", "different_element", ServiceElementType::EVENT},
                           "same_specifier"}),

        std::make_pair(ServiceElementInstanceIdentifierView{{"same_type", "same_element", ServiceElementType::EVENT},
                                                            "same_specifier"},
                       ServiceElementInstanceIdentifierView{{"same_type", "same_element", ServiceElementType::FIELD},
                                                            "same_specifier"}),

        std::make_pair(ServiceElementInstanceIdentifierView{{"same_type", "same_element", ServiceElementType::EVENT},
                                                            "same_specifier"},
                       ServiceElementInstanceIdentifierView{{"same_type", "same_element", ServiceElementType::EVENT},
                                                            "different_specifier"})));

TEST(ServiceElementInstanceIdentifierHashDeathTest, HashingServiceElementInstanceIdentifierWithTooLongStringsTerminates)
{
    constexpr std::size_t max_buffer_size{1024U};

    std::string service_type_name(max_buffer_size, 'a');
    std::string service_element_name(max_buffer_size, 'b');

    // Given 2 ServiceElementIdentifiers with containing the same values
    const ServiceElementInstanceIdentifierView service_element_instance_identifier_view{
        {service_type_name, service_element_name, ServiceElementType::EVENT}, kInstanceSpecifier};

    // When calculating the hash of the ServiceElementIdentifierView
    EXPECT_DEATH(std::hash<ServiceElementInstanceIdentifierView>{}(service_element_instance_identifier_view), ".*");
}

TEST(ServiceElementInstanceIdentifierViewHashColisionTest,
     IfServiceElementTypeOverwritesFirstLetterOfInstanceSpecifierHashesCanCollide)
{
    std::string service_type_name{"bla"};
    std::string service_element_name{"meh"};

    // Two instance specifiers that only differ in the first letter
    // Given 2 ServiceElementIdentifiers with containing the same values
    std::string instance_specifier_b{"bat"};
    std::string instance_specifier_c{"cat"};

    const ServiceElementInstanceIdentifierView service_element_instance_identifier_view_b{
        {service_type_name, service_element_name, ServiceElementType::EVENT}, instance_specifier_b};
    const ServiceElementInstanceIdentifierView service_element_instance_identifier_view_c{
        {service_type_name, service_element_name, ServiceElementType::EVENT}, instance_specifier_c};

    // Hashes should not collide even if the only difference is the first letter
    // of the instance specifier
    auto hash_b = std::hash<ServiceElementInstanceIdentifierView>{}(service_element_instance_identifier_view_b);
    auto hash_c = std::hash<ServiceElementInstanceIdentifierView>{}(service_element_instance_identifier_view_c);
    EXPECT_FALSE(hash_b == hash_c);
}
}  // namespace
}  // namespace score::mw::com::impl::tracing
