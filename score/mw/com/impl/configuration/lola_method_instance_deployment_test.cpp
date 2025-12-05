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
#include "score/mw/com/impl/configuration/lola_method_instance_deployment.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

namespace score::mw::com::impl
{
namespace
{

TEST(LolaMethodInstanceDeploymentTest, EqualityOperatorWithSameQueueSize)
{
    // Given two LolaMethodInstanceDeployments with the same queue size
    LolaMethodInstanceDeployment unit1{50U};
    LolaMethodInstanceDeployment unit2{50U};

    // When comparing them
    // Then they should be equal
    EXPECT_EQ(unit1, unit2);
}

TEST(LolaMethodInstanceDeploymentTest, EqualityOperatorWithDifferentQueueSize)
{
    // Given two LolaMethodInstanceDeployments with different queue sizes
    LolaMethodInstanceDeployment unit1{10U};
    LolaMethodInstanceDeployment unit2{20U};

    // When comparing them
    // Then they should not be equal
    EXPECT_FALSE(unit1 == unit2);
}

TEST(LolaMethodInstanceDeploymentTest, DefaultInstancesAreEqual)
{
    // Given two LolaMethodInstanceDeployments constructed with std::nullopt
    LolaMethodInstanceDeployment unit1{std::nullopt};
    LolaMethodInstanceDeployment unit2{std::nullopt};

    // When comparing them
    // Then they should be equal
    EXPECT_EQ(unit1, unit2);
}

TEST(LolaMethodInstanceDeploymentTest, MaxQueueSize)
{
    // Given a LolaMethodInstanceDeployment with maximum queue size
    LolaMethodInstanceDeployment unit{std::numeric_limits<LolaMethodInstanceDeployment::QueueSize>::max()};

    // Then the queue size should match
    EXPECT_EQ(unit.queue_size_, std::numeric_limits<LolaMethodInstanceDeployment::QueueSize>::max());
}

TEST(LolaMethodInstanceDeploymentSerializationTest, CreateFromJsonWithQueueSize)
{
    // Given a JSON object with queueSize
    const LolaMethodInstanceDeployment::QueueSize queue_size{20U};
    score::json::Object json_object{};
    json_object["queueSize"] = score::json::Any{queue_size};

    // When creating from JSON
    auto unit = LolaMethodInstanceDeployment::CreateFromJson(json_object);

    // Then the queue size should match the value from JSON
    EXPECT_EQ(unit.queue_size_, queue_size);
}

TEST(LolaMethodInstanceDeploymentSerializationTest, CreateFromJsonWithoutQueueSizeResultsInNullopt)
{
    // Given an empty JSON object
    score::json::Object json_object{};

    // When creating from JSON
    auto unit = LolaMethodInstanceDeployment::CreateFromJson(json_object);

    // Then the queue size should be std::nullopt
    EXPECT_FALSE(unit.queue_size_.has_value());
    EXPECT_EQ(unit.queue_size_, std::nullopt);
}

TEST(LolaMethodInstanceDeploymentSerializationTest, SerializeAndDeserializePreservesQueueSize)
{
    // Given a LolaMethodInstanceDeployment with custom queue size
    const LolaMethodInstanceDeployment::QueueSize queue_size{100U};
    score::json::Object json_object{};
    json_object["queueSize"] = score::json::Any{queue_size};
    auto original_unit = LolaMethodInstanceDeployment::CreateFromJson(json_object);

    // When serializing and deserializing
    auto serialized = original_unit.Serialize();
    auto reconstructed_unit = LolaMethodInstanceDeployment::CreateFromJson(serialized);

    // Then the queue size should be preserved
    EXPECT_EQ(reconstructed_unit.queue_size_, queue_size);
    EXPECT_EQ(reconstructed_unit, original_unit);
}

TEST(LolaMethodInstanceDeploymentSerializationTest, SerializeIncludesQueueSize)
{
    // Given a LolaMethodInstanceDeployment with queue size 42
    const LolaMethodInstanceDeployment::QueueSize queue_size{42U};
    score::json::Object json_object{};
    json_object["queueSize"] = score::json::Any{queue_size};
    auto unit = LolaMethodInstanceDeployment::CreateFromJson(json_object);

    // When serializing
    auto serialized = unit.Serialize();

    // Then the serialized object should contain the queueSize key and the value is correct
    auto queue_size_iter = serialized.find("queueSize");
    ASSERT_NE(queue_size_iter, serialized.end());
    EXPECT_EQ(queue_size_iter->second.As<LolaMethodInstanceDeployment::QueueSize>().value(), queue_size);
}

}  // namespace
}  // namespace score::mw::com::impl
