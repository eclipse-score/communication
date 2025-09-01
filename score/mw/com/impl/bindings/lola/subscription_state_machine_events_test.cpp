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
#include "score/mw/com/impl/bindings/lola/event_data_control_test_resources.h"
#include "score/mw/com/impl/bindings/lola/subscription_state_machine.h"
#include "score/mw/com/impl/bindings/lola/subscription_state_machine_states.h"
#include "score/mw/com/impl/bindings/lola/test/proxy_event_test_resources.h"
#include "score/mw/com/impl/bindings/lola/test/transaction_log_test_resources.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_id.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/proxy_binding.h"

#include <gtest/gtest.h>
#include <cstddef>
#include <memory>

namespace score::mw::com::impl::lola
{
namespace
{

using ::testing::Return;

const TransactionLogId kDummyTransactionLogId{10U};

/**
 * These test cases test the public methods of the SubscriptionStateMachine which cause state transitions.
 */
class StateMachineEventsFixture : public LolaProxyEventResources
{
  protected:
    StateMachineEventsFixture()
        : LolaProxyEventResources(),
          state_machine_{proxy_->GetQualityType(),
                         element_fq_id_,
                         kDummyPid,
                         proxy_->GetEventControl(element_fq_id_),
                         kDummyTransactionLogId}
    {
    }

    void SetUp() override
    {
        EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::NOT_SUBSCRIBED_STATE);
    }

    void TearDown() override
    {
        // We call Unsubscribe in the tear down to make sure that the state machine is correctly cleaned up.
        // Specifically, it's important that the Unsubscribe is recorded so that when ~TransactionLogRegistrationGuard
        // unregisters the TransactionLog, there are no open transactions.
        state_machine_.UnsubscribeEvent();
    }

    void EnterSubscriptionPending(const std::size_t max_samples) noexcept
    {
        EnterSubscribed(max_samples);
        state_machine_.StopOfferEvent();
        EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::SUBSCRIPTION_PENDING_STATE);
    }

    void EnterSubscribed(const std::size_t max_samples) noexcept
    {
        const auto subscription_result = state_machine_.SubscribeEvent(max_samples);
        EXPECT_TRUE(subscription_result.has_value());
        EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::SUBSCRIBED_STATE);
    }

    score::cpp::optional<std::reference_wrapper<TransactionLog>> GetTransactionLog(
        const TransactionLogId& transaction_log_id) noexcept
    {
        auto& transaction_log_set = proxy_->GetEventControl(element_fq_id_).data_control.GetTransactionLogSet();
        auto& transaction_logs = TransactionLogSetAttorney{transaction_log_set}.GetProxyTransactionLogs();
        auto result =
            std::find_if(transaction_logs.begin(), transaction_logs.end(), [&transaction_log_id](const auto& element) {
                return (element.IsActive() && (element.GetTransactionLogId() == transaction_log_id));
            });
        if (result == transaction_logs.end())
        {
            return {};
        }
        return {result->GetTransactionLog()};
    }

    bool IsProxyTransactionLogIdRegistered(const TransactionLogId& transaction_log_id) noexcept
    {
        return GetTransactionLog(transaction_log_id).has_value();
    }

    bool DoesTransactionLogContainSubscriptionTransaction(const TransactionLogId& transaction_log_id) noexcept
    {
        const auto transaction_log_result = GetTransactionLog(transaction_log_id);
        if (!transaction_log_result.has_value())
        {
            return false;
        }

        return TransactionLogAttorney{transaction_log_result.value().get()}.IsSubscribeTransactionSuccesfullyRecorded();
    }

    score::Result<TransactionLogSet::TransactionLogIndex> RegisterTransactionLog(
        const TransactionLogId& transaction_log_id) noexcept
    {
        auto& transaction_log_set = proxy_->GetEventControl(element_fq_id_).data_control.GetTransactionLogSet();
        return transaction_log_set.RegisterProxyElement(transaction_log_id);
    }

    SubscriptionStateMachine state_machine_;
    pid_t new_event_source_pid_{kDummyPid + 1};
};

using StateMachineNotSubscribedStateFixture = StateMachineEventsFixture;
TEST_F(StateMachineNotSubscribedStateFixture, CallingSubscribeWithValidSampleTransitionsToSubscribed)
{
    const auto subscription_result = state_machine_.SubscribeEvent(max_num_slots_);
    ASSERT_TRUE(subscription_result.has_value());

    EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::SUBSCRIBED_STATE);
}

TEST_F(StateMachineNotSubscribedStateFixture, CallingSubscribeWhenMaxSubscribersAlreadyReachedReturnsError)
{
    for (std::size_t i = 0; i < max_subscribers_; ++i)
    {
        const TransactionLogId dummy_transaction_log_id{static_cast<uid_t>(i)};
        const auto transaction_log_index_result = RegisterTransactionLog(dummy_transaction_log_id);
        EXPECT_TRUE(transaction_log_index_result.has_value());
    }

    const auto subscription_result = state_machine_.SubscribeEvent(max_num_slots_);
    ASSERT_FALSE(subscription_result.has_value());
    EXPECT_EQ(subscription_result.error(), ComErrc::kMaxSubscribersExceeded);
}

TEST_F(StateMachineNotSubscribedStateFixture, CanRepeatedlySubscribeAndUnsubscribe)
{
    for (auto i = 0; i < 100; ++i)
    {
        const auto subscription_result = state_machine_.SubscribeEvent(max_num_slots_);
        ASSERT_TRUE(subscription_result.has_value());
        EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::SUBSCRIBED_STATE);

        state_machine_.UnsubscribeEvent();
        EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::NOT_SUBSCRIBED_STATE);
    }
}

TEST_F(StateMachineNotSubscribedStateFixture, CallingSubscribeWithInValidSampleCountReturnsError)
{
    const auto subscription_result = state_machine_.SubscribeEvent(max_num_slots_ + 1);
    ASSERT_FALSE(subscription_result.has_value());

    EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::NOT_SUBSCRIBED_STATE);
}

TEST_F(StateMachineNotSubscribedStateFixture, CallingUnsubscribeDoesNothing)
{
    state_machine_.UnsubscribeEvent();
    EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::NOT_SUBSCRIBED_STATE);
}

TEST_F(StateMachineNotSubscribedStateFixture, CallingSubscribeWithValidSamplesWillRegisterTransactionLog)
{
    EXPECT_FALSE(IsProxyTransactionLogIdRegistered(kDummyTransactionLogId));
    score::cpp::ignore = state_machine_.SubscribeEvent(max_num_slots_);
    EXPECT_TRUE(IsProxyTransactionLogIdRegistered(kDummyTransactionLogId));
}

TEST_F(StateMachineNotSubscribedStateFixture, CallingSubscribeWithInvalidSamplesWillNotRegisterTransactionLog)
{
    EXPECT_FALSE(IsProxyTransactionLogIdRegistered(kDummyTransactionLogId));
    score::cpp::ignore = state_machine_.SubscribeEvent(max_num_slots_ + 1U);
    EXPECT_FALSE(IsProxyTransactionLogIdRegistered(kDummyTransactionLogId));
}

TEST_F(StateMachineNotSubscribedStateFixture, CallingSubscribeWithValidSamplesWillRecordSubscriptionTransaction)
{
    EXPECT_FALSE(DoesTransactionLogContainSubscriptionTransaction(kDummyTransactionLogId));
    score::cpp::ignore = state_machine_.SubscribeEvent(max_num_slots_);
    EXPECT_TRUE(DoesTransactionLogContainSubscriptionTransaction(kDummyTransactionLogId));
}

TEST_F(StateMachineNotSubscribedStateFixture, CallingSubscribeWithInvalidSamplesWillAbortSubscriptionTransaction)
{
    EXPECT_FALSE(DoesTransactionLogContainSubscriptionTransaction(kDummyTransactionLogId));
    score::cpp::ignore = state_machine_.SubscribeEvent(max_num_slots_ + 1U);
    EXPECT_FALSE(DoesTransactionLogContainSubscriptionTransaction(kDummyTransactionLogId));
}

TEST_F(StateMachineNotSubscribedStateFixture, CallingStopOfferEventBeforeSubscribeWillTransitionToSubscriptionPending)
{
    state_machine_.StopOfferEvent();
    EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::NOT_SUBSCRIBED_STATE);

    const auto subscription_result = state_machine_.SubscribeEvent(max_num_slots_);
    ASSERT_TRUE(subscription_result.has_value());
    EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::SUBSCRIPTION_PENDING_STATE);
}

TEST_F(StateMachineNotSubscribedStateFixture, CallingReOfferEventBeforeSubscribeWillTransitionToSubscribed)
{
    state_machine_.ReOfferEvent(new_event_source_pid_);
    EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::NOT_SUBSCRIBED_STATE);

    const auto subscription_result = state_machine_.SubscribeEvent(max_num_slots_);
    ASSERT_TRUE(subscription_result.has_value());
    EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::SUBSCRIBED_STATE);
}

TEST_F(StateMachineNotSubscribedStateFixture,
       CallingStopOfferEventThenReOfferEventBeforeSubscribeWillTransitionToSubscribed)
{
    state_machine_.StopOfferEvent();

    state_machine_.ReOfferEvent(new_event_source_pid_);
    EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::NOT_SUBSCRIBED_STATE);

    const auto subscription_result = state_machine_.SubscribeEvent(max_num_slots_);
    ASSERT_TRUE(subscription_result.has_value());
    EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::SUBSCRIBED_STATE);
}

using StateMachineSubscriptionPendingStateFixture = StateMachineEventsFixture;
TEST_F(StateMachineSubscriptionPendingStateFixture, CallingSubscribeWithSameMaxSampleCountDoesNothing)
{
    EnterSubscriptionPending(max_num_slots_);

    const auto subscription_result = state_machine_.SubscribeEvent(max_num_slots_);
    ASSERT_TRUE(subscription_result.has_value());
    EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::SUBSCRIPTION_PENDING_STATE);
}

TEST_F(StateMachineSubscriptionPendingStateFixture, CallingSubscribeWithDifferentMaxSampleCountReturnsError)
{
    const std::size_t other_max_sample_value{max_num_slots_ + 1U};
    EnterSubscriptionPending(max_num_slots_);

    const auto subscription_result = state_machine_.SubscribeEvent(other_max_sample_value);
    ASSERT_FALSE(subscription_result.has_value());
    EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::SUBSCRIPTION_PENDING_STATE);
}

TEST_F(StateMachineSubscriptionPendingStateFixture, CanRepeatedlySubscribeAndUnsubscribe)
{
    EnterSubscriptionPending(max_num_slots_);
    for (auto i = 0; i < 100; ++i)
    {
        state_machine_.UnsubscribeEvent();
        EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::NOT_SUBSCRIBED_STATE);

        const auto subscription_result = state_machine_.SubscribeEvent(max_num_slots_);
        ASSERT_TRUE(subscription_result.has_value());
        EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::SUBSCRIPTION_PENDING_STATE);
    }
}

TEST_F(StateMachineSubscriptionPendingStateFixture, CallingUnsubscribeTransitionsToNotSubscribed)
{
    EnterSubscriptionPending(max_num_slots_);
    state_machine_.UnsubscribeEvent();
    EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::NOT_SUBSCRIBED_STATE);
}

using StateMachineSubscriptionPendingStateDeathTest = StateMachineSubscriptionPendingStateFixture;
TEST_F(StateMachineSubscriptionPendingStateDeathTest, CallingStopOfferEventTerminates)
{
    const auto test_function = [this] {
        EnterSubscriptionPending(max_num_slots_);
        state_machine_.StopOfferEvent();
    };
    EXPECT_DEATH(test_function(), ".*");
}

TEST_F(StateMachineSubscriptionPendingStateFixture, CallingReOfferEventTransitionsToSubscribed)
{
    EnterSubscriptionPending(max_num_slots_);
    state_machine_.ReOfferEvent(new_event_source_pid_);
    EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::SUBSCRIBED_STATE);
}

TEST_F(StateMachineSubscriptionPendingStateFixture, CallingUnsubscribeWillUnregisterTransactionLog)
{
    EnterSubscriptionPending(max_num_slots_);

    EXPECT_TRUE(IsProxyTransactionLogIdRegistered(kDummyTransactionLogId));
    state_machine_.UnsubscribeEvent();
    EXPECT_FALSE(IsProxyTransactionLogIdRegistered(kDummyTransactionLogId));
}

TEST_F(StateMachineSubscriptionPendingStateFixture, CallingUnsubscribeWillRecordUnsubscribeTransaction)
{
    EnterSubscriptionPending(max_num_slots_);

    EXPECT_TRUE(DoesTransactionLogContainSubscriptionTransaction(kDummyTransactionLogId));
    state_machine_.UnsubscribeEvent();
    EXPECT_FALSE(DoesTransactionLogContainSubscriptionTransaction(kDummyTransactionLogId));
}

using StateMachineSubscribedStateFixture = StateMachineEventsFixture;
TEST_F(StateMachineSubscribedStateFixture, CallingSubscribeWithSameMaxSampleCountDoesNothing)
{
    EnterSubscribed(max_num_slots_);

    const auto subscription_result = state_machine_.SubscribeEvent(max_num_slots_);
    ASSERT_TRUE(subscription_result.has_value());
    EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::SUBSCRIBED_STATE);
}

TEST_F(StateMachineSubscribedStateFixture, CallingSubscribeWithDifferentMaxSampleCountReturnsError)
{
    const std::size_t other_max_sample_value{max_num_slots_ + 1U};
    EnterSubscribed(max_num_slots_);

    const auto subscription_result = state_machine_.SubscribeEvent(other_max_sample_value);
    ASSERT_FALSE(subscription_result.has_value());
    EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::SUBSCRIBED_STATE);
}

TEST_F(StateMachineSubscribedStateFixture, CallingUnsubscribeTransitionsToNotSubscribed)
{
    EnterSubscribed(max_num_slots_);
    state_machine_.UnsubscribeEvent();
    EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::NOT_SUBSCRIBED_STATE);
}

TEST_F(StateMachineSubscribedStateFixture, CallingStopOfferEventTransitionsToSubscriptionPending)
{
    EnterSubscribed(max_num_slots_);
    state_machine_.StopOfferEvent();
    EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::SUBSCRIPTION_PENDING_STATE);
}

TEST_F(StateMachineSubscribedStateFixture,
       CallingStopOfferEventWillPreventReenteringSubscribedUntilReofferEventIsCalled)
{
    EnterSubscribed(max_num_slots_);
    state_machine_.StopOfferEvent();
    EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::SUBSCRIPTION_PENDING_STATE);

    state_machine_.UnsubscribeEvent();
    EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::NOT_SUBSCRIBED_STATE);

    const auto subscription_result = state_machine_.SubscribeEvent(max_num_slots_);
    ASSERT_TRUE(subscription_result.has_value());
    EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::SUBSCRIPTION_PENDING_STATE);

    state_machine_.ReOfferEvent(new_event_source_pid_);
    EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::SUBSCRIBED_STATE);
}

TEST_F(StateMachineSubscribedStateFixture, CallingUnsubscribeWillUnregisterTransactionLog)
{
    EnterSubscribed(max_num_slots_);

    EXPECT_TRUE(IsProxyTransactionLogIdRegistered(kDummyTransactionLogId));
    state_machine_.UnsubscribeEvent();
    EXPECT_FALSE(IsProxyTransactionLogIdRegistered(kDummyTransactionLogId));
}

TEST_F(StateMachineSubscribedStateFixture, CallingUnsubscribeWillRecordUnsubscribeTransaction)
{
    EnterSubscribed(max_num_slots_);

    EXPECT_TRUE(DoesTransactionLogContainSubscriptionTransaction(kDummyTransactionLogId));
    state_machine_.UnsubscribeEvent();
    EXPECT_FALSE(DoesTransactionLogContainSubscriptionTransaction(kDummyTransactionLogId));
}

TEST_F(StateMachineSubscribedStateFixture, CallingReOfferEventDoesNothing)
{
    EnterSubscribed(max_num_slots_);

    state_machine_.ReOfferEvent(new_event_source_pid_);
    EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::SUBSCRIBED_STATE);
}

}  // namespace
}  // namespace score::mw::com::impl::lola
