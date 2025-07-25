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
#include "score/mw/com/impl/tracing/configuration/trace_point_key.h"

#include "score/mw/com/impl/service_element_type.h"
#include "score/mw/com/impl/tracing/configuration/service_element_identifier_view.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl::tracing
{
namespace
{

const ServiceElementIdentifierView kServiceElementIdentifier{"type_name", "element_name", ServiceElementType::EVENT};
const std::uint8_t kTracePointType{1U};

TEST(TracePointKeyHashTest, CanHash)
{
    // Given a TracePointKey
    const TracePointKey trace_point_key{kServiceElementIdentifier, kTracePointType};

    // When calculating the hash of a TracePointKey
    auto hash_value = std::hash<TracePointKey>{}(trace_point_key);

    // Then the hash value should be non-zero
    ASSERT_NE(hash_value, 0);
}

TEST(TracePointKeyHashTest, CanUseAsKeyInMap)
{
    // Given a TracePointKey
    const TracePointKey trace_point_key{kServiceElementIdentifier, kTracePointType};

    // When using a TracePointKey as a key in a map
    std::unordered_map<TracePointKey, int> my_map{std::make_pair(trace_point_key, 10)};

    // Then we compile and don't crash
}

TEST(TracePointKeyHashTest, HashesOfTheSameTracePointKeyAreEqual)
{
    // Given 2 TracePointKeys with containing the same values
    ServiceElementIdentifierView service_element_identifier_view{
        "service_type_name", "service_element_name", ServiceElementType::EVENT};
    ServiceElementIdentifierView service_element_identifier_view_2{
        "service_type_name", "service_element_name", ServiceElementType::EVENT};

    const TracePointKey trace_point_key{service_element_identifier_view, kTracePointType};
    const TracePointKey trace_point_key_2{service_element_identifier_view_2, kTracePointType};

    // When calculating the hash of the TracePointKeys
    auto hash_value = std::hash<TracePointKey>{}(trace_point_key);
    auto hash_value_2 = std::hash<TracePointKey>{}(trace_point_key_2);

    // Then the hash value should be equal
    ASSERT_EQ(hash_value, hash_value_2);
}

class TracePointKeyHashFixture : public ::testing::TestWithParam<std::array<TracePointKey, 2>>
{
};

TEST_P(TracePointKeyHashFixture, HashesOfTheDifferentTracePointKeyAreNotEqual)
{
    const auto trace_point_keys = GetParam();

    // Given 2 TracePointKeys containing different values
    const auto trace_point_key = trace_point_keys.at(0);
    const auto trace_point_key_2 = trace_point_keys.at(1);

    // When calculating the hash of the TracePointKeys
    auto hash_value = std::hash<TracePointKey>{}(trace_point_key);
    auto hash_value_2 = std::hash<TracePointKey>{}(trace_point_key_2);

    // Then the hash value should be different
    ASSERT_NE(hash_value, hash_value_2);
}

// Test that each element that should be used in the hashing algorithm is used by changing them one at a time.
INSTANTIATE_TEST_CASE_P(
    TracePointKeyHashDifferentKeys,
    TracePointKeyHashFixture,
    ::testing::Values(
        std::array<TracePointKey, 2>{
            TracePointKey{
                ServiceElementIdentifierView{"same_type_name", "same_element_name", ServiceElementType::EVENT},
                1U},
            TracePointKey{
                ServiceElementIdentifierView{"different_type_name", "same_element_name", ServiceElementType::EVENT},
                1U}},
        std::array<TracePointKey, 2>{
            TracePointKey{
                ServiceElementIdentifierView{"same_type_name", "same_element_name", ServiceElementType::EVENT},
                1U},
            TracePointKey{
                ServiceElementIdentifierView{"same_type_name", "different_element_name", ServiceElementType::EVENT},
                1U}},
        std::array<TracePointKey, 2>{
            TracePointKey{
                ServiceElementIdentifierView{"same_type_name", "same_element_name", ServiceElementType::EVENT},
                1U},
            TracePointKey{
                ServiceElementIdentifierView{"same_type_name", "same_element_name", ServiceElementType::FIELD},
                1U}},
        std::array<TracePointKey, 2>{
            TracePointKey{
                ServiceElementIdentifierView{"same_type_name", "same_element_name", ServiceElementType::EVENT},
                1U},
            TracePointKey{
                ServiceElementIdentifierView{"same_type_name", "same_element_name", ServiceElementType::EVENT},
                2U}}));

TEST(TracePointKeyHashDeathTest, HashingTracePointKeyWithTooLongStringsTerminates)
{
    constexpr std::size_t max_buffer_size{1024U};

    std::string service_type_name(max_buffer_size, 'a');
    std::string service_element_name(max_buffer_size, 'b');

    // Given 2 TracePointKeys with containing the same values
    ServiceElementIdentifierView service_element_identifier_view{
        service_type_name, service_element_name, ServiceElementType::EVENT};
    const TracePointKey trace_point_key{service_element_identifier_view, kTracePointType};

    // When calculating the hash of the TracePointKeys
    EXPECT_DEATH(std::hash<TracePointKey>{}(trace_point_key), ".*");
}

}  // namespace
}  // namespace score::mw::com::impl::tracing
