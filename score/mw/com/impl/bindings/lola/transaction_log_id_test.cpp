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
#include "score/mw/com/impl/bindings/lola/transaction_log_id.h"

#include "score/mw/com/impl/instance_specifier.h"

#include <gtest/gtest.h>

namespace score
{
namespace mw
{
namespace com
{
namespace impl
{
namespace lola
{
namespace
{

const InstanceSpecifier kSameInstanceSpecifier{InstanceSpecifier::Create("same_instance_specifier").value()};
const InstanceSpecifier kDifferentInstanceSpecifier{InstanceSpecifier::Create("different_instance_specifier").value()};

TEST(TransactionLogIdEqualityTest, SameTransactionLogIdsAreEqual)
{
    const uid_t test_uid{10U};
    const auto test_instance_specifier = InstanceSpecifier::Create("my_test_specifier").value();
    TransactionLogId id1{test_uid, test_instance_specifier.ToString()};
    TransactionLogId id2{test_uid, test_instance_specifier.ToString()};

    EXPECT_EQ(id1, id2);
}

TEST(TransactionLogIdHashTest, CanHash)
{
    const uid_t test_uid{10U};
    const auto test_instance_specifier = InstanceSpecifier::Create("my_test_specifier").value();
    TransactionLogId transaction_log_id{test_uid, test_instance_specifier.ToString()};
    static auto unit = std::hash<TransactionLogId>{}(transaction_log_id);
    ASSERT_NE(unit, 0);
}

TEST(TransactionLogIdHashTest, HashesOfTheSameTransactionLogIdAreEqual)
{
    const uid_t test_uid{10U};
    const auto test_instance_specifier = InstanceSpecifier::Create("my_test_specifier").value();

    TransactionLogId unit{test_uid, test_instance_specifier.ToString()};
    TransactionLogId unit_2{test_uid, test_instance_specifier.ToString()};

    static auto hash_value = std::hash<TransactionLogId>{}(unit);
    static auto hash_value_2 = std::hash<TransactionLogId>{}(unit_2);

    ASSERT_EQ(hash_value, hash_value_2);
}

class TransactionLogIdEqualityFixture : public ::testing::TestWithParam<std::pair<TransactionLogId, TransactionLogId>>
{
};

TEST_P(TransactionLogIdEqualityFixture, DifferentTransactionLogIdsAreNotEqual)
{
    const auto transaction_log_ids = GetParam();

    // Given 2 TransactionLogIds containing different values
    const auto transaction_log_id = transaction_log_ids.first;
    const auto transaction_log_id_2 = transaction_log_ids.second;

    // Then the equality operator should return false
    EXPECT_FALSE(transaction_log_id == transaction_log_id_2);
}

TEST_P(TransactionLogIdEqualityFixture, HashesOfTheDifferentTransactionLogIdsAreNotEqual)
{
    const auto transaction_log_ids = GetParam();

    // Given 2 TransactionLogIds containing different values
    const auto unit = transaction_log_ids.first;
    const auto unit_2 = transaction_log_ids.second;

    // When calculating the hash of the TransactionLogIds
    auto hash_value = std::hash<TransactionLogId>{}(unit);
    auto hash_value_2 = std::hash<TransactionLogId>{}(unit_2);

    // Then the hash value should be different
    ASSERT_NE(hash_value, hash_value_2);
}

INSTANTIATE_TEST_CASE_P(TransactionLogIdEqualityFixture,
                        TransactionLogIdEqualityFixture,
                        ::testing::Values(std::make_pair(TransactionLogId{1U, kSameInstanceSpecifier.ToString()},
                                                         TransactionLogId{2U, kSameInstanceSpecifier.ToString()}),
                                          std::make_pair(TransactionLogId{1U, kSameInstanceSpecifier.ToString()},
                                                         TransactionLogId{1U,
                                                                          kDifferentInstanceSpecifier.ToString()})));

}  // namespace
}  // namespace lola
}  // namespace impl
}  // namespace com
}  // namespace mw
}  // namespace score
