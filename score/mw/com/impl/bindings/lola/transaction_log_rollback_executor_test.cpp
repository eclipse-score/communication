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
#include "score/mw/com/impl/bindings/lola/transaction_log_rollback_executor.h"

#include "score/mw/com/impl/bindings/lola/messaging/message_passing_service_mock.h"
#include "score/mw/com/impl/bindings/lola/rollback_synchronization.h"
#include "score/mw/com/impl/bindings/lola/runtime.h"
#include "score/mw/com/impl/bindings/lola/runtime_mock.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event_properties.h"
#include "score/mw/com/impl/bindings/lola/test/transaction_log_test_resources.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/impl/runtime_mock.h"

#include "score/memory/shared/shared_memory_resource_heap_allocator_mock.h"

#include <score/jthread.hpp>
#include <gtest/gtest.h>

namespace score::mw::com::impl::lola
{
namespace
{

using ::testing::_;
using ::testing::Return;
using ::testing::ReturnRef;

constexpr std::uint64_t kMemoryResourceId{10U};
const TransactionLogId kDummyTransactionLogId{20U};
const pid_t kDummyProviderPid{33U};
const pid_t kDummyOldConsumerPid{17U};
const pid_t kDummyCurrentConsumerPid{155U};
const QualityType kDummyQualityType{QualityType::kASIL_QM};
const ElementFqId kDummyElementFqId{1U, 2U, 3U, ElementType::EVENT};

constexpr std::size_t kNumberOfSlots{20U};
constexpr std::size_t kMaxSubscribers{20U};
const SkeletonEventProperties kDummySkeletonEventProperties{kNumberOfSlots, kMaxSubscribers, true};

class RuntimeMockGuard
{
  public:
    RuntimeMockGuard()
    {
        impl::Runtime::InjectMock(&mock_);
    }
    ~RuntimeMockGuard()
    {
        impl::Runtime::InjectMock(nullptr);
    }

    impl::RuntimeMock mock_;
};

class TransactionLogRollbackExecutorFixture : public ::testing::Test
{
  protected:
    TransactionLogRollbackExecutorFixture() noexcept
    {
        ON_CALL(runtime_mock_guard_.mock_, GetBindingRuntime(BindingType::kLoLa))
            .WillByDefault(Return(&lola_runtime_mock_));
        ON_CALL(lola_runtime_mock_, GetPid()).WillByDefault(Return(kDummyCurrentConsumerPid));
        ON_CALL(lola_runtime_mock_, GetLolaMessaging).WillByDefault(ReturnRef(message_passing_service_mock_));

        AddEvent(kDummyElementFqId, kDummySkeletonEventProperties);
    }

    void AddEvent(const ElementFqId element_fq_id, const SkeletonEventProperties skeleton_event_properties) noexcept
    {
        const auto emplace_result = service_data_control_.event_controls_.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(element_fq_id),
            std::forward_as_tuple(skeleton_event_properties.number_of_slots,
                                  skeleton_event_properties.max_subscribers,
                                  skeleton_event_properties.enforce_max_samples,
                                  memory_resource_mock_.getMemoryResourceProxy()));
        ASSERT_TRUE(emplace_result.second);
    }

    void AddUidPidMapping(uid_t uid, pid_t pid) noexcept
    {
        const auto result_pid = service_data_control_.uid_pid_mapping_.RegisterPid(uid, pid);
        ASSERT_TRUE(result_pid.has_value());
        ASSERT_EQ(result_pid.value(), pid);
    }

    EventDataControl& GetEventDataControl(const ElementFqId element_fq_id) noexcept
    {
        auto find_result = service_data_control_.event_controls_.find(element_fq_id);
        EXPECT_NE(find_result, service_data_control_.event_controls_.cend());
        return find_result->second.data_control;
    }

    void InsertServiceDataControl(RollbackSynchronization& rollback_synchronization) noexcept
    {
        auto rollback_mutex = rollback_synchronization.GetMutex(&service_data_control_);
        // we expect that the mutex did not yet exist, but has been created by our call.
        ASSERT_FALSE(rollback_mutex.second);
    }

    TransactionLogSet::TransactionLogNode& RegisterProxyElementWithTransactionLogSet(
        const ElementFqId& element_fq_id,
        const TransactionLogId& transaction_log_id) noexcept
    {
        auto& event_data_control = GetEventDataControl(element_fq_id);
        auto& transaction_log_set = event_data_control.GetTransactionLogSet();
        const auto transaction_log_index = transaction_log_set.RegisterProxyElement(transaction_log_id).value();

        auto& transaction_logs = TransactionLogSetAttorney{transaction_log_set}.GetProxyTransactionLogs();
        auto& transaction_log_node = transaction_logs.at(transaction_log_index);

        EXPECT_TRUE(transaction_log_node.IsActive());
        EXPECT_FALSE(transaction_log_node.NeedsRollback());

        return transaction_log_node;
    }

    RuntimeMockGuard runtime_mock_guard_{};
    lola::RuntimeMock lola_runtime_mock_{};
    memory::shared::SharedMemoryResourceHeapAllocatorMock memory_resource_mock_{kMemoryResourceId};
    ServiceDataControl service_data_control_{memory_resource_mock_.getMemoryResourceProxy()};
    MessagePassingServiceMock message_passing_service_mock_{};
};

using TransactionLogRegisterProxyElementFixture = TransactionLogRollbackExecutorFixture;
TEST_F(TransactionLogRegisterProxyElementFixture, RegisteringProxyElementLeadsToActiveLogNode)
{
    auto& event_data_control = GetEventDataControl(kDummyElementFqId);
    auto& transaction_log_set = event_data_control.GetTransactionLogSet();
    const auto transaction_log_index = transaction_log_set.RegisterProxyElement(kDummyTransactionLogId).value();

    auto& transaction_logs = TransactionLogSetAttorney{transaction_log_set}.GetProxyTransactionLogs();
    auto& transaction_log_node = transaction_logs.at(transaction_log_index);

    EXPECT_TRUE(transaction_log_node.IsActive());
    EXPECT_FALSE(transaction_log_node.NeedsRollback());
}

using TransactionLogRollbackExecutorRollbackLogsFixture = TransactionLogRollbackExecutorFixture;
TEST_F(TransactionLogRollbackExecutorRollbackLogsFixture, WillDoNothingWhenServiceDataControlAlreadyInMap)
{
    RollbackSynchronization rollback_synchronization{};
    // Given our current service instance (which the TransactionLogRollbackExecutor unit under test deals with) is
    // already registered within the global RollbackSynchronization
    InsertServiceDataControl(rollback_synchronization);
    ON_CALL(lola_runtime_mock_, GetRollbackSynchronization()).WillByDefault(ReturnRef(rollback_synchronization));

    // and given our unit based on a transaction log set with one active log node
    TransactionLogRollbackExecutor unit{
        service_data_control_, kDummyQualityType, kDummyProviderPid, kDummyTransactionLogId};
    auto& transaction_log_node = RegisterProxyElementWithTransactionLogSet(kDummyElementFqId, kDummyTransactionLogId);

    // when calling RollbackTransactionLogs()
    auto result = unit.RollbackTransactionLogs();
    // expect, that it succeeded
    EXPECT_TRUE(result.has_value());
    // and expect, that the log node is still there and active (nothing has changed)
    EXPECT_TRUE(transaction_log_node.IsActive());
    EXPECT_FALSE(transaction_log_node.NeedsRollback());
}

TEST_F(TransactionLogRollbackExecutorRollbackLogsFixture,
       WillCreateRollbackSynchronizationEntryWhenRollbackSynchronizationEntryDoesNotAlreadyExist)
{
    // given an empty RollbackSynchronization
    RollbackSynchronization rollback_synchronization{};
    // which gets returned by the lola runtime
    ON_CALL(lola_runtime_mock_, GetRollbackSynchronization()).WillByDefault(ReturnRef(rollback_synchronization));

    TransactionLogRollbackExecutor unit{
        service_data_control_, kDummyQualityType, kDummyProviderPid, kDummyTransactionLogId};
    // when RollbackTransactionLogs() has been called
    unit.RollbackTransactionLogs();
    // expect, that the synchronization mutex for service_data_control_ exists afterward
    auto [mutex, mutex_existed] = rollback_synchronization.GetMutex(&service_data_control_);
    EXPECT_EQ(mutex_existed, true);
}

TEST_F(TransactionLogRollbackExecutorRollbackLogsFixture, WillNotifyProviderOutdatedPidWhenPidAlreadyRegistered)
{
    // given, we already have an existing uid/pid mapping registered for our uid (= kDummyTransactionLogId)
    AddUidPidMapping(kDummyTransactionLogId, kDummyOldConsumerPid);
    RollbackSynchronization rollback_synchronization{};
    ON_CALL(lola_runtime_mock_, GetRollbackSynchronization()).WillByDefault(ReturnRef(rollback_synchronization));

    // expect, that the provider will get notified about the old/previous pid being outdated
    EXPECT_CALL(message_passing_service_mock_,
                NotifyOutdatedNodeId(kDummyQualityType, kDummyOldConsumerPid, kDummyProviderPid));
    TransactionLogRollbackExecutor unit{
        service_data_control_, kDummyQualityType, kDummyProviderPid, kDummyTransactionLogId};
    // when RollbackTransactionLogs() is called.
    auto result = unit.RollbackTransactionLogs();
    EXPECT_TRUE(result.has_value());
}

TEST_F(TransactionLogRollbackExecutorRollbackLogsFixture, WillNotNotifyProviderOutdatedPidWhenPidNotAlreadyRegistered)
{
    // given, we don't have yet an existing uid/pid mapping registered for our uid (= kDummyTransactionLogId)
    RollbackSynchronization rollback_synchronization{};
    ON_CALL(lola_runtime_mock_, GetRollbackSynchronization()).WillByDefault(ReturnRef(rollback_synchronization));

    // expect, that NO provider notification happens about an old/previous pid being outdated
    EXPECT_CALL(message_passing_service_mock_, NotifyOutdatedNodeId(_, _, _)).Times(0);
    TransactionLogRollbackExecutor unit{
        service_data_control_, kDummyQualityType, kDummyProviderPid, kDummyTransactionLogId};
    // when RollbackTransactionLogs() is called.
    auto result = unit.RollbackTransactionLogs();
    EXPECT_TRUE(result.has_value());
}

TEST_F(TransactionLogRollbackExecutorRollbackLogsFixture, RollbackTransactionLogsCanBeCalledMultiThreaded)
{
    RollbackSynchronization rollback_synchronization{};
    ON_CALL(lola_runtime_mock_, GetRollbackSynchronization()).WillByDefault(ReturnRef(rollback_synchronization));

    TransactionLogRollbackExecutor unit{
        service_data_control_, kDummyQualityType, kDummyProviderPid, kDummyTransactionLogId};

    // We do the registration in a pre-step so that the threads are created as close as possible to each other to
    // increase the chance of thread contention.
    std::vector<std::reference_wrapper<const TransactionLogSet::TransactionLogNode>> transaction_log_nodes{};
    for (std::size_t i = 0; i < kMaxSubscribers; ++i)
    {
        const auto& transaction_log_node =
            RegisterProxyElementWithTransactionLogSet(kDummyElementFqId, kDummyTransactionLogId);
        transaction_log_nodes.emplace_back(transaction_log_node);
    }

    std::vector<score::cpp::jthread> threads{};
    for (std::size_t i = 0; i < kMaxSubscribers; ++i)
    {
        threads.emplace_back([&unit]() noexcept {
            unit.RollbackTransactionLogs();
        });
    }

    for (std::size_t i = 0; i < kMaxSubscribers; ++i)
    {
        auto& thread = threads.at(i);
        if (thread.joinable())
        {
            thread.join();
        }

        auto transaction_log_node = transaction_log_nodes.at(i);
        EXPECT_FALSE(transaction_log_node.get().IsActive());
        EXPECT_FALSE(transaction_log_node.get().NeedsRollback());
    }
}

TEST_F(TransactionLogRollbackExecutorRollbackLogsFixture, CallingRollbackTransactionLogsWillRollBackOneLogPerCall)
{
    RollbackSynchronization rollback_synchronization{};
    ON_CALL(lola_runtime_mock_, GetRollbackSynchronization()).WillByDefault(ReturnRef(rollback_synchronization));

    TransactionLogRollbackExecutor unit{
        service_data_control_, kDummyQualityType, kDummyProviderPid, kDummyTransactionLogId};

    auto& transaction_log_node_0 = RegisterProxyElementWithTransactionLogSet(kDummyElementFqId, kDummyTransactionLogId);
    auto& transaction_log_node_1 = RegisterProxyElementWithTransactionLogSet(kDummyElementFqId, kDummyTransactionLogId);

    ASSERT_TRUE(unit.RollbackTransactionLogs().has_value());

    EXPECT_FALSE(transaction_log_node_0.IsActive());
    EXPECT_FALSE(transaction_log_node_0.NeedsRollback());
    EXPECT_TRUE(transaction_log_node_1.IsActive());
    EXPECT_TRUE(transaction_log_node_1.NeedsRollback());

    ASSERT_TRUE(unit.RollbackTransactionLogs().has_value());

    EXPECT_FALSE(transaction_log_node_1.IsActive());
    EXPECT_FALSE(transaction_log_node_1.NeedsRollback());

    ASSERT_TRUE(unit.RollbackTransactionLogs().has_value());
}

/// \brief test case checks, that rollback of transaction logs for a given service instance is only done
///        once per lifecycle.
/// \details The 1st call to RollbackTransactionLogs() of an service instance specific TransactionLogRollbackExecutor
///          will rollback any existing transaction-logs for the given service instance and note down in the runtime,
///          that rollback for this service instance has ben done. Any further call to RollbackTransactionLogs() for the
///          same TransactionLogRollbackExecutor instance (service instance) has no effect, even if AFTER the 1st call
///          to RollbackTransactionLogs() new transaction-logs are created/added related to the same service instance,
///          they get ignored by a later call to RollbackTransactionLogs().
///
TEST_F(TransactionLogRollbackExecutorRollbackLogsFixture, WillOnlyRollBackLogsInFirstCall)
{
    // Given an empty/default constructed RollbackSynchronization and a LoLa runtime always returning it
    RollbackSynchronization rollback_synchronization{};
    ON_CALL(lola_runtime_mock_, GetRollbackSynchronization()).WillByDefault(ReturnRef(rollback_synchronization));

    // and given a unit under test for a given service instance and given proxy application (identified by
    // kDummyTransactionLogId)
    TransactionLogRollbackExecutor unit{
        service_data_control_, kDummyQualityType, kDummyProviderPid, kDummyTransactionLogId};

    // when we register a proxy-element
    auto& transaction_log_node_0 = RegisterProxyElementWithTransactionLogSet(kDummyElementFqId, kDummyTransactionLogId);

    // expect, that a rollback of transaction logs is successful
    ASSERT_TRUE(unit.RollbackTransactionLogs().has_value());
    // and that the transaction log for the previously registered proxy element is NOT active/does NOT need a rollback
    EXPECT_FALSE(transaction_log_node_0.IsActive());
    EXPECT_FALSE(transaction_log_node_0.NeedsRollback());

    // when we register the same proxy element a 2nd time (simulating a 2nd local proxy for the same service instance)
    auto& transaction_log_node_1 = RegisterProxyElementWithTransactionLogSet(kDummyElementFqId, kDummyTransactionLogId);
    // expect, that call to RollbackTransactionLogs() succeeds
    ASSERT_TRUE(unit.RollbackTransactionLogs().has_value());
    // and expect, that it did nothing -> i.e. the transaction log of the 2nd registered proxy element is still active
    EXPECT_TRUE(transaction_log_node_1.IsActive());
    // and it does NOT need rollback.
    EXPECT_FALSE(transaction_log_node_1.NeedsRollback());
}

TEST_F(TransactionLogRollbackExecutorRollbackLogsFixture, WillReturnErrorIfAnyLogsFailToRollBack)
{
    // Given an empty/default constructed RollbackSynchronization and a LoLa runtime always returning it
    RollbackSynchronization rollback_synchronization{};
    ON_CALL(lola_runtime_mock_, GetRollbackSynchronization()).WillByDefault(ReturnRef(rollback_synchronization));

    TransactionLogRollbackExecutor unit{
        service_data_control_, kDummyQualityType, kDummyProviderPid, kDummyTransactionLogId};

    auto& transaction_log_node_0 = RegisterProxyElementWithTransactionLogSet(kDummyElementFqId, kDummyTransactionLogId);
    auto& transaction_log_node_1 = RegisterProxyElementWithTransactionLogSet(kDummyElementFqId, kDummyTransactionLogId);

    transaction_log_node_1.GetTransactionLog().SubscribeTransactionBegin(0U);

    ASSERT_TRUE(unit.RollbackTransactionLogs().has_value());

    EXPECT_FALSE(transaction_log_node_0.IsActive());
    EXPECT_FALSE(transaction_log_node_0.NeedsRollback());
    EXPECT_TRUE(transaction_log_node_1.IsActive());
    EXPECT_TRUE(transaction_log_node_1.NeedsRollback());

    ASSERT_FALSE(unit.RollbackTransactionLogs().has_value());

    EXPECT_TRUE(transaction_log_node_1.IsActive());
    EXPECT_TRUE(transaction_log_node_1.NeedsRollback());
}

using TransactionLogRollbackExecutorMarkNeedRollbackDeathFixture = TransactionLogRollbackExecutorFixture;
TEST_F(TransactionLogRollbackExecutorMarkNeedRollbackDeathFixture, FailingToGetLolaRuntimeTerminates)
{
    ON_CALL(runtime_mock_guard_.mock_, GetBindingRuntime(BindingType::kLoLa)).WillByDefault(Return(nullptr));
    TransactionLogRollbackExecutor unit{
        service_data_control_, kDummyQualityType, kDummyProviderPid, kDummyTransactionLogId};
    EXPECT_DEATH(unit.RollbackTransactionLogs(), ".*");
}

}  // namespace
}  // namespace score::mw::com::impl::lola
