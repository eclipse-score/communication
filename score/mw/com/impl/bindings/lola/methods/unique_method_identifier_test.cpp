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

constexpr LolaServiceElementId kDummyId{42U};
constexpr LolaServiceElementId kDummyId2{43U};

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
    const UniqueMethodIdentifier lhs{std::numeric_limits<LolaServiceElementId>::max(), MethodType::kSet};
    const UniqueMethodIdentifier rhs{std::numeric_limits<LolaServiceElementId>::max(), MethodType::kSet};

    // When hashing the two objects
    const auto hash_result_lhs = std::hash<UniqueMethodIdentifier>{}(lhs);
    const auto hash_result_rhs = std::hash<UniqueMethodIdentifier>{}(rhs);

    // Then the hash results are the same
    EXPECT_EQ(hash_result_lhs, hash_result_rhs);
}

TEST(UniqueMethodIdentifierTest, TestifMaxIdWithDifferentMethodTypeReturnsDifferentHash)
{
    // Given two UniqueMethodIdentifier objects with max id but different MethodType
    constexpr auto kMaxId = std::numeric_limits<LolaServiceElementId>::max();
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

class UniqueMethodIdentifierPararaterisedFixture
    : public ::testing::TestWithParam<std::pair<UniqueMethodIdentifier, std::string>>
{
};

INSTANTIATE_TEST_SUITE_P(
    UniqueMethodIdentifierPararaterisedFixture,
    UniqueMethodIdentifierPararaterisedFixture,
    ::testing::Values(
        std::make_pair(UniqueMethodIdentifier{42, MethodType::kMethod}, "MethodOrFieldId: 42. MethodType: Method"),
        std::make_pair(UniqueMethodIdentifier{42, MethodType::kGet}, "MethodOrFieldId: 42. MethodType: Get"),
        std::make_pair(UniqueMethodIdentifier{42, MethodType::kSet}, "MethodOrFieldId: 42. MethodType: Set"),
        std::make_pair(UniqueMethodIdentifier{42, MethodType::kUnknown}, "MethodOrFieldId: 42. MethodType: Unknown")));

TEST_P(UniqueMethodIdentifierPararaterisedFixture, TestIfStreamOperatorReturnsExpectedString)
{
    // Given a UniqueMethodIdentifier object
    const UniqueMethodIdentifier unique_method_identifier = GetParam().first;

    // When streaming it to a standard ostream
    std::stringstream buffer{};
    buffer << unique_method_identifier;

    // Then the result is as expected
    const std::string expected_string = GetParam().second;
    EXPECT_EQ(buffer.str(), expected_string);
}

TEST_P(UniqueMethodIdentifierPararaterisedFixture, TestIfScoreLogLogsExpectedString)
{
    // Given a UniqueMethodIdentifier object
    const UniqueMethodIdentifier unique_method_identifier = GetParam().first;

    // and given a mocked LogRecorder which calls StartRecord with a unique SlotHandle
    mw::log::RecorderMock recorder_mock{};
    score::mw::log::SetLogRecorder(&recorder_mock);
    mw::log::SlotHandle handle{10};
    ON_CALL(recorder_mock, StartRecord(_, mw::log::LogLevel::kDebug)).WillByDefault(Return(handle));

    // Expecting that the UniqueMethodIdentifier will be logged
    {
        ::testing::InSequence s;
        EXPECT_CALL(recorder_mock, LogStringView(handle, "MethodOrFieldId: "));
        EXPECT_CALL(recorder_mock, LogUint16(handle, unique_method_identifier.method_or_field_id));
        EXPECT_CALL(recorder_mock, LogStringView(handle, ". MethodType: "));
        EXPECT_CALL(recorder_mock, LogStringView(handle, to_string(unique_method_identifier.method_type)));
    }

    // When logging it with score log
    score::mw::log::LogDebug() << unique_method_identifier;
}

}  // namespace
}  // namespace score::mw::com::impl::lola
