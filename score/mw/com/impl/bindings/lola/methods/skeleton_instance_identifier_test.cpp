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
#include "score/mw/com/impl/bindings/lola/methods/skeleton_instance_identifier.h"

#include "score/mw/com/impl/configuration/global_configuration.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <functional>
#include <limits>

namespace score::mw::com::impl::lola
{
namespace
{

constexpr LolaServiceId kDummyServiceId{10U};
constexpr LolaServiceInstanceId::InstanceId kDummyInstanceId{15U};

TEST(SkeletonInstanceIdentifierHashTest, EqualObjectsReturnTheSameHash)
{
    // Given two SkeletonInstanceIdentifier objects containing the same values
    const SkeletonInstanceIdentifier unit_0{kDummyServiceId, kDummyInstanceId};
    const SkeletonInstanceIdentifier unit_1{kDummyServiceId, kDummyInstanceId};

    // When hashing the two objects
    auto hash_result0 = std::hash<SkeletonInstanceIdentifier>{}(unit_0);
    auto hash_result1 = std::hash<SkeletonInstanceIdentifier>{}(unit_1);

    // Then the hash results are the same
    EXPECT_EQ(hash_result0, hash_result1);
}

TEST(SkeletonInstanceIdentifierHashTest, EqualObjectsWithMaxValuesReturnTheSameHash)
{
    // Given two SkeletonInstanceIdentifier objects containing max values
    const SkeletonInstanceIdentifier unit_0{std::numeric_limits<LolaServiceId>::max(),
                                            std::numeric_limits<LolaServiceInstanceId::InstanceId>::max()};
    const SkeletonInstanceIdentifier unit_1{std::numeric_limits<LolaServiceId>::max(),
                                            std::numeric_limits<LolaServiceInstanceId::InstanceId>::max()};

    // When hashing the two objects
    auto hash_result0 = std::hash<SkeletonInstanceIdentifier>{}(unit_0);
    auto hash_result1 = std::hash<SkeletonInstanceIdentifier>{}(unit_1);

    // Then the hash results are the same
    EXPECT_EQ(hash_result0, hash_result1);
}

TEST(SkeletonInstanceIdentifierHashTest, ObjectsWithDifferentServiceIdsReturnsDifferentHash)
{
    // Given two SkeletonInstanceIdentifier objects containing different service IDs
    const SkeletonInstanceIdentifier unit_0{kDummyServiceId, kDummyInstanceId};
    const SkeletonInstanceIdentifier unit_1{kDummyServiceId + 1U, kDummyInstanceId};

    // When hashing the two objects
    auto hash_result0 = std::hash<SkeletonInstanceIdentifier>{}(unit_0);
    auto hash_result1 = std::hash<SkeletonInstanceIdentifier>{}(unit_1);

    // Then the hash results are different
    EXPECT_NE(hash_result0, hash_result1);
}

TEST(SkeletonInstanceIdentifierHashTest, ObjectsWithDifferentInstanceIdsReturnsDifferentHash)
{
    // Given two SkeletonInstanceIdentifier objects containing different instance IDs
    const SkeletonInstanceIdentifier unit_0{kDummyServiceId, kDummyInstanceId + 1U};
    const SkeletonInstanceIdentifier unit_1{kDummyServiceId, kDummyInstanceId};

    // When hashing the two objects
    auto hash_result0 = std::hash<SkeletonInstanceIdentifier>{}(unit_0);
    auto hash_result1 = std::hash<SkeletonInstanceIdentifier>{}(unit_1);

    // Then the hash results are different
    EXPECT_NE(hash_result0, hash_result1);
}

TEST(SkeletonInstanceIdentifierHashTest, ObjectsWithDifferentApplicationIdAndUniqueIdentifierReturnsDifferentHash)
{
    // Given two SkeletonInstanceIdentifier objects containing different application Ids and unique
    // identifiers
    const SkeletonInstanceIdentifier unit_0{kDummyServiceId, kDummyInstanceId};
    const SkeletonInstanceIdentifier unit_1{kDummyServiceId + 1U, kDummyInstanceId + 1U};

    // When hashing the two objects
    auto hash_result0 = std::hash<SkeletonInstanceIdentifier>{}(unit_0);
    auto hash_result1 = std::hash<SkeletonInstanceIdentifier>{}(unit_1);

    // Then the hash results are different
    EXPECT_NE(hash_result0, hash_result1);
}

TEST(SkeletonInstanceIdentifierHashTest, OperatorStreamOutputsExpectedString)
{
    // Given a SkeletonInstanceIdentifier
    const SkeletonInstanceIdentifier unit{kDummyServiceId, kDummyInstanceId};
    testing::internal::CaptureStdout();

    // When streaming the SkeletonInstanceIdentifier to a log
    score::mw::log::LogFatal("test") << unit;
    std::string output = testing::internal::GetCapturedStdout();

    // Then the output should contain the expected string
    EXPECT_THAT(output, ::testing::HasSubstr("Service ID: 10 . Instance ID: 15"));
}

}  // namespace
}  // namespace score::mw::com::impl::lola
