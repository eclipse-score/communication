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
#include "score/mw/com/impl/bindings/lola/transaction_log.h"
#include "score/mw/com/impl/bindings/lola/test/transaction_log_test_resources.h"

#include "score/memory/shared/shared_memory_resource_heap_allocator_mock.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace score::mw::com::impl::lola
{
namespace
{

using ::testing::_;
using ::testing::MockFunction;
using ::testing::StrictMock;

const std::size_t kNumberOfSlots{5U};
const TransactionLog::MaxSampleCountType kSubscriptionMaxSampleCount{5U};

const std::size_t kSlotIndex0{0U};
const std::size_t kSlotIndex1{1U};

class TransactionLogFixture : public ::testing::Test
{
  protected:
    TransactionLog::DereferenceSlotCallback GetDereferenceSlotCallbackWrapper() noexcept
    {
        // Since a MockFunction doesn't fit within an score::cpp::callback, we wrap it in a smaller lambda which only stores a
        // pointer to the MockFunction and therefore fits within the score::cpp::callback.
        return [this](const TransactionLog::SlotIndexType slot_index) noexcept {
            dereference_slot_callback_.AsStdFunction()(slot_index);
        };
    }

    TransactionLog::UnsubscribeCallback GetUnsubscribeCallbackWrapper() noexcept
    {
        // Since a MockFunction doesn't fit within an score::cpp::callback, we wrap it in a smaller lambda which only stores a
        // pointer to the MockFunction and therefore fits within the score::cpp::callback.
        return [this](const TransactionLog::MaxSampleCountType subscription_max_sample_count) noexcept {
            unsubscribe_callback_.AsStdFunction()(subscription_max_sample_count);
        };
    }

    memory::shared::SharedMemoryResourceHeapAllocatorMock memory_resource_{1U};
    TransactionLog unit_{kNumberOfSlots, memory_resource_.getMemoryResourceProxy()};

    StrictMock<MockFunction<void(TransactionLog::SlotIndexType)>> dereference_slot_callback_{};
    StrictMock<MockFunction<void(TransactionLog::MaxSampleCountType)>> unsubscribe_callback_{};
};

using TransactionLogProxyElementFixture = TransactionLogFixture;
TEST_F(TransactionLogProxyElementFixture, RollbackWillNotCallCallbackWhenNoTransactionsRecorded)
{
    // Given a valid TransactionLog

    // Expecting that neither callback will be called
    EXPECT_CALL(dereference_slot_callback_, Call(_)).Times(0);
    EXPECT_CALL(unsubscribe_callback_, Call(_)).Times(0);

    // When no transactions are recorded

    // and when rollback is called
    const auto rollback_result =
        unit_.RollbackProxyElementLog(GetDereferenceSlotCallbackWrapper(), GetUnsubscribeCallbackWrapper());

    // Then the result will not contain an error
    EXPECT_TRUE(rollback_result.has_value());
}

TEST_F(TransactionLogProxyElementFixture, RollbackWillNotCallCallbackWhenOnlySubscribeAndUnsubscribeRecorded)
{
    // Given a valid TransactionLog

    // Expecting that neither callback will be called
    EXPECT_CALL(dereference_slot_callback_, Call(_)).Times(0);
    EXPECT_CALL(unsubscribe_callback_, Call(_)).Times(0);

    // When Subscribe is recorded
    unit_.SubscribeTransactionBegin(kSubscriptionMaxSampleCount);
    unit_.SubscribeTransactionCommit();

    // and no transactions are recorded

    // and then Unsubscribe is recorded
    unit_.UnsubscribeTransactionBegin();
    unit_.UnsubscribeTransactionCommit();

    // and when rollback is called
    const auto rollback_result =
        unit_.RollbackProxyElementLog(GetDereferenceSlotCallbackWrapper(), GetUnsubscribeCallbackWrapper());

    // Then the result will not contain an error
    EXPECT_TRUE(rollback_result.has_value());
}

TEST_F(TransactionLogProxyElementFixture, RollbackWillNotCallCallbackAfterDereferencingAndUnsubscribingCompleted)
{
    // Given a valid TransactionLog

    // Expecting that neither callback will be called
    EXPECT_CALL(dereference_slot_callback_, Call(_)).Times(0);
    EXPECT_CALL(unsubscribe_callback_, Call(_)).Times(0);

    // When Subscribe is recorded
    unit_.SubscribeTransactionBegin(kSubscriptionMaxSampleCount);
    unit_.SubscribeTransactionCommit();

    // and 2 slots are succesfully referenced and dereferenced
    unit_.ReferenceTransactionBegin(kSlotIndex0);
    unit_.ReferenceTransactionCommit(kSlotIndex0);
    unit_.ReferenceTransactionBegin(kSlotIndex1);
    unit_.ReferenceTransactionCommit(kSlotIndex1);

    unit_.DereferenceTransactionBegin(kSlotIndex0);
    unit_.DereferenceTransactionCommit(kSlotIndex0);
    unit_.DereferenceTransactionBegin(kSlotIndex1);
    unit_.DereferenceTransactionCommit(kSlotIndex1);

    // and then Unsubscribe is recorded
    unit_.UnsubscribeTransactionBegin();
    unit_.UnsubscribeTransactionCommit();

    // and when rollback is called
    const auto rollback_result =
        unit_.RollbackProxyElementLog(GetDereferenceSlotCallbackWrapper(), GetUnsubscribeCallbackWrapper());

    // Then the result will not contain an error
    EXPECT_TRUE(rollback_result.has_value());
}

TEST_F(TransactionLogProxyElementFixture, RollbackWillNotCallCallbackIfReferencingAborted)
{
    // Given a valid TransactionLog

    // Expecting that neither callback will be called
    EXPECT_CALL(dereference_slot_callback_, Call(_)).Times(0);
    EXPECT_CALL(unsubscribe_callback_, Call(_)).Times(0);

    // When Subscribe is recorded
    unit_.SubscribeTransactionBegin(kSubscriptionMaxSampleCount);
    unit_.SubscribeTransactionCommit();

    // and 2 slots are referenced and aborted
    unit_.ReferenceTransactionBegin(kSlotIndex0);
    unit_.ReferenceTransactionAbort(kSlotIndex0);
    unit_.ReferenceTransactionBegin(kSlotIndex1);
    unit_.ReferenceTransactionAbort(kSlotIndex1);

    // and then Unsubscribe is recorded
    unit_.UnsubscribeTransactionBegin();
    unit_.UnsubscribeTransactionCommit();

    // and when rollback is called
    const auto rollback_result =
        unit_.RollbackProxyElementLog(GetDereferenceSlotCallbackWrapper(), GetUnsubscribeCallbackWrapper());

    // Then the result will not contain an error
    EXPECT_TRUE(rollback_result.has_value());
}

TEST_F(TransactionLogProxyElementFixture, RollbackWillNotCallCallbackIfSubscribeAborted)
{
    // Given a valid TransactionLog

    // Expecting that neither callback will be called
    EXPECT_CALL(dereference_slot_callback_, Call(_)).Times(0);
    EXPECT_CALL(unsubscribe_callback_, Call(_)).Times(0);

    // When Subscribe is recorded but then aborted
    unit_.SubscribeTransactionBegin(kSubscriptionMaxSampleCount);
    unit_.SubscribeTransactionAbort();

    // and when rollback is called
    const auto rollback_result =
        unit_.RollbackProxyElementLog(GetDereferenceSlotCallbackWrapper(), GetUnsubscribeCallbackWrapper());

    // Then the result will not contain an error
    EXPECT_TRUE(rollback_result.has_value());
}

TEST_F(TransactionLogProxyElementFixture, RollbackWillCallBothCallbacksAfterReferencingCompleted)
{

    // Given a valid TransactionLog

    // Expecting that the decrement slot callback will be called once for 2 slots
    EXPECT_CALL(dereference_slot_callback_, Call(kSlotIndex0));
    EXPECT_CALL(dereference_slot_callback_, Call(kSlotIndex1));

    // and the unsubscribe callback will be called
    EXPECT_CALL(unsubscribe_callback_, Call(kSubscriptionMaxSampleCount));

    // When Subscribe is recorded
    unit_.SubscribeTransactionBegin(kSubscriptionMaxSampleCount);
    unit_.SubscribeTransactionCommit();

    // and both slots are referenced but never dereferenced
    unit_.ReferenceTransactionBegin(kSlotIndex0);
    unit_.ReferenceTransactionCommit(kSlotIndex0);

    unit_.ReferenceTransactionBegin(kSlotIndex1);
    unit_.ReferenceTransactionCommit(kSlotIndex1);

    // and when rollback is called
    const auto rollback_result =
        unit_.RollbackProxyElementLog(GetDereferenceSlotCallbackWrapper(), GetUnsubscribeCallbackWrapper());

    // Then the result will not contain an error
    EXPECT_TRUE(rollback_result.has_value());

    // and when rollback is called again, then the slots should have already been derefenced so it should do nothing
    const auto rollback_result_2 =
        unit_.RollbackProxyElementLog(GetDereferenceSlotCallbackWrapper(), GetUnsubscribeCallbackWrapper());
    EXPECT_TRUE(rollback_result_2.has_value());
}

TEST_F(TransactionLogProxyElementFixture, RollbackWillCallUnsubscribeCallbackAfterDereferencingButNotUnsubscribing)
{
    // Given a valid TransactionLog

    // Expecting that the decrement slot callback will never be called
    EXPECT_CALL(dereference_slot_callback_, Call(_)).Times(0);

    // But the unsubscribe callback will be called
    EXPECT_CALL(unsubscribe_callback_, Call(kSubscriptionMaxSampleCount));

    // When Subscribe is recorded
    unit_.SubscribeTransactionBegin(kSubscriptionMaxSampleCount);
    unit_.SubscribeTransactionCommit();

    // and 2 slots are succesfully referenced and dereferenced
    unit_.ReferenceTransactionBegin(kSlotIndex0);
    unit_.ReferenceTransactionCommit(kSlotIndex0);
    unit_.ReferenceTransactionBegin(kSlotIndex1);
    unit_.ReferenceTransactionCommit(kSlotIndex1);

    unit_.DereferenceTransactionBegin(kSlotIndex0);
    unit_.DereferenceTransactionCommit(kSlotIndex0);
    unit_.DereferenceTransactionBegin(kSlotIndex1);
    unit_.DereferenceTransactionCommit(kSlotIndex1);

    // and when rollback is called
    const auto rollback_result =
        unit_.RollbackProxyElementLog(GetDereferenceSlotCallbackWrapper(), GetUnsubscribeCallbackWrapper());

    // Then the result will not contain an error
    EXPECT_TRUE(rollback_result.has_value());
}

TEST_F(TransactionLogProxyElementFixture, RollbackWillCallUnsubscribeCallbackWithMostRecentSubscriptionMaxSampleCount)
{
    const std::size_t first_subscription_max_sample_count{5U};
    const std::size_t second_subscription_max_sample_count{10U};

    // Given a valid TransactionLog

    // Expecting that the decrement slot callback will never be called
    EXPECT_CALL(dereference_slot_callback_, Call(_)).Times(0);

    // But the unsubscribe callback will be called with the max sample count from the second subscription
    EXPECT_CALL(unsubscribe_callback_, Call(second_subscription_max_sample_count));

    // When Subscribe is recorded
    unit_.SubscribeTransactionBegin(first_subscription_max_sample_count);
    unit_.SubscribeTransactionCommit();

    // and then Unsubscribe is recorded
    unit_.UnsubscribeTransactionBegin();
    unit_.UnsubscribeTransactionCommit();

    // and a second Subscribe is recorded with a different max sample count
    unit_.SubscribeTransactionBegin(second_subscription_max_sample_count);
    unit_.SubscribeTransactionCommit();

    // and when rollback is called
    const auto rollback_result =
        unit_.RollbackProxyElementLog(GetDereferenceSlotCallbackWrapper(), GetUnsubscribeCallbackWrapper());

    // Then the result will not contain an error
    EXPECT_TRUE(rollback_result.has_value());
}

TEST_F(TransactionLogProxyElementFixture, RollbackWillReturnErrorIfReferenceTransactionDidNotComplete)
{
    // Given a valid TransactionLog

    // Expecting that neither callback will be called
    EXPECT_CALL(dereference_slot_callback_, Call(_)).Times(0);
    EXPECT_CALL(unsubscribe_callback_, Call(_)).Times(0);

    // When Subscribe is recorded
    unit_.SubscribeTransactionBegin(kSubscriptionMaxSampleCount);
    unit_.SubscribeTransactionCommit();

    // and both slots are referenced but the referencing never finished
    unit_.ReferenceTransactionBegin(kSlotIndex0);

    unit_.ReferenceTransactionBegin(kSlotIndex1);

    // and when rollback is called
    const auto rollback_result =
        unit_.RollbackProxyElementLog(GetDereferenceSlotCallbackWrapper(), GetUnsubscribeCallbackWrapper());

    // Then the result will contain an error
    EXPECT_FALSE(rollback_result.has_value());

    // and when rollback is called again, then an error should still be returned
    const auto rollback_result_2 =
        unit_.RollbackProxyElementLog(GetDereferenceSlotCallbackWrapper(), GetUnsubscribeCallbackWrapper());
    EXPECT_FALSE(rollback_result_2.has_value());
}

TEST_F(TransactionLogProxyElementFixture, RollbackWillReturnErrorIfDereferenceTransactionDidNotComplete)
{
    // Given a valid TransactionLog

    // Expecting that neither callback will be called
    EXPECT_CALL(dereference_slot_callback_, Call(_)).Times(0);
    EXPECT_CALL(unsubscribe_callback_, Call(_)).Times(0);

    // When Subscribe is recorded
    unit_.SubscribeTransactionBegin(kSubscriptionMaxSampleCount);
    unit_.SubscribeTransactionCommit();

    // and 2 slots are succesfully referenced
    unit_.ReferenceTransactionBegin(kSlotIndex0);
    unit_.ReferenceTransactionCommit(kSlotIndex0);
    unit_.ReferenceTransactionBegin(kSlotIndex1);
    unit_.ReferenceTransactionCommit(kSlotIndex1);

    // but dereferencing both slots never finished
    unit_.DereferenceTransactionBegin(kSlotIndex0);
    unit_.DereferenceTransactionBegin(kSlotIndex1);

    // and when rollback is called
    const auto rollback_result =
        unit_.RollbackProxyElementLog(GetDereferenceSlotCallbackWrapper(), GetUnsubscribeCallbackWrapper());

    // Then the result will contain an error
    EXPECT_FALSE(rollback_result.has_value());

    // and when rollback is called again, then an error should still be returned
    const auto rollback_result_2 =
        unit_.RollbackProxyElementLog(GetDereferenceSlotCallbackWrapper(), GetUnsubscribeCallbackWrapper());
    EXPECT_FALSE(rollback_result_2.has_value());
}

TEST_F(TransactionLogProxyElementFixture, RollbackWillReturnErrorIfSubscribeTransactionDidNotComplete)
{
    // Given a valid TransactionLog

    // Expecting that the decrement slot callback will be called once for 2 slots
    EXPECT_CALL(dereference_slot_callback_, Call(kSlotIndex0));
    EXPECT_CALL(dereference_slot_callback_, Call(kSlotIndex1));

    // Expecting that the unsubscribe callback will never be called
    EXPECT_CALL(unsubscribe_callback_, Call(_)).Times(0);

    // When subscribe is begun but never finished
    unit_.SubscribeTransactionBegin(kSubscriptionMaxSampleCount);

    // and 2 slots are succesfully referenced
    unit_.ReferenceTransactionBegin(kSlotIndex0);
    unit_.ReferenceTransactionCommit(kSlotIndex0);
    unit_.ReferenceTransactionBegin(kSlotIndex1);
    unit_.ReferenceTransactionCommit(kSlotIndex1);

    // and when rollback is called
    const auto rollback_result =
        unit_.RollbackProxyElementLog(GetDereferenceSlotCallbackWrapper(), GetUnsubscribeCallbackWrapper());

    // Then the result will contain an error
    EXPECT_FALSE(rollback_result.has_value());

    // and when rollback is called again, then an error should still be returned
    const auto rollback_result_2 =
        unit_.RollbackProxyElementLog(GetDereferenceSlotCallbackWrapper(), GetUnsubscribeCallbackWrapper());
    EXPECT_FALSE(rollback_result_2.has_value());
}

TEST_F(TransactionLogProxyElementFixture, RollbackWillReturnErrorIfUnsubscribeTransactionDidNotComplete)
{
    // Given a valid TransactionLog

    // Expecting that the decrement slot callback will be called once for 2 slots
    EXPECT_CALL(dereference_slot_callback_, Call(kSlotIndex0));
    EXPECT_CALL(dereference_slot_callback_, Call(kSlotIndex1));

    // Expecting that the unsubscribe callback will never be called
    EXPECT_CALL(unsubscribe_callback_, Call(_)).Times(0);

    // When subscribe is called
    unit_.SubscribeTransactionBegin(kSubscriptionMaxSampleCount);
    unit_.SubscribeTransactionCommit();

    // and 2 slots are succesfully referenced
    unit_.ReferenceTransactionBegin(kSlotIndex0);
    unit_.ReferenceTransactionCommit(kSlotIndex0);
    unit_.ReferenceTransactionBegin(kSlotIndex1);
    unit_.ReferenceTransactionCommit(kSlotIndex1);

    // but Unsubscribe is begun but never finished
    unit_.UnsubscribeTransactionBegin();

    // and when rollback is called
    const auto rollback_result =
        unit_.RollbackProxyElementLog(GetDereferenceSlotCallbackWrapper(), GetUnsubscribeCallbackWrapper());

    // Then the result will contain an error
    EXPECT_FALSE(rollback_result.has_value());

    // and when rollback is called again, then an error should still be returned
    const auto rollback_result_2 =
        unit_.RollbackProxyElementLog(GetDereferenceSlotCallbackWrapper(), GetUnsubscribeCallbackWrapper());
    EXPECT_FALSE(rollback_result_2.has_value());
}

using TransactionLogSkeletonTracingElementFixture = TransactionLogFixture;
TEST_F(TransactionLogSkeletonTracingElementFixture, RollbackWillNotCallCallbackWhenNoTransactionsRecorded)
{
    // Given a valid TransactionLog

    // Expecting that the callback will not be called
    EXPECT_CALL(dereference_slot_callback_, Call(_)).Times(0);

    // When no transactions are recorded

    // and when rollback is called
    const auto rollback_result = unit_.RollbackSkeletonTracingElementLog(GetDereferenceSlotCallbackWrapper());

    // Then the result will not contain an error
    EXPECT_TRUE(rollback_result.has_value());
}

TEST_F(TransactionLogSkeletonTracingElementFixture, RollbackWillNotCallCallbackAfterDereferencingCompleted)
{
    // Given a valid TransactionLog

    // Expecting that the callback will not be called
    EXPECT_CALL(dereference_slot_callback_, Call(_)).Times(0);

    // When 2 slots are succesfully referenced and dereferenced
    unit_.ReferenceTransactionBegin(kSlotIndex0);
    unit_.ReferenceTransactionCommit(kSlotIndex0);
    unit_.ReferenceTransactionBegin(kSlotIndex1);
    unit_.ReferenceTransactionCommit(kSlotIndex1);

    unit_.DereferenceTransactionBegin(kSlotIndex0);
    unit_.DereferenceTransactionCommit(kSlotIndex0);
    unit_.DereferenceTransactionBegin(kSlotIndex1);
    unit_.DereferenceTransactionCommit(kSlotIndex1);

    // and when rollback is called
    const auto rollback_result = unit_.RollbackSkeletonTracingElementLog(GetDereferenceSlotCallbackWrapper());

    // Then the result will not contain an error
    EXPECT_TRUE(rollback_result.has_value());
}

TEST_F(TransactionLogSkeletonTracingElementFixture, RollbackWillNotCallCallbackIfRerencingAborted)
{
    // Given a valid TransactionLog

    // Expecting that the callback will not be called
    EXPECT_CALL(dereference_slot_callback_, Call(_)).Times(0);

    // When 2 slots are referenced and aborted
    unit_.ReferenceTransactionBegin(kSlotIndex0);
    unit_.ReferenceTransactionAbort(kSlotIndex0);
    unit_.ReferenceTransactionBegin(kSlotIndex1);
    unit_.ReferenceTransactionAbort(kSlotIndex1);

    // and when rollback is called
    const auto rollback_result = unit_.RollbackSkeletonTracingElementLog(GetDereferenceSlotCallbackWrapper());

    // Then the result will not contain an error
    EXPECT_TRUE(rollback_result.has_value());
}

TEST_F(TransactionLogSkeletonTracingElementFixture, RollbackWillCallCallbackAfterReferencingCompleted)
{
    // Given a valid TransactionLog

    // Expecting that the decrement slot callback will be called once for 2 slots
    EXPECT_CALL(dereference_slot_callback_, Call(kSlotIndex0));
    EXPECT_CALL(dereference_slot_callback_, Call(kSlotIndex1));

    // When 2 slots are referenced but not dereferenced
    unit_.ReferenceTransactionBegin(kSlotIndex0);
    unit_.ReferenceTransactionCommit(kSlotIndex0);
    unit_.ReferenceTransactionBegin(kSlotIndex1);
    unit_.ReferenceTransactionCommit(kSlotIndex1);

    // and when rollback is called
    const auto rollback_result = unit_.RollbackSkeletonTracingElementLog(GetDereferenceSlotCallbackWrapper());

    // Then the result will not contain an error
    EXPECT_TRUE(rollback_result.has_value());
}

TEST_F(TransactionLogSkeletonTracingElementFixture, RollbackWillReturnErrorIfTransactionDidNotComplete)
{
    // Given a valid TransactionLog

    // Expecting that the callback will be called for the slot which completed its transaction
    EXPECT_CALL(dereference_slot_callback_, Call(kSlotIndex0));

    // When 2 slots are referenced but doesn't finish for one slot
    unit_.ReferenceTransactionBegin(kSlotIndex0);
    unit_.ReferenceTransactionCommit(kSlotIndex0);
    unit_.ReferenceTransactionBegin(kSlotIndex1);

    // and when rollback is called
    const auto rollback_result = unit_.RollbackSkeletonTracingElementLog(GetDereferenceSlotCallbackWrapper());

    // Then the result will contain an error
    EXPECT_FALSE(rollback_result.has_value());

    // and when rollback is called again, then an error should still be returned
    const auto rollback_result_2 = unit_.RollbackSkeletonTracingElementLog(GetDereferenceSlotCallbackWrapper());
    EXPECT_FALSE(rollback_result_2.has_value());
}

TEST_F(TransactionLogSkeletonTracingElementFixture, RollbackWillReturnErrorIfDereferenceTransactionDidNotComplete)
{
    // Given a valid TransactionLog

    // Expecting that the callback will never be called (as the first slot succesfully dereferences its slot and the
    // second slot fails during rollback)
    EXPECT_CALL(dereference_slot_callback_, Call(_)).Times(0);

    // When 2 slots are referenced
    unit_.ReferenceTransactionBegin(kSlotIndex0);
    unit_.ReferenceTransactionCommit(kSlotIndex0);
    unit_.ReferenceTransactionBegin(kSlotIndex1);
    unit_.ReferenceTransactionCommit(kSlotIndex1);

    // but dereferencing for one slot never finished
    unit_.DereferenceTransactionBegin(kSlotIndex0);
    unit_.DereferenceTransactionCommit(kSlotIndex0);
    unit_.DereferenceTransactionBegin(kSlotIndex1);

    // and when rollback is called
    const auto rollback_result = unit_.RollbackSkeletonTracingElementLog(GetDereferenceSlotCallbackWrapper());

    // Then the result will contain an error
    EXPECT_FALSE(rollback_result.has_value());

    // and when rollback is called again, then an error should still be returned
    const auto rollback_result_2 = unit_.RollbackSkeletonTracingElementLog(GetDereferenceSlotCallbackWrapper());
    EXPECT_FALSE(rollback_result_2.has_value());
}

using TransactionLogContainsTransactionsSubscriptionFixture = TransactionLogSkeletonTracingElementFixture;
TEST_F(TransactionLogContainsTransactionsSubscriptionFixture, ReturnsFalseWhenNoTransactionsRecorded)
{
    // Given a valid TransactionLog which has no recorded Transactions

    // When calling ContainsTransactions
    // Then the result should be false
    EXPECT_FALSE(unit_.ContainsTransactions());
}

TEST_F(TransactionLogContainsTransactionsSubscriptionFixture, ReturnsTrueWhenSubscribeTransactionBeginRecorded)
{
    // Given a valid TransactionLog which has recorded a SubscribeTransactionBegin
    unit_.SubscribeTransactionBegin(kSubscriptionMaxSampleCount);

    // When calling ContainsTransactions
    // Then the result should be true
    EXPECT_TRUE(unit_.ContainsTransactions());
}

TEST_F(TransactionLogContainsTransactionsSubscriptionFixture, ReturnsTrueWhenSubscribeTransactionCommitRecorded)
{
    // Given a valid TransactionLog which has recorded a full subscription transaction
    unit_.SubscribeTransactionBegin(kSubscriptionMaxSampleCount);
    unit_.SubscribeTransactionCommit();

    // When calling ContainsTransactions
    // Then the result should be true
    EXPECT_TRUE(unit_.ContainsTransactions());
}

TEST_F(TransactionLogContainsTransactionsSubscriptionFixture, ReturnsFalseWhenSubscribeTransactionAbortRecorded)
{
    // Given a valid TransactionLog which has recorded an aborted subscription transaction
    unit_.SubscribeTransactionBegin(kSubscriptionMaxSampleCount);
    unit_.SubscribeTransactionAbort();

    // When calling ContainsTransactions
    // Then the result should be false
    EXPECT_FALSE(unit_.ContainsTransactions());
}

TEST_F(TransactionLogContainsTransactionsSubscriptionFixture, ReturnsTrueWhenUnsubscribeTransactionBeginRecorded)
{
    // Given a valid TransactionLog which has recorded a full subscription transaction and a UnsubscribeTransactionBegin
    unit_.SubscribeTransactionBegin(kSubscriptionMaxSampleCount);
    unit_.SubscribeTransactionCommit();
    unit_.UnsubscribeTransactionBegin();

    // When calling ContainsTransactions
    // Then the result should be true
    EXPECT_TRUE(unit_.ContainsTransactions());
}

TEST_F(TransactionLogContainsTransactionsSubscriptionFixture, ReturnsFalseWhenUnsubscribeTransactionCommitRecorded)
{
    // Given a valid TransactionLog which has recorded a full subscription and unsubscription transaction
    unit_.SubscribeTransactionBegin(kSubscriptionMaxSampleCount);
    unit_.SubscribeTransactionCommit();
    unit_.UnsubscribeTransactionBegin();
    unit_.UnsubscribeTransactionCommit();

    // When calling ContainsTransactions
    // Then the result should be false
    EXPECT_FALSE(unit_.ContainsTransactions());
}

using TransactionLogContainsTransactionsReferenceFixture = TransactionLogSkeletonTracingElementFixture;
TEST_F(TransactionLogContainsTransactionsReferenceFixture, ReturnsTrueWhenReferenceTransactionBeginRecorded)
{
    // Given a valid TransactionLog which has recorded a ReferenceTransactionBegin
    unit_.ReferenceTransactionBegin(kSlotIndex0);

    // When calling ContainsTransactions
    // Then the result should be true
    EXPECT_TRUE(unit_.ContainsTransactions());
}

TEST_F(TransactionLogContainsTransactionsReferenceFixture, ReturnsTrueWhenReferenceTransactionCommitRecorded)
{
    // Given a valid TransactionLog which has recorded a full subscription transaction
    unit_.ReferenceTransactionBegin(kSlotIndex0);
    unit_.ReferenceTransactionCommit(kSlotIndex0);

    // When calling ContainsTransactions
    // Then the result should be true
    EXPECT_TRUE(unit_.ContainsTransactions());
}

TEST_F(TransactionLogContainsTransactionsReferenceFixture, ReturnsFalseWhenReferenceTransactionAbortRecorded)
{
    // Given a valid TransactionLog which has recorded an aborted subscription transaction
    unit_.ReferenceTransactionBegin(kSlotIndex0);
    unit_.ReferenceTransactionAbort(kSlotIndex0);

    // When calling ContainsTransactions
    // Then the result should be false
    EXPECT_FALSE(unit_.ContainsTransactions());
}

TEST_F(TransactionLogContainsTransactionsReferenceFixture, ReturnsTrueWhenDereferenceTransactionBeginRecorded)
{
    // Given a valid TransactionLog which has recorded a full subscription transaction and a DereferenceTransactionBegin
    unit_.ReferenceTransactionBegin(kSlotIndex0);
    unit_.ReferenceTransactionCommit(kSlotIndex0);
    unit_.DereferenceTransactionBegin(kSlotIndex0);

    // When calling ContainsTransactions
    // Then the result should be true
    EXPECT_TRUE(unit_.ContainsTransactions());
}

TEST_F(TransactionLogContainsTransactionsReferenceFixture, ReturnsFalseWhenDereferenceTransactionCommitRecorded)
{
    // Given a valid TransactionLog which has recorded a full subscription and unsubscription transaction
    unit_.ReferenceTransactionBegin(kSlotIndex0);
    unit_.ReferenceTransactionCommit(kSlotIndex0);
    unit_.DereferenceTransactionBegin(kSlotIndex0);
    unit_.DereferenceTransactionCommit(kSlotIndex0);

    // When calling ContainsTransactions
    // Then the result should be false
    EXPECT_FALSE(unit_.ContainsTransactions());
}

// Test for boundary condition: ReferenceTransactionBegin should retry and terminate
// when transaction-END bit remains TRUE after max retries (indicating stuck dereference thread).
class ReferenceTransactionBoundaryConditionFixture : public TransactionLogFixture
{
};

TEST_F(ReferenceTransactionBoundaryConditionFixture, ReferenceTransactionBeginTerminatesWhenTransactionEndRemainsTrue)
{
    // Given a TransactionLog with a slot where transaction-END bit is stuck TRUE
    // Set up the slot: BEGIN=false, END=true (boundary condition)
    auto& slot = TransactionLogAttorney{unit_}.GetReferenceCountSlot(kSlotIndex0);
    slot.SetTransactionBegin(false);
    slot.SetTransactionEnd(true);

    // When ReferenceTransactionBegin is called with transaction-END still TRUE
    // Then it should attempt retries and finally terminate (because END stays TRUE)
    EXPECT_DEATH(unit_.ReferenceTransactionBegin(kSlotIndex0), ".*");
}

TEST_F(ReferenceTransactionBoundaryConditionFixture, ReferenceTransactionBeginSuccessfulWhenTransactionEndIsFalse)
{
    // Given a TransactionLog with a slot
    // Set up the slot: BEGIN=false, END=false
    auto& slot = TransactionLogAttorney{unit_}.GetReferenceCountSlot(kSlotIndex0);
    slot.SetTransactionBegin(false);
    slot.SetTransactionEnd(false);

    // When ReferenceTransactionBegin is called
    unit_.ReferenceTransactionBegin(kSlotIndex0);
    // then TransactionBegin is set to true.
    EXPECT_TRUE(slot.GetTransactionBegin());
}

}  // namespace
}  // namespace score::mw::com::impl::lola
