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
#include "score/mw/com/impl/bindings/lola/transaction_log_set.h"

#include "score/mw/com/impl/bindings/lola/runtime_mock.h"
#include "score/mw/com/impl/bindings/lola/test/transaction_log_test_resources.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_local_view.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/test/runtime_mock_guard.h"

#include "score/memory/shared/shared_memory_resource_heap_allocator_mock.h"

#include <score/assert_support.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <vector>

namespace score::mw::com::impl::lola
{
namespace
{

using ::testing::_;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrictMock;

const std::size_t kNumberOfLogs{5U};
const TransactionLog::MaxSampleCountType kSubscriptionMaxSampleCount{5U};
const std::size_t kDummyNumberOfSlots{5U};
const TransactionLogId kDummyTransactionLogId{10U};

class TransactionLogSetFixture : public TransactionLogSetHelperFixture
{
  protected:
    TransactionLogSetFixture& WithATransactionLogSet(std::size_t number_of_logs)
    {
        unit_ = std::make_unique<TransactionLogSet>(number_of_logs, kDummyNumberOfSlots, memory_resource_);
        return *this;
    }

    TransactionLogLocalView::DereferenceSlotCallback GetDereferenceSlotCallbackWrapper() noexcept
    {
        // Since a MockFunction doesn't fit within an score::cpp::callback, we wrap it in a smaller lambda which only
        // stores a pointer to the MockFunction and therefore fits within the score::cpp::callback.
        return [this](const TransactionLog::SlotIndexType slot_index) noexcept {
            dereference_slot_callback_.AsStdFunction()(slot_index);
        };
    }

    TransactionLogLocalView::DereferenceSlotCallback GetUnsubscribeCallbackWrapper() noexcept
    {
        // Since a MockFunction doesn't fit within an score::cpp::callback, we wrap it in a smaller lambda which only
        // stores a pointer to the MockFunction and therefore fits within the score::cpp::callback.
        return [this](const TransactionLog::MaxSampleCountType subscription_max_sample_count) noexcept {
            unsubscribe_callback_.AsStdFunction()(subscription_max_sample_count);
        };
    }

    TransactionLogRegistrationGuard RegisterProxyElementWithSubscribeTransaction(
        const TransactionLogId& transaction_log_id) noexcept
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT(unit_ != nullptr);
        auto transaction_log_registration_guard =
            unit_->RegisterProxyElement(transaction_log_id, proxy_event_data_control_local_).value();
        const auto transaction_log_index = transaction_log_registration_guard.GetTransactionLogIndex();
        TransactionLogLocalView transaction_log_local_view = unit_->GetTransactionLog(transaction_log_index);
        transaction_log_local_view.SubscribeTransactionBegin(kSubscriptionMaxSampleCount);
        transaction_log_local_view.SubscribeTransactionCommit();

        auto& transaction_logs = TransactionLogSetAttorney{*unit_}.GetProxyTransactionLogs();
        auto& transaction_log_node = transaction_logs.at(transaction_log_index);

        EXPECT_TRUE(transaction_log_node.IsActive());
        EXPECT_FALSE(transaction_log_node.NeedsRollback());

        return transaction_log_registration_guard;
    }

    TransactionLogRegistrationGuard RegisterProxyElementWithSubscribeAndReferenceTransactions(
        const TransactionLogId& transaction_log_id,
        const TransactionLog::SlotIndexType slot_index) noexcept
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT(unit_ != nullptr);
        auto transaction_log_registration_guard =
            unit_->RegisterProxyElement(transaction_log_id, proxy_event_data_control_local_).value();
        const auto transaction_log_index = transaction_log_registration_guard.GetTransactionLogIndex();
        TransactionLogLocalView transaction_log_local_view = unit_->GetTransactionLog(transaction_log_index);
        transaction_log_local_view.SubscribeTransactionBegin(kSubscriptionMaxSampleCount);
        transaction_log_local_view.SubscribeTransactionCommit();
        transaction_log_local_view.ReferenceTransactionBegin(slot_index);
        transaction_log_local_view.ReferenceTransactionCommit(slot_index);

        auto& transaction_logs = TransactionLogSetAttorney{*unit_}.GetProxyTransactionLogs();
        auto& transaction_log_node = transaction_logs.at(transaction_log_index);

        EXPECT_TRUE(transaction_log_node.IsActive());
        EXPECT_FALSE(transaction_log_node.NeedsRollback());

        return transaction_log_registration_guard;
    }

    memory::shared::SharedMemoryResourceHeapAllocatorMock memory_resource_{1U};
    EventDataControl event_data_control_{kNumberOfLogs, memory_resource_};
    ProxyEventDataControlLocalView<> proxy_event_data_control_local_{event_data_control_};

    std::unique_ptr<TransactionLogSet> unit_{nullptr};

    StrictMock<MockFunction<void(TransactionLog::SlotIndexType)>> dereference_slot_callback_{};
    StrictMock<MockFunction<void(TransactionLog::MaxSampleCountType)>> unsubscribe_callback_{};
};

using TransactionLogSetRollbackFixture = TransactionLogSetFixture;
TEST_F(TransactionLogSetRollbackFixture, CallingRollbackOnUnregisteredIdIdReturnsValidResult)
{
    // Given a TransactionLogSet which is initialized to contain kNumberOfLogs logs
    WithATransactionLogSet(kNumberOfLogs);

    // When calling RollbackProxyTransactions with a transaction log id that was never registered
    const auto rollback_result = unit_->RollbackProxyTransactions(
        kDummyTransactionLogId, GetDereferenceSlotCallbackWrapper(), GetUnsubscribeCallbackWrapper());

    // Then a valid result is returned
    ASSERT_TRUE(rollback_result.has_value());
}
TEST_F(TransactionLogSetRollbackFixture, CallingRollbackWhenMaxLogsIsZeroReturnsValidResult)
{
    // Given a TransactionLogSet which is initialized to contain 0 logs
    const std::size_t number_of_logs{0U};
    WithATransactionLogSet(number_of_logs);

    // When calling RollbackProxyTransactions with a transaction log id that was never registered
    const auto rollback_result = unit_->RollbackProxyTransactions(
        kDummyTransactionLogId, GetDereferenceSlotCallbackWrapper(), GetUnsubscribeCallbackWrapper());

    // Then a valid result is returned
    EXPECT_TRUE(rollback_result.has_value());
}

TEST_F(TransactionLogSetRollbackFixture, CallingRollbackOnRegisteredIdWithoutMarkingNeedsRollbackIdDoesNothing)
{
    const TransactionLog::SlotIndexType slot_index{1U};

    WithATransactionLogSet(kNumberOfLogs);

    // Expecting that both callbacks will not be called
    EXPECT_CALL(unsubscribe_callback_, Call(kSubscriptionMaxSampleCount)).Times(0);
    EXPECT_CALL(dereference_slot_callback_, Call(slot_index)).Times(0);

    // When registering a TransactionLog
    const TransactionLogRegistrationGuard transaction_registration_guard =
        unit_->RegisterProxyElement(kDummyTransactionLogId, proxy_event_data_control_local_).value();
    const auto transaction_log_index = transaction_registration_guard.GetTransactionLogIndex();

    // and MarkTransactionLogsNeedRollback is not called

    // and RollbackProxyTransactions is called
    const auto rollback_result = unit_->RollbackProxyTransactions(
        kDummyTransactionLogId, GetDereferenceSlotCallbackWrapper(), GetUnsubscribeCallbackWrapper());
    ASSERT_TRUE(rollback_result.has_value());

    // Then the TransactionLog still remains
    const bool expect_needs_rollback{false};
    ExpectProxyTransactionLogExistsAtIndex(
        *unit_, kDummyTransactionLogId, transaction_log_index, expect_needs_rollback);
}

TEST_F(TransactionLogSetRollbackFixture, CallingRollbackOnRegisteredIdRollsBackLogAndResetsElement)
{
    // Since we manually call rollback and also destroy a TransactionLogRegistrationGuard in this test, we need to
    // disable the guard's destructor from doing any operations to avoid a crash. Details in
    // TransactionLogRegistrationGuard.
    TransactionLogRegistrationGuardDeactiveDestructionOperationGuard guard{};

    const TransactionLog::SlotIndexType slot_index{1U};

    WithATransactionLogSet(kNumberOfLogs);

    // Expecting that both callbacks will be called once
    EXPECT_CALL(unsubscribe_callback_, Call(kSubscriptionMaxSampleCount));
    EXPECT_CALL(dereference_slot_callback_, Call(slot_index));

    // When registering a TransactionLog with successful transactions
    const auto transaction_log_registration_guard =
        RegisterProxyElementWithSubscribeAndReferenceTransactions(kDummyTransactionLogId, slot_index);

    // When MarkTransactionLogsNeedRollback is called
    unit_->MarkTransactionLogsNeedRollback(kDummyTransactionLogId);

    // and RollbackProxyTransactions is called
    const auto rollback_result = unit_->RollbackProxyTransactions(
        kDummyTransactionLogId, GetDereferenceSlotCallbackWrapper(), GetUnsubscribeCallbackWrapper());

    // Then no error should be returned
    ASSERT_TRUE(rollback_result.has_value());

    // And the transaction log should be cleared
    ExpectTransactionLogSetEmpty(*unit_);
}

TEST_F(TransactionLogSetRollbackFixture, CallingRollbackOnRegisteredIdRollsBackFirstLogWithProvidedId)
{
    // Since we manually call rollback and also destroy a TransactionLogRegistrationGuard in this test, we need to
    // disable the guard's destructor from doing any operations to avoid a crash. Details in
    // TransactionLogRegistrationGuard.
    TransactionLogRegistrationGuardDeactiveDestructionOperationGuard guard{};

    const TransactionLog::SlotIndexType slot_index{1U};

    WithATransactionLogSet(kNumberOfLogs);

    // Expecting that the unsubscribe callback will be called for both instances
    EXPECT_CALL(unsubscribe_callback_, Call(kSubscriptionMaxSampleCount)).Times(2);

    // and expecting that the dereference callback will only be called for the instance with reference transactions
    // recorded
    EXPECT_CALL(dereference_slot_callback_, Call(slot_index)).Times(1);

    // When registering TransactionLogs with successful transactions
    const auto transaction_log_registration_guard =
        RegisterProxyElementWithSubscribeAndReferenceTransactions(kDummyTransactionLogId, slot_index);
    const auto transaction_log_registration_guard_2 =
        RegisterProxyElementWithSubscribeTransaction(kDummyTransactionLogId);

    // When MarkTransactionLogsNeedRollback is called
    unit_->MarkTransactionLogsNeedRollback(kDummyTransactionLogId);

    // and RollbackProxyTransactions is called
    const auto rollback_result = unit_->RollbackProxyTransactions(
        kDummyTransactionLogId, GetDereferenceSlotCallbackWrapper(), GetUnsubscribeCallbackWrapper());

    // Then no error should be returned
    ASSERT_TRUE(rollback_result.has_value());

    // And the only the second transaction log should remain
    const bool expect_needs_rollback{true};
    ExpectProxyTransactionLogExistsAtIndex(*unit_,
                                           kDummyTransactionLogId,
                                           transaction_log_registration_guard_2.GetTransactionLogIndex(),
                                           expect_needs_rollback);

    // and when RollbackProxyTransactions is called again
    const auto rollback_result_2 = unit_->RollbackProxyTransactions(
        kDummyTransactionLogId, GetDereferenceSlotCallbackWrapper(), GetUnsubscribeCallbackWrapper());

    // Then no error should be returned
    ASSERT_TRUE(rollback_result_2.has_value());

    // And the transaction log should be cleared
    ExpectTransactionLogSetEmpty(*unit_);
}

TEST_F(TransactionLogSetRollbackFixture, CallingRollbackOnRegisteredIdOnlyRollsBackLogsMarkedForRollback)
{
    // Since we manually call rollback and also destroy a TransactionLogRegistrationGuard in this test, we need to
    // disable the guard's destructor from doing any operations to avoid a crash. Details in
    // TransactionLogRegistrationGuard.
    TransactionLogRegistrationGuardDeactiveDestructionOperationGuard guard{};

    const TransactionLog::SlotIndexType slot_index{1U};

    WithATransactionLogSet(kNumberOfLogs);

    // Expecting that the unsubscribe callback will be called once
    EXPECT_CALL(unsubscribe_callback_, Call(kSubscriptionMaxSampleCount));

    // and expecting that the dereference callback will never be called
    EXPECT_CALL(dereference_slot_callback_, Call(slot_index)).Times(0);

    // When registering a TransactionLog with a successful subscribe transaction
    const auto transaction_log_registration_guard =
        RegisterProxyElementWithSubscribeTransaction(kDummyTransactionLogId);

    // When MarkTransactionLogsNeedRollback is called
    unit_->MarkTransactionLogsNeedRollback(kDummyTransactionLogId);

    // and then a second TransactionLog is registered with successful transactions
    const auto transaction_log_registration_guard_2 =
        RegisterProxyElementWithSubscribeAndReferenceTransactions(kDummyTransactionLogId, slot_index);

    // When RollbackProxyTransactions is called
    const auto rollback_result = unit_->RollbackProxyTransactions(
        kDummyTransactionLogId, GetDereferenceSlotCallbackWrapper(), GetUnsubscribeCallbackWrapper());

    // Then no error should be returned
    ASSERT_TRUE(rollback_result.has_value());

    // And the second transaction log should remain
    const bool expect_needs_rollback{false};
    ExpectProxyTransactionLogExistsAtIndex(*unit_,
                                           kDummyTransactionLogId,
                                           transaction_log_registration_guard_2.GetTransactionLogIndex(),
                                           expect_needs_rollback);
}

TEST_F(TransactionLogSetRollbackFixture,
       CallingRollbackOnRegisteredProxyTransactionLogIdPropagatesErrorFromTransactionLog)
{
    // Since we manually call rollback and also destroy a TransactionLogRegistrationGuard in this test, we need to
    // disable the guard's destructor from doing any operations to avoid a crash. Details in
    // TransactionLogRegistrationGuard.
    TransactionLogRegistrationGuardDeactiveDestructionOperationGuard guard{};

    const std::size_t slot_index{1U};

    WithATransactionLogSet(kNumberOfLogs);

    // Expecting that both callbacks are never called
    EXPECT_CALL(unsubscribe_callback_, Call(_)).Times(0);
    EXPECT_CALL(dereference_slot_callback_, Call(slot_index)).Times(0);

    // When registering a TransactionLog
    const TransactionLogRegistrationGuard transaction_registration_guard{
        unit_->RegisterProxyElement(kDummyTransactionLogId, proxy_event_data_control_local_).value()};
    const auto transaction_log_index = transaction_registration_guard.GetTransactionLogIndex();

    // and a subscribe transaction is begun but never finished, indicating a crash
    TransactionLogLocalView transaction_log_local_view = unit_->GetTransactionLog(transaction_log_index);
    transaction_log_local_view.SubscribeTransactionBegin(kSubscriptionMaxSampleCount);

    // When MarkTransactionLogsNeedRollback is called
    unit_->MarkTransactionLogsNeedRollback(kDummyTransactionLogId);

    // and RollbackProxyTransactions is called
    const auto rollback_result = unit_->RollbackProxyTransactions(
        kDummyTransactionLogId, GetDereferenceSlotCallbackWrapper(), GetUnsubscribeCallbackWrapper());

    // Then an error should be returned
    ASSERT_FALSE(rollback_result.has_value());
    EXPECT_EQ(rollback_result.error(), ComErrc::kCouldNotRestartProxy);

    // And the transaction log should not be cleared
    const bool expect_needs_rollback{true};
    ExpectProxyTransactionLogExistsAtIndex(
        *unit_, kDummyTransactionLogId, transaction_log_index, expect_needs_rollback);
}

TEST_F(TransactionLogSetRollbackFixture, CallingRollbackOnUnregisteredSkeletonTransactionLogIdDoesNothing)
{
    WithATransactionLogSet(kNumberOfLogs);

    const auto rollback_result = unit_->RollbackSkeletonTracingTransactions(GetDereferenceSlotCallbackWrapper());
    ASSERT_TRUE(rollback_result.has_value());
}

TEST_F(TransactionLogSetRollbackFixture,
       CallingRollbackOnRegisteredSkeletonTransactionLogIdRollsBackLogAndResetsElement)
{
    // Since we manually call rollback and also destroy a TransactionLogRegistrationGuard in this test, we need to
    // disable the guard's destructor from doing any operations to avoid a crash. Details in
    // TransactionLogRegistrationGuard.
    TransactionLogRegistrationGuardDeactiveDestructionOperationGuard guard{};

    const std::size_t slot_index{kDummyNumberOfSlots - 1U};

    WithATransactionLogSet(kNumberOfLogs);

    // Expecting that the dereference callback will be called once
    EXPECT_CALL(dereference_slot_callback_, Call(slot_index));

    // When registering a TransactionLog
    const auto transaction_registration_guard = unit_->RegisterSkeletonTracingElement(proxy_event_data_control_local_);
    const auto transaction_log_index = transaction_registration_guard.GetTransactionLogIndex();

    TransactionLogLocalView transaction_log_local_view = unit_->GetTransactionLog(transaction_log_index);

    // and a successful reference transaction is recorded
    transaction_log_local_view.ReferenceTransactionBegin(slot_index);
    transaction_log_local_view.ReferenceTransactionCommit(slot_index);

    // When RollbackSkeletonTracingTransactions is called
    const auto rollback_result = unit_->RollbackSkeletonTracingTransactions(GetDereferenceSlotCallbackWrapper());

    // Then no error should be returned
    ASSERT_TRUE(rollback_result.has_value());

    // And the transaction log should be cleared
    EXPECT_FALSE(TransactionLogSetAttorney{*unit_}.GetSkeletonTransactionLog().has_value());
}

TEST_F(TransactionLogSetRollbackFixture,
       CallingRollbackOnRegisteredSkeletonTransactionLogIdPropagatesErrorFromTransactionLog)
{
    // Since we manually call rollback and also destroy a TransactionLogRegistrationGuard in this test, we need to
    // disable the guard's destructor from doing any operations to avoid a crash. Details in
    // TransactionLogRegistrationGuard.
    TransactionLogRegistrationGuardDeactiveDestructionOperationGuard guard{};

    const std::size_t slot_index{1U};

    WithATransactionLogSet(kNumberOfLogs);

    // Expecting that the dereference callback is never called
    EXPECT_CALL(dereference_slot_callback_, Call(slot_index)).Times(0);

    // When registering a TransactionLog
    const auto transaction_registration_guard = unit_->RegisterSkeletonTracingElement(proxy_event_data_control_local_);
    const auto transaction_log_index = transaction_registration_guard.GetTransactionLogIndex();

    // and a reference transaction is begun but never finished, indicating a crash
    TransactionLogLocalView transaction_log_local_view = unit_->GetTransactionLog(transaction_log_index);
    transaction_log_local_view.ReferenceTransactionBegin(slot_index);

    // When RollbackProxyTransactions is called
    const auto rollback_result = unit_->RollbackSkeletonTracingTransactions(GetDereferenceSlotCallbackWrapper());

    // Then an error should be returned
    ASSERT_FALSE(rollback_result.has_value());
    EXPECT_EQ(rollback_result.error(), ComErrc::kCouldNotRestartProxy);

    // And the transaction log should not be cleared
    EXPECT_TRUE(TransactionLogSetAttorney{*unit_}.GetSkeletonTransactionLog().has_value());
}

using TransactionLogSetRegisterFixture = TransactionLogSetFixture;

TEST_F(TransactionLogSetRegisterFixture, RegisteringWhenMaxLogsIsZeroReturnsError)
{
    // Given a TransactionLogSet which is initialized to contain 0 logs
    const std::size_t number_of_logs{0U};
    WithATransactionLogSet(number_of_logs);

    // When calling RegisterProxyElement
    const auto transaction_log_registration_result =
        unit_->RegisterProxyElement(kDummyTransactionLogId, proxy_event_data_control_local_);

    // Then an error is returned
    EXPECT_FALSE(transaction_log_registration_result.has_value());
}

TEST_F(TransactionLogSetRegisterFixture, RegisteringLessThanTheMaxNumberPassedToConstructorReturnsValidIndexes)
{
    WithATransactionLogSet(kNumberOfLogs);

    std::vector<TransactionLogRegistrationGuard> transaction_log_registration_guards{};
    for (std::size_t i = 0; i < kNumberOfLogs; ++i)
    {
        const TransactionLogId transaction_log_id{static_cast<uid_t>(i)};
        transaction_log_registration_guards.push_back(
            unit_->RegisterProxyElement(transaction_log_id, proxy_event_data_control_local_).value());
    }
}

TEST_F(TransactionLogSetRegisterFixture, RegisteringMoreThanTheMaxNumberPassedToConstructorReturnsError)
{
    WithATransactionLogSet(kNumberOfLogs);

    std::vector<TransactionLogRegistrationGuard> transaction_log_registration_guards{};
    for (std::size_t i = 0; i < kNumberOfLogs; ++i)
    {
        const TransactionLogId transaction_log_id{static_cast<uid_t>(i)};
        transaction_log_registration_guards.push_back(
            unit_->RegisterProxyElement(transaction_log_id, proxy_event_data_control_local_).value());
    }

    const TransactionLogId transaction_log_id{static_cast<uid_t>(kNumberOfLogs)};
    const auto transaction_log_registration_result =
        unit_->RegisterProxyElement(transaction_log_id, proxy_event_data_control_local_);
    EXPECT_FALSE(transaction_log_registration_result.has_value());
}

TEST_F(TransactionLogSetRegisterFixture,
       CallingRegisterProxyWillCreateATransactionLogAndReturnValidTransactionLogRegistrationGuard)
{
    const bool expect_needs_rollback{false};

    WithATransactionLogSet(kNumberOfLogs);

    // When registering the proxy with the TransactionLogSet
    const TransactionLogRegistrationGuard transaction_registration_guard =
        unit_->RegisterProxyElement(kDummyTransactionLogId, proxy_event_data_control_local_).value();

    // Then we get a valid TransactionLogRegistrationGuard which contains an index corresponding to a valid
    // TransactionLog
    const auto transaction_log_index = transaction_registration_guard.GetTransactionLogIndex();
    ExpectProxyTransactionLogExistsAtIndex(
        *unit_, kDummyTransactionLogId, transaction_log_index, expect_needs_rollback);
}

TEST_F(TransactionLogSetRegisterFixture, RegisterWithSameIdCanBeReCalledAfterUnregistering)
{
    const bool expect_needs_rollback{false};

    WithATransactionLogSet(kNumberOfLogs);

    std::optional<TransactionLogRegistrationGuard> transaction_registration_guard{
        unit_->RegisterProxyElement(kDummyTransactionLogId, proxy_event_data_control_local_).value()};
    transaction_registration_guard.reset();
    std::optional<TransactionLogRegistrationGuard> transaction_registration_guard_2{
        unit_->RegisterProxyElement(kDummyTransactionLogId, proxy_event_data_control_local_).value()};
    const auto transaction_log_index_2 = transaction_registration_guard_2->GetTransactionLogIndex();
    ExpectProxyTransactionLogExistsAtIndex(
        *unit_, kDummyTransactionLogId, transaction_log_index_2, expect_needs_rollback);
}

TEST_F(TransactionLogSetRegisterFixture, CallingRegisterWithSameIdWillReturnDifferentIndices)
{
    const bool expect_needs_rollback{false};

    WithATransactionLogSet(kNumberOfLogs);

    const TransactionLogRegistrationGuard transaction_registration_guard{
        unit_->RegisterProxyElement(kDummyTransactionLogId, proxy_event_data_control_local_).value()};
    const auto transaction_log_index = transaction_registration_guard.GetTransactionLogIndex();
    ExpectProxyTransactionLogExistsAtIndex(
        *unit_, kDummyTransactionLogId, transaction_log_index, expect_needs_rollback);

    const TransactionLogRegistrationGuard transaction_registration_guard_2{
        unit_->RegisterProxyElement(kDummyTransactionLogId, proxy_event_data_control_local_).value()};
    const auto transaction_log_index_2 = transaction_registration_guard_2.GetTransactionLogIndex();
    EXPECT_NE(transaction_log_index, transaction_log_index_2);

    const bool expect_other_slots_empty{false};
    ExpectProxyTransactionLogExistsAtIndex(
        *unit_, kDummyTransactionLogId, transaction_log_index_2, expect_needs_rollback, expect_other_slots_empty);
}

TEST_F(TransactionLogSetRegisterFixture, CallingRegisterSkeletonWillCreateATransactionLogAndReturnTheIndex)
{
    WithATransactionLogSet(kNumberOfLogs);

    const auto transaction_registration_guard = unit_->RegisterSkeletonTracingElement(proxy_event_data_control_local_);
    const auto transaction_log_index = transaction_registration_guard.GetTransactionLogIndex();
    EXPECT_EQ(transaction_log_index, TransactionLogSet::kSkeletonIndexSentinel);
    EXPECT_TRUE(TransactionLogSetAttorney{*unit_}.GetSkeletonTransactionLog().has_value());
}

TEST_F(TransactionLogSetRegisterFixture, CallingUnRegisterWillRemoveProxyTransactionLog)
{
    WithATransactionLogSet(kNumberOfLogs);

    std::optional<TransactionLogRegistrationGuard> transaction_registration_guard{
        unit_->RegisterProxyElement(kDummyTransactionLogId, proxy_event_data_control_local_).value()};
    transaction_registration_guard.reset();
    ExpectTransactionLogSetEmpty(*unit_);
}

TEST_F(TransactionLogSetRegisterFixture, CallingUnRegisterAfterRegisteringTwiceWithSameIdOnlyUnregistersOneAtATime)
{
    const bool expect_needs_rollback{false};

    WithATransactionLogSet(kNumberOfLogs);

    std::optional<TransactionLogRegistrationGuard> transaction_registration_guard{
        unit_->RegisterProxyElement(kDummyTransactionLogId, proxy_event_data_control_local_).value()};
    std::optional<TransactionLogRegistrationGuard> transaction_registration_guard_2{
        unit_->RegisterProxyElement(kDummyTransactionLogId, proxy_event_data_control_local_).value()};
    const auto transaction_log_index_2 = transaction_registration_guard_2->GetTransactionLogIndex();

    transaction_registration_guard.reset();
    ExpectProxyTransactionLogExistsAtIndex(
        *unit_, kDummyTransactionLogId, transaction_log_index_2, expect_needs_rollback);

    transaction_registration_guard_2.reset();
    ExpectTransactionLogSetEmpty(*unit_);
}

TEST_F(TransactionLogSetRegisterFixture, CallingUnRegisterWillRemoveSkeletonTransactionLog)
{
    WithATransactionLogSet(kNumberOfLogs);

    std::optional<TransactionLogRegistrationGuard> transaction_registration_guard{
        unit_->RegisterSkeletonTracingElement(proxy_event_data_control_local_)};
    transaction_registration_guard.reset();
    EXPECT_FALSE(TransactionLogSetAttorney{*unit_}.GetSkeletonTransactionLog().has_value());
}

/// TransactionLogNode::Reset() currently asserts (SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE), when called while
/// containing transactions. So in this is a DEATH_TEST ensuring that the assert is hit and program terminates.
/// Currently test is needed for full code coverage.
using TransactionLogSetRegisterFixtureDeathTest = TransactionLogSetRegisterFixture;
TEST_F(TransactionLogSetRegisterFixtureDeathTest, CallingUnRegisterWhileTransactionLogContainsTransactionsAsserts)
{
    auto test_function = [this] {
        WithATransactionLogSet(kNumberOfLogs);

        // When registering a TransactionLog
        TransactionLogRegistrationGuard transaction_registration_guard{
            unit_->RegisterSkeletonTracingElement(proxy_event_data_control_local_)};
        const auto transaction_log_index = transaction_registration_guard.GetTransactionLogIndex();

        // and a reference transaction is begun but never finished, indicating a crash
        TransactionLogLocalView transaction_log_local_view = unit_->GetTransactionLog(transaction_log_index);
        transaction_log_local_view.ReferenceTransactionBegin(2U);
    };

    // We must use a death test here since the operation which should terminate is stored as a unique_ptr in
    // TransactionLogRegistrationGuard. The destructor of a unique_ptr is noexcept and therefore we cannot catch the
    // assertion in an SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED.
    EXPECT_DEATH(test_function(), ".*");
}

TEST_F(TransactionLogSetRegisterFixture, RegisterUnregisterMultipleTransactionLogsConcurrently)
{
    WithATransactionLogSet(kNumberOfLogs);

    auto& unit = unit_;
    std::vector<score::cpp::jthread> threads{};
    constexpr auto thread_count = 1U;
    constexpr auto loop_count = 1U;
    // constexpr auto number_of_proxy_elements = thread_count * loop_count;

    // In production, we have one ProxyEventDataControlLocalView per ProxyEvent. Therefore, we must create a vector of
    // ProxyEventDataControlLocalViews to ensure that each call to RegisterProxyElement uses a different
    // ProxyEventDataControlLocalView. We use a 2D vector here to make the indexing simpler (rather than creating a 1D
    // vector of size thread_count * loop_count and then calculating the index manually). We use a NonRelocatableVector
    // since ProxyEventDataControlLocalView is not copyable or movable.
    containers::NonRelocatableVector<containers::NonRelocatableVector<ProxyEventDataControlLocalView<>>>
        proxy_event_data_control_locals(thread_count);
    for (std::size_t i = 0; i < thread_count; ++i)
    {
        proxy_event_data_control_locals.emplace_back(loop_count);
        for (std::size_t j = 0; j < loop_count; ++j)
        {
            proxy_event_data_control_locals.at(j).emplace_back(event_data_control_);
        }
    }

    // Given thread_count threads, which concurrently Register a proxy element and - after some sleep time - unregister
    // again.
    for (std::size_t thread_idx = 0; thread_idx < thread_count; ++thread_idx)
    {
        threads.emplace_back([this,
                              &unit,
                              thread_number = TransactionLogIndex(thread_idx + 1U),
                              &proxy_event_data_control_locals,
                              thread_idx]() noexcept {
            for (auto loop_idx = 0U; loop_idx < loop_count; loop_idx++)
            {
                auto& proxy_event_data_control_local = proxy_event_data_control_locals.at(thread_idx).at(loop_idx);
                auto register_result = unit->RegisterProxyElement(thread_number, proxy_event_data_control_local);
                ASSERT_TRUE(register_result.has_value());
                auto transaction_registration_guard = std::move(register_result).value();
                using namespace std::chrono_literals;
                std::this_thread::sleep_for(5ms);
            }
        });
    }

    // and when all threads have concluded their work
    for (std::size_t i = 0; i < thread_count; ++i)
    {
        auto& thread = threads.at(i);
        if (thread.joinable())
        {
            thread.join();
        }
    }

    // then all acquired TransactionLogNodes have been correctly unregistered by the threads and thus the
    // TransactionLogSet is "empty" again.
    ExpectTransactionLogSetEmpty(*unit_);
}

}  // namespace
}  // namespace score::mw::com::impl::lola
