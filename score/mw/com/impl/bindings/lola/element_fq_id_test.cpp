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
#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include <cstdint>

#include "score/mw/log/logging.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <limits>
#include <random>

namespace score::mw::com::impl::lola
{
namespace
{

const std::uint8_t kInvalidType{0U};
const std::uint8_t kEventType{1U};
const std::uint8_t kFieldType{2U};

TEST(ElementFqId, DefaultConstruction)
{
    ElementFqId fqid{};

    EXPECT_EQ(fqid.service_id_, std::numeric_limits<std::uint16_t>::max());
    EXPECT_EQ(fqid.element_id_, std::numeric_limits<std::uint16_t>::max());
    EXPECT_EQ(fqid.instance_id_, std::numeric_limits<std::uint16_t>::max());
    EXPECT_EQ(static_cast<std::uint8_t>(fqid.element_type_), kInvalidType);
}

TEST(ElementFqId, ConstructingEvent)
{
    const std::uint16_t service_id{10U};
    const std::uint16_t element_id{11U};
    const std::uint16_t instance_id{12U};

    ElementFqId fqid(service_id, element_id, instance_id, kEventType);

    EXPECT_EQ(fqid.service_id_, service_id);
    EXPECT_EQ(fqid.element_id_, element_id);
    EXPECT_EQ(fqid.instance_id_, instance_id);
    EXPECT_EQ(static_cast<std::uint8_t>(fqid.element_type_), kEventType);
}

TEST(ElementFqId, ConstructingField)
{
    const std::uint16_t service_id{10U};
    const std::uint16_t element_id{11U};
    const std::uint16_t instance_id{12U};

    ElementFqId fqid(service_id, element_id, instance_id, kFieldType);

    EXPECT_EQ(fqid.service_id_, service_id);
    EXPECT_EQ(fqid.element_id_, element_id);
    EXPECT_EQ(fqid.instance_id_, instance_id);
    EXPECT_EQ(static_cast<std::uint8_t>(fqid.element_type_), kFieldType);
}

TEST(ElementFqId, ConstructingEventEnumConstructor)
{
    const std::uint16_t service_id{10U};
    const std::uint16_t element_id{11U};
    const std::uint16_t instance_id{12U};

    ElementFqId fqid(service_id, element_id, instance_id, ServiceElementType::EVENT);

    EXPECT_EQ(fqid.service_id_, service_id);
    EXPECT_EQ(fqid.element_id_, element_id);
    EXPECT_EQ(fqid.instance_id_, instance_id);
    EXPECT_EQ(static_cast<std::uint8_t>(fqid.element_type_), kEventType);
}

TEST(ElementFqId, ConstructingFieldEnumConstructor)
{
    const std::uint16_t service_id{10U};
    const std::uint16_t element_id{11U};
    const std::uint16_t instance_id{12U};

    ElementFqId fqid(service_id, element_id, instance_id, ServiceElementType::FIELD);

    EXPECT_EQ(fqid.service_id_, service_id);
    EXPECT_EQ(fqid.element_id_, element_id);
    EXPECT_EQ(fqid.instance_id_, instance_id);
    EXPECT_EQ(static_cast<std::uint8_t>(fqid.element_type_), kFieldType);
}

TEST(ElementFqIdDeathTest, ConstructingEventWithInvalidElementTypeTerminates)
{
    const std::uint16_t service_id{10U};
    const std::uint16_t element_id{11U};
    const std::uint16_t instance_id{12U};

    const std::uint8_t invalid_element_type{12U};

    EXPECT_DEATH(ElementFqId(service_id, element_id, instance_id, invalid_element_type), ".*");
}

TEST(ElementFqId, SmallerOnServiceId)
{
    ElementFqId lhs{1, 1, 1, kInvalidType};
    ElementFqId rhs{2, 1, 1, kInvalidType};

    EXPECT_LT(lhs, rhs);
}

TEST(ElementFqId, SmallerOnInstanceId)
{
    ElementFqId lhs{1, 1, 1, kInvalidType};
    ElementFqId rhs{1, 1, 2, kInvalidType};

    EXPECT_LT(lhs, rhs);
}

TEST(ElementFqId, SmallerOnElementId)
{
    ElementFqId lhs{1, 1, 1, kInvalidType};
    ElementFqId rhs{1, 2, 1, kInvalidType};

    EXPECT_LT(lhs, rhs);
}

TEST(ElementFqId, Equal)
{
    ElementFqId lhs{1, 1, 1, kInvalidType};
    ElementFqId rhs{1, 1, 1, kInvalidType};

    EXPECT_FALSE(lhs < rhs);
}

TEST(ElementFqId, HashDeterministic)
{
    // Given an ElementFqId
    ElementFqId fqid{1266, 13, 1, kInvalidType};

    // when hashing the same ElementFqId
    std::hash<ElementFqId> hash;
    auto hash_result1 = hash(fqid);
    auto hash_result2 = hash(fqid);

    // expect, that the same hash value gets computed
    EXPECT_EQ(hash_result1, hash_result2);
}

TEST(ElementFqId, HashDoesNotUseElementType)
{
    // Given 2 ElementFqIds which have the same service, element and instance ID but different element type
    ElementFqId fqid_1{1266, 13, 1, ServiceElementType::EVENT};
    ElementFqId fqid_2{1266, 13, 1, ServiceElementType::FIELD};

    // when hashing the ElementFqIds
    std::hash<ElementFqId> hash;
    auto hash_result_1 = hash(fqid_1);
    auto hash_result_2 = hash(fqid_2);

    // expect, that the same hash value gets computed
    EXPECT_EQ(hash_result_1, hash_result_2);
}

TEST(ElementFqId, HashAcceptableCollisions)
{
    // Given 10 random ElementFqIds
    constexpr size_t fqid_cnt = 10;
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<std::mt19937::result_type> distribUint16(1,
                                                                           std::numeric_limits<std::uint16_t>::max());

    // when the N hashes are calculated
    std::hash<ElementFqId> hash;
    std::array<size_t, fqid_cnt> hash_values{};
    for (size_t& hashval : hash_values)
    {
        hashval = hash(ElementFqId{static_cast<uint16_t>(distribUint16(rng)),
                                   static_cast<uint16_t>(distribUint16(rng)),
                                   static_cast<uint16_t>(distribUint16(rng)),
                                   kInvalidType});
    }

    // expect, that we do not have more than one duplicate
    std::sort(hash_values.begin(), hash_values.end());
    auto it = std::adjacent_find(hash_values.begin(), hash_values.end());
    bool hasDuplicates = it != hash_values.end();
    if (hasDuplicates)
    {
        // search for a 2nd duplicate
        it = std::adjacent_find(it, hash_values.end());
        hasDuplicates = it != hash_values.end();
    }
    EXPECT_FALSE(hasDuplicates);
}

TEST(ElementFqId, ToStringEvent)
{
    // Given an ElementFqId
    ElementFqId fqid{1266, 13, 1, ServiceElementType::EVENT};

    // when calling ToString()
    const auto str = fqid.ToString();

    // expect, that result matches expectation
    const auto expected_str = "ElementFqId{S:1266, E:13, I:1, T:1}";
    EXPECT_EQ(str, expected_str);
}

TEST(ElementFqId, ToStringField)
{
    // Given an ElementFqId
    ElementFqId fqid{1266, 13, 1, ServiceElementType::FIELD};

    // when calling ToString()
    const auto str = fqid.ToString();

    // expect, that result matches expectation
    const auto expected_str = "ElementFqId{S:1266, E:13, I:1, T:2}";
    EXPECT_EQ(str, expected_str);
}

TEST(ElementFqId, OverloadedOperatorDoesNotCrash)
{
    // This test is to complete code coverage.
    // Given an ElementFqId
    ElementFqId fqid{1266, 13, 1, kInvalidType};

    // when calling the overloaded operator
    ::score::mw::log::LogDebug("lola") << fqid;

    // we do not crash
}

TEST(ElementFqId, Equality_True)
{
    ElementFqId lhs{1, 1, 1, kInvalidType};
    ElementFqId rhs{1, 1, 1, kInvalidType};

    EXPECT_TRUE(lhs == rhs);
}

TEST(ElementFqId, Equality_False1)
{
    ElementFqId lhs{1, 1, 1, kInvalidType};
    ElementFqId rhs{1, 1, 0, kInvalidType};

    EXPECT_FALSE(lhs == rhs);
}

TEST(ElementFqId, Equality_False2)
{
    ElementFqId lhs{1, 1, 1, kInvalidType};
    ElementFqId rhs{1, 5, 1, kInvalidType};

    EXPECT_FALSE(lhs == rhs);
}

TEST(ElementFqId, Equality_False3)
{
    ElementFqId lhs{1, 1, 1, kInvalidType};
    ElementFqId rhs{2, 1, 1, kInvalidType};

    EXPECT_FALSE(lhs == rhs);
}

TEST(ElementFqId, Equality_DoesNotUseElementType)
{
    ElementFqId lhs{1, 1, 1, ServiceElementType::EVENT};
    ElementFqId rhs{1, 1, 1, ServiceElementType::FIELD};

    EXPECT_TRUE(lhs == rhs);
}

TEST(ElementFqId, DifferentHashesAreGeneratedFor8And16BitInstanceIds)
{
    ElementFqId fqid8bit{1266, 13, 0, kInvalidType};
    ElementFqId fqid16bit{1266, 13, 256, kInvalidType};

    std::hash<ElementFqId> hash;
    auto hash_result1 = hash(fqid8bit);
    auto hash_result2 = hash(fqid16bit);

    EXPECT_NE(hash_result1, hash_result2);
}

TEST(ElementFqId, InsertionOperator_DoesNotCrash)
{
    ElementFqId element_fq_id{1, 1, 1, kInvalidType};
    ::score::mw::log::LogDebug("lola") << element_fq_id;
}

TEST(ElementFqId, IsElementEventReturnsTrueOnlyForEventType)
{
    EXPECT_TRUE(IsElementEvent(ElementFqId{1, 1, 1, ServiceElementType::EVENT}));
    EXPECT_FALSE(IsElementEvent(ElementFqId{1, 1, 1, ServiceElementType::FIELD}));
    EXPECT_FALSE(IsElementEvent(ElementFqId{1, 1, 1, ServiceElementType::INVALID}));
    EXPECT_FALSE(IsElementEvent(ElementFqId{1, 1, 1, static_cast<ServiceElementType>(100U)}));
}

TEST(ElementFqId, IsElementFieldReturnsTrueOnlyForFieldType)
{
    EXPECT_TRUE(IsElementField(ElementFqId{1, 1, 1, ServiceElementType::FIELD}));
    EXPECT_FALSE(IsElementField(ElementFqId{1, 1, 1, ServiceElementType::EVENT}));
    EXPECT_FALSE(IsElementField(ElementFqId{1, 1, 1, ServiceElementType::INVALID}));
    EXPECT_FALSE(IsElementEvent(ElementFqId{1, 1, 1, static_cast<ServiceElementType>(100U)}));
}

}  // namespace
}  // namespace score::mw::com::impl::lola
