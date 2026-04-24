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
#include "score/mw/com/impl/bindings/lola/methods/proxy_method_instance_identifier.h"

#include "score/mw/com/impl/configuration/global_configuration.h"
#include "score/mw/com/impl/configuration/lola_service_element_id.h"

#include "score/mw/log/logging.h"

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
constexpr LolaServiceElementId kDummyMethodOrFieldId{20U};
const UniqueMethodIdentifier kDummyUniqueMethodIdentifier{kDummyMethodOrFieldId, MethodType::kMethod};

TEST(ProxyMethodInstanceIdentifierTest, EqualObjectsReturnTheSameHash)
{
    // Given two ProxyMethodInstanceIdentifier objects containing the same values
    const ProxyMethodInstanceIdentifier unit_0{{kDummyProcessIdentifier, kDummyProxyInstanceCounter},
                                               kDummyUniqueMethodIdentifier};
    const ProxyMethodInstanceIdentifier unit_1{{kDummyProcessIdentifier, kDummyProxyInstanceCounter},
                                               kDummyUniqueMethodIdentifier};

    // When hashing the two objects
    auto hash_result0 = std::hash<ProxyMethodInstanceIdentifier>{}(unit_0);
    auto hash_result1 = std::hash<ProxyMethodInstanceIdentifier>{}(unit_1);

    // Then the hash results are the same
    EXPECT_EQ(hash_result0, hash_result1);
}

TEST(ProxyMethodInstanceIdentifierTest, EqualObjectsWithMaxValuesReturnTheSameHash)
{
    // Given two ProxyMethodInstanceIdentifier objects containing max values
    const ProxyMethodInstanceIdentifier unit_0{
        {std::numeric_limits<GlobalConfiguration::ApplicationId>::max(),
         std::numeric_limits<ProxyInstanceIdentifier::ProxyInstanceCounter>::max()},
        {std::numeric_limits<LolaServiceElementId>::max(), MethodType::kSet}};
    const ProxyMethodInstanceIdentifier unit_1{
        {std::numeric_limits<GlobalConfiguration::ApplicationId>::max(),
         std::numeric_limits<ProxyInstanceIdentifier::ProxyInstanceCounter>::max()},
        {std::numeric_limits<LolaServiceElementId>::max(), MethodType::kSet}};

    // When hashing the two objects
    auto hash_result0 = std::hash<ProxyMethodInstanceIdentifier>{}(unit_0);
    auto hash_result1 = std::hash<ProxyMethodInstanceIdentifier>{}(unit_1);

    // Then the hash results are the same
    EXPECT_EQ(hash_result0, hash_result1);
}

TEST(ProxyMethodInstanceIdentifierTest, ObjectsWithDifferentProcessIdentifierReturnsDifferentHash)
{
    // Given two ProxyMethodInstanceIdentifier objects containing different process identifiers
    const ProxyMethodInstanceIdentifier unit_0{{kDummyProcessIdentifier, kDummyProxyInstanceCounter},
                                               kDummyUniqueMethodIdentifier};
    const ProxyMethodInstanceIdentifier unit_1{{kDummyProcessIdentifier + 1U, kDummyProxyInstanceCounter},
                                               kDummyUniqueMethodIdentifier};

    // When hashing the two objects
    auto hash_result0 = std::hash<ProxyMethodInstanceIdentifier>{}(unit_0);
    auto hash_result1 = std::hash<ProxyMethodInstanceIdentifier>{}(unit_1);

    // Then the hash results are different
    EXPECT_NE(hash_result0, hash_result1);
}

TEST(ProxyMethodInstanceIdentifierTest, ObjectsWithDifferentProxyInstanceCountersReturnsDifferentHash)
{
    // Given two ProxyMethodInstanceIdentifier objects containing different proxy instance counters
    const ProxyMethodInstanceIdentifier unit_0{{kDummyProcessIdentifier, kDummyProxyInstanceCounter + 1U},
                                               kDummyUniqueMethodIdentifier};
    const ProxyMethodInstanceIdentifier unit_1{{kDummyProcessIdentifier, kDummyProxyInstanceCounter},
                                               kDummyUniqueMethodIdentifier};

    // When hashing the two objects
    auto hash_result0 = std::hash<ProxyMethodInstanceIdentifier>{}(unit_0);
    auto hash_result1 = std::hash<ProxyMethodInstanceIdentifier>{}(unit_1);

    // Then the hash results are different
    EXPECT_NE(hash_result0, hash_result1);
}

TEST(ProxyMethodInstanceIdentifierTest, ObjectsWithDifferentMethodIdsReturnsDifferentHash)
{
    // Given two ProxyMethodInstanceIdentifier objects containing different method ids
    const ProxyMethodInstanceIdentifier unit_0{{kDummyProcessIdentifier, kDummyProxyInstanceCounter},
                                               kDummyUniqueMethodIdentifier};
    const ProxyMethodInstanceIdentifier unit_1{{kDummyProcessIdentifier, kDummyProxyInstanceCounter},
                                               {kDummyMethodOrFieldId + 1U, MethodType::kMethod}};

    // When hashing the two objects
    auto hash_result0 = std::hash<ProxyMethodInstanceIdentifier>{}(unit_0);
    auto hash_result1 = std::hash<ProxyMethodInstanceIdentifier>{}(unit_1);

    // Then the hash results are different
    EXPECT_NE(hash_result0, hash_result1);
}

TEST(ProxyMethodInstanceIdentifierTest,
     ObjectsWithDifferentProcessIdentifiersAndProxyInstanceCountersAndMethodIdReturnsDifferentHash)
{
    // Given two ProxyMethodInstanceIdentifier objects containing different process identifiers, proxy instance
    // counters and method IDs
    const ProxyMethodInstanceIdentifier unit_0{{kDummyProcessIdentifier, kDummyProxyInstanceCounter},
                                               kDummyUniqueMethodIdentifier};
    const ProxyMethodInstanceIdentifier unit_1{{kDummyProcessIdentifier + 1U, kDummyProxyInstanceCounter + 1U},
                                               {kDummyMethodOrFieldId + 1U, MethodType::kMethod}};

    // When hashing the two objects
    auto hash_result0 = std::hash<ProxyMethodInstanceIdentifier>{}(unit_0);
    auto hash_result1 = std::hash<ProxyMethodInstanceIdentifier>{}(unit_1);

    // Then the hash results are different
    EXPECT_NE(hash_result0, hash_result1);
}

TEST(ProxyMethodInstanceIdentifierTest, OperatorStreamOutputsExpectedString)
{
    // Given a ProxyMethodInstanceIdentifier
    const ProxyMethodInstanceIdentifier unit{{kDummyProcessIdentifier, kDummyProxyInstanceCounter},
                                             kDummyUniqueMethodIdentifier};
    testing::internal::CaptureStdout();

    // When streaming the ProxyMethodInstanceIdentifier to a log
    score::mw::log::LogFatal("test") << unit;
    std::string output = testing::internal::GetCapturedStdout();

    // Then the output should contain the expected string
    EXPECT_THAT(output,
                ::testing::HasSubstr("ProxyInstanceIdentifier: Application ID: 10 . Proxy Instance Counter: 15 "
                                     ". UniqueMethodIdentifier: MethodOrFieldId: 20 . MethodType: Method"));
}

}  // namespace
}  // namespace score::mw::com::impl::lola
