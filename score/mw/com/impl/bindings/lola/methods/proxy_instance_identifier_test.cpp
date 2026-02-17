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
#include "score/mw/com/impl/bindings/lola/methods/proxy_instance_identifier.h"

#include "score/mw/com/impl/configuration/global_configuration.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <functional>
#include <limits>

namespace score::mw::com::impl::lola
{
namespace
{

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

TEST(ProxyInstanceIdentifierTest, OperatorStreamOutputsExpectedString)
{
    // Given a ProxyInstanceIdentifier
    const ProxyInstanceIdentifier unit{kDummyProcessIdentifier, kDummyProxyInstanceCounter};
    testing::internal::CaptureStdout();

    // When streaming the ProxyInstanceIdentifier to a log
    score::mw::log::LogFatal("test") << unit;
    std::string output = testing::internal::GetCapturedStdout();

    // Then the output should contain the expected string
    EXPECT_THAT(output, ::testing::HasSubstr("Application ID: 10 . Proxy Instance Counter: 15"));
}

}  // namespace
}  // namespace score::mw::com::impl::lola
