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
#include "score/mw/com/impl/bindings/lola/proxy_instance_identifier.h"

#include "score/mw/com/impl/configuration/global_configuration.h"

#include "score/mw/log/logging.h"
#include "score/mw/log/recorder_mock.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <functional>
#include <limits>
#include <sstream>

namespace score::mw::com::impl::lola
{
namespace
{

using namespace ::testing;

constexpr GlobalConfiguration::ApplicationId kDummyProcessIdentifier{10U};
constexpr ProxyInstanceIdentifier::ProxyInstanceCounter kDummyProxyInstanceCounter{15U};

TEST(ProxyInstanceIdentifierHashTest, EqualObjectsReturnTheSameHash)
{
    // Given two ProxyInstanceIdentifier objects containing the same values
    const ProxyInstanceIdentifier unit_0{kDummyProcessIdentifier, kDummyProxyInstanceCounter};
    const ProxyInstanceIdentifier unit_1{kDummyProcessIdentifier, kDummyProxyInstanceCounter};

    // When hashing the two objects
    auto hash_result0 = std::hash<ProxyInstanceIdentifier>{}(unit_0);
    auto hash_result1 = std::hash<ProxyInstanceIdentifier>{}(unit_1);

    // Then the hash results are the same
    EXPECT_EQ(hash_result0, hash_result1);
}

TEST(ProxyInstanceIdentifierHashTest, EqualObjectsWithMaxValuesReturnTheSameHash)
{
    // Given two ProxyInstanceIdentifier objects containing max values
    const ProxyInstanceIdentifier unit_0{std::numeric_limits<GlobalConfiguration::ApplicationId>::max(),
                                         std::numeric_limits<ProxyInstanceIdentifier::ProxyInstanceCounter>::max()};
    const ProxyInstanceIdentifier unit_1{std::numeric_limits<GlobalConfiguration::ApplicationId>::max(),
                                         std::numeric_limits<ProxyInstanceIdentifier::ProxyInstanceCounter>::max()};

    // When hashing the two objects
    auto hash_result0 = std::hash<ProxyInstanceIdentifier>{}(unit_0);
    auto hash_result1 = std::hash<ProxyInstanceIdentifier>{}(unit_1);

    // Then the hash results are the same
    EXPECT_EQ(hash_result0, hash_result1);
}

TEST(ProxyInstanceIdentifierHashTest, ObjectsWithDifferentProcessIdentifierReturnsDifferentHash)
{
    // Given two ProxyInstanceIdentifier objects containing different process identifiers
    const ProxyInstanceIdentifier unit_0{kDummyProcessIdentifier, kDummyProxyInstanceCounter};
    const ProxyInstanceIdentifier unit_1{kDummyProcessIdentifier + 1U, kDummyProxyInstanceCounter};

    // When hashing the two objects
    auto hash_result0 = std::hash<ProxyInstanceIdentifier>{}(unit_0);
    auto hash_result1 = std::hash<ProxyInstanceIdentifier>{}(unit_1);

    // Then the hash results are different
    EXPECT_NE(hash_result0, hash_result1);
}

TEST(ProxyInstanceIdentifierHashTest, ObjectsWithDifferentProxyInstanceCountersReturnsDifferentHash)
{
    // Given two ProxyInstanceIdentifier objects containing different proxy instance counters
    const ProxyInstanceIdentifier unit_0{kDummyProcessIdentifier, kDummyProxyInstanceCounter + 1U};
    const ProxyInstanceIdentifier unit_1{kDummyProcessIdentifier, kDummyProxyInstanceCounter};

    // When hashing the two objects
    auto hash_result0 = std::hash<ProxyInstanceIdentifier>{}(unit_0);
    auto hash_result1 = std::hash<ProxyInstanceIdentifier>{}(unit_1);

    // Then the hash results are different
    EXPECT_NE(hash_result0, hash_result1);
}

TEST(ProxyInstanceIdentifierHashTest,
     ObjectsWithDifferentProcessIdentifiersAndProxyInstanceCountersReturnsDifferentHash)
{
    // Given two ProxyInstanceIdentifier objects containing different process identifiers and proxy instance counters
    const ProxyInstanceIdentifier unit_0{kDummyProcessIdentifier, kDummyProxyInstanceCounter};
    const ProxyInstanceIdentifier unit_1{kDummyProcessIdentifier + 1U, kDummyProxyInstanceCounter + 1U};

    // When hashing the two objects
    auto hash_result0 = std::hash<ProxyInstanceIdentifier>{}(unit_0);
    auto hash_result1 = std::hash<ProxyInstanceIdentifier>{}(unit_1);

    // Then the hash results are different
    EXPECT_NE(hash_result0, hash_result1);
}

class ProxyInstanceIdentifierParamaterisedFixture
    : public ::testing::TestWithParam<std::pair<ProxyInstanceIdentifier, std::string>>
{
};

INSTANTIATE_TEST_SUITE_P(
    ToStringTests,
    ProxyInstanceIdentifierParamaterisedFixture,
    ::testing::Values(std::make_pair(ProxyInstanceIdentifier{10, 15}, "Application ID: 10. Proxy Instance Counter: 15"),
                      std::make_pair(
                          ProxyInstanceIdentifier{
                              std::numeric_limits<GlobalConfiguration::ApplicationId>::max(),
                              std::numeric_limits<ProxyInstanceIdentifier::ProxyInstanceCounter>::max()},
                          "Application ID: 4294967295. Proxy Instance Counter: 65535")));

TEST_P(ProxyInstanceIdentifierParamaterisedFixture, StreamOperatorOutputsExpectedString)
{
    // Given a ProxyInstanceIdentifier and its expected string representation
    const ProxyInstanceIdentifier unit = GetParam().first;

    // When streaming it to a standard ostream
    std::stringstream buffer{};
    buffer << unit;

    // Then the result should be the expected string
    const std::string expected_string = GetParam().second;
    EXPECT_EQ(buffer.str(), expected_string);
}

TEST_P(ProxyInstanceIdentifierParamaterisedFixture, LogStreamOperatorOutputsExpectedString)
{
    // Given a ProxyInstanceIdentifier object
    const ProxyInstanceIdentifier proxy_instance_identifier = GetParam().first;

    // and given a mocked LogRecorder which calls StartRecord with a unique SlotHandle
    mw::log::RecorderMock recorder_mock{};
    score::mw::log::SetLogRecorder(&recorder_mock);
    mw::log::SlotHandle handle{10};
    ON_CALL(recorder_mock, StartRecord(_, mw::log::LogLevel::kDebug)).WillByDefault(Return(handle));

    // Expecting that the ProxyInstanceIdentifier will be logged
    {
        ::testing::InSequence s;
        EXPECT_CALL(recorder_mock, LogStringView(handle, "Application ID: "));
        EXPECT_CALL(recorder_mock, LogUint32(handle, proxy_instance_identifier.application_id));
        EXPECT_CALL(recorder_mock, LogStringView(handle, ". Proxy Instance Counter: "));
        EXPECT_CALL(recorder_mock, LogUint16(handle, proxy_instance_identifier.proxy_instance_counter));
    }

    // When logging it with score log
    score::mw::log::LogDebug() << proxy_instance_identifier;
}

}  // namespace
}  // namespace score::mw::com::impl::lola
