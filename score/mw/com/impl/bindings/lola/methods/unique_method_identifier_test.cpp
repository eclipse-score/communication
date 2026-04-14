/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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
#include "score/mw/com/impl/bindings/lola/methods/unique_method_identifier.h"

#include "score/mw/log/logging.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <functional>
#include <limits>

namespace score::mw::com::impl::lola
{
namespace
{

constexpr LolaMethodOrFieldId kDummyId{42U};
constexpr LolaMethodOrFieldId kDummyId2{43U};

TEST(UniqueMethodIdentifierTest, TestIfEqualObjectsAreEqual)
{
    // Given two UniqueMethodIdentifier objects containing the same values
    const UniqueMethodIdentifier lhs{kDummyId, MethodType::kMethod};
    const UniqueMethodIdentifier rhs{kDummyId, MethodType::kMethod};

    // When comparing the two objects
    const auto result = (lhs == rhs);

    // Then they are equal
    EXPECT_TRUE(result);
}

TEST(UniqueMethodIdentifierTest, TestIfObjectsWithDifferentIdAreNotEqual)
{
    // Given two UniqueMethodIdentifier objects with different method_or_field_ids
    const UniqueMethodIdentifier lhs{kDummyId, MethodType::kMethod};
    const UniqueMethodIdentifier rhs{kDummyId2, MethodType::kMethod};

    // When comparing the two objects
    const auto result = (lhs != rhs);

    // Then they are not equal
    EXPECT_TRUE(result);
}

TEST(UniqueMethodIdentifierTest, TestIfObjectsWithDifferentMethodTypeAreNotEqual)
{
    // Given two UniqueMethodIdentifier objects with the same id but different MethodType
    const UniqueMethodIdentifier lhs{kDummyId, MethodType::kMethod};
    const UniqueMethodIdentifier rhs{kDummyId, MethodType::kGet};

    // When comparing the two objects
    const auto result = (lhs != rhs);

    // Then they are not equal
    EXPECT_TRUE(result);
}

TEST(UniqueMethodIdentifierTest, TestIfEqualObjectsReturnTheSameHash)
{
    // Given two UniqueMethodIdentifier objects containing the same values
    const UniqueMethodIdentifier lhs{kDummyId, MethodType::kGet};
    const UniqueMethodIdentifier rhs{kDummyId, MethodType::kGet};

    // When hashing the two objects
    const auto hash_result_lhs = std::hash<UniqueMethodIdentifier>{}(lhs);
    const auto hash_result_rhs = std::hash<UniqueMethodIdentifier>{}(rhs);

    // Then the hash results are the same
    EXPECT_EQ(hash_result_lhs, hash_result_rhs);
}

TEST(UniqueMethodIdentifierTest, TestIfObjectsWithDifferentMethodTypeReturnDifferentHash)
{
    // Given two UniqueMethodIdentifier objects with the same id but different MethodType
    const UniqueMethodIdentifier get_id{kDummyId, MethodType::kGet};
    const UniqueMethodIdentifier set_id{kDummyId, MethodType::kSet};

    // When hashing the two objects
    const auto hash_result_get = std::hash<UniqueMethodIdentifier>{}(get_id);
    const auto hash_result_set = std::hash<UniqueMethodIdentifier>{}(set_id);

    // Then the hash results are different
    EXPECT_NE(hash_result_get, hash_result_set);
}

TEST(UniqueMethodIdentifierTest, TestifObjectsWithDifferentIdReturnDifferentHash)
{
    // Given two UniqueMethodIdentifier objects with different ids but the same MethodType
    const UniqueMethodIdentifier lhs{kDummyId, MethodType::kMethod};
    const UniqueMethodIdentifier rhs{kDummyId2, MethodType::kMethod};

    // When hashing the two objects
    const auto hash_result_lhs = std::hash<UniqueMethodIdentifier>{}(lhs);
    const auto hash_result_rhs = std::hash<UniqueMethodIdentifier>{}(rhs);

    // Then the hash results are different
    EXPECT_NE(hash_result_lhs, hash_result_rhs);
}

TEST(UniqueMethodIdentifierTest, TestIfUniqueMethodIdentifierWithSameIdWithDifferentMethodTypeReturnDifferentHash)
{
    // Given a METHOD and a GET UniqueMethodIdentifier with the same id
    const UniqueMethodIdentifier method_id{kDummyId, MethodType::kMethod};
    const UniqueMethodIdentifier get_id{kDummyId, MethodType::kGet};

    // When hashing the two objects
    const auto hash_result_method = std::hash<UniqueMethodIdentifier>{}(method_id);
    const auto hash_result_get = std::hash<UniqueMethodIdentifier>{}(get_id);

    // Then the hash results are different
    EXPECT_NE(hash_result_method, hash_result_get);
}

TEST(UniqueMethodIdentifierTest, TestIfEqualObjectsWithMaxValuesReturnTheSameHash)
{
    // Given two UniqueMethodIdentifier objects containing max values
    const UniqueMethodIdentifier lhs{std::numeric_limits<LolaMethodOrFieldId>::max(), MethodType::kSet};
    const UniqueMethodIdentifier rhs{std::numeric_limits<LolaMethodOrFieldId>::max(), MethodType::kSet};

    // When hashing the two objects
    const auto hash_result_lhs = std::hash<UniqueMethodIdentifier>{}(lhs);
    const auto hash_result_rhs = std::hash<UniqueMethodIdentifier>{}(rhs);

    // Then the hash results are the same
    EXPECT_EQ(hash_result_lhs, hash_result_rhs);
}

TEST(UniqueMethodIdentifierTest, TestifMaxIdWithDifferentMethodTypeReturnsDifferentHash)
{
    // Given two UniqueMethodIdentifier objects with max id but different MethodType
    constexpr auto kMaxId = std::numeric_limits<LolaMethodOrFieldId>::max();
    const UniqueMethodIdentifier lhs{kMaxId, MethodType::kGet};
    const UniqueMethodIdentifier rhs{kMaxId, MethodType::kSet};

    // When hashing the two objects
    const auto hash_result_lhs = std::hash<UniqueMethodIdentifier>{}(lhs);
    const auto hash_result_rhs = std::hash<UniqueMethodIdentifier>{}(rhs);

    // Then the hash results are different
    EXPECT_NE(hash_result_lhs, hash_result_rhs);
}

TEST(UniqueMethodIdentifierTest, TestIfGetAndUnknownMethodTypeWithSameIdReturnDifferentHash)
{
    // Given a GET and an UNKNOWN UniqueMethodIdentifier with the same id
    const UniqueMethodIdentifier get_id{kDummyId, MethodType::kGet};
    const UniqueMethodIdentifier unknown_id{kDummyId, MethodType::kUnknown};

    // When hashing the two objects
    const auto hash_result_get = std::hash<UniqueMethodIdentifier>{}(get_id);
    const auto hash_result_unknown = std::hash<UniqueMethodIdentifier>{}(unknown_id);

    // Then the hash results are different
    EXPECT_NE(hash_result_get, hash_result_unknown);
}

}  // namespace
}  // namespace score::mw::com::impl::lola
