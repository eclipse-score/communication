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

class ProxyMethodInstanceIdentifierParamaterisedFixture
    : public ::testing::TestWithParam<std::pair<ProxyMethodInstanceIdentifier, std::string>>
{
};

INSTANTIATE_TEST_SUITE_P(
    ToStringTest,
    ProxyMethodInstanceIdentifierParamaterisedFixture,
    ::testing::Values(
        std::make_pair(ProxyMethodInstanceIdentifier{{10, 15}, {20, MethodType::kMethod}},
                       "Application ID: 10. Proxy Instance Counter: 15. MethodOrFieldId: 20. MethodType: Method"),
        std::make_pair(ProxyMethodInstanceIdentifier{{10, 15}, {20, MethodType::kSet}},
                       "Application ID: 10. Proxy Instance Counter: 15. MethodOrFieldId: 20. MethodType: Set"),
        std::make_pair(
            ProxyMethodInstanceIdentifier{{std::numeric_limits<GlobalConfiguration::ApplicationId>::max(),
                                           std::numeric_limits<ProxyInstanceIdentifier::ProxyInstanceCounter>::max()},
                                          {std::numeric_limits<LolaServiceElementId>::max(), MethodType::kGet}},
            "Application ID: 4294967295. Proxy Instance Counter: 65535. MethodOrFieldId: 65535. MethodType: "
            "Get")));

TEST_P(ProxyMethodInstanceIdentifierParamaterisedFixture, StreamOperatorReturnsExpectedString)
{
    // Given a ProxyMethodInstanceIdentifier and the expected string representation
    const ProxyMethodInstanceIdentifier unit = GetParam().first;

    // When streaming the ProxyMethodInstanceIdentifier to a standard ostream
    std::stringstream buffer{};
    buffer << unit;

    // Then the result is the expected string
    const std::string expected_string = GetParam().second;
    EXPECT_EQ(buffer.str(), expected_string);
}

TEST_P(ProxyMethodInstanceIdentifierParamaterisedFixture, LogStreamOperatorReturnsExpectedString)
{
    // Given a ProxyMethodInstanceIdentifier object
    const ProxyMethodInstanceIdentifier proxy_method_instance_identifier = GetParam().first;

    // and given a mocked LogRecorder which calls StartRecord with a unique SlotHandle
    mw::log::RecorderMock recorder_mock{};
    score::mw::log::SetLogRecorder(&recorder_mock);
    mw::log::SlotHandle handle{10};
    ON_CALL(recorder_mock, StartRecord(_, mw::log::LogLevel::kDebug)).WillByDefault(Return(handle));

    // Expecting that the ProxyMethodInstanceIdentifier will be logged
    {
        ::testing::InSequence s;
        EXPECT_CALL(recorder_mock, LogStringView(handle, "Application ID: "));
        EXPECT_CALL(recorder_mock,
                    LogUint32(handle, proxy_method_instance_identifier.proxy_instance_identifier.application_id));
        EXPECT_CALL(recorder_mock, LogStringView(handle, ". Proxy Instance Counter: "));
        EXPECT_CALL(
            recorder_mock,
            LogUint16(handle, proxy_method_instance_identifier.proxy_instance_identifier.proxy_instance_counter));
        EXPECT_CALL(recorder_mock, LogStringView(handle, ". "));
        EXPECT_CALL(recorder_mock, LogStringView(handle, "MethodOrFieldId: "));
        EXPECT_CALL(recorder_mock,
                    LogUint16(handle, proxy_method_instance_identifier.unique_method_identifier.method_or_field_id));
        EXPECT_CALL(recorder_mock, LogStringView(handle, ". MethodType: "));
        EXPECT_CALL(
            recorder_mock,
            LogStringView(handle, to_string(proxy_method_instance_identifier.unique_method_identifier.method_type)));
    }

    // When logging it with score log
    score::mw::log::LogDebug() << proxy_method_instance_identifier;
}

}  // namespace
}  // namespace score::mw::com::impl::lola
