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
#include "score/mw/com/impl/bindings/lola/subscription_state_machine.h"
#include "score/mw/com/impl/bindings/lola/subscription_state_machine_states.h"
#include "score/mw/com/impl/bindings/lola/test/proxy_event_test_resources.h"
#include "score/mw/com/impl/scoped_event_receive_handler.h"

#include "score/language/safecpp/scoped_function/scope.h"

#include <gtest/gtest.h>
#include <chrono>
#include <future>
#include <memory>

namespace score::mw::com::impl::lola
{
namespace
{

using ::testing::_;
using ::testing::InvokeWithoutArgs;
using ::testing::MockFunction;
using ::testing::Not;
using ::testing::Return;
using ::testing::StrictMock;

MATCHER(FutureValueIsSet, "")
{
    return arg.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

const TransactionLogId kDummyTransactionLogId{10U};

/**
 * These test cases test the public methods of the SubscriptionStateMachine which don't cause state transitions.
 */
class StateMachineMethodsFixture : public LolaProxyEventResources
{
  protected:
    StateMachineMethodsFixture()
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
        ON_CALL(runtime_mock_.mock_, GetBindingRuntime(BindingType::kLoLa)).WillByDefault(Return(&binding_runtime_));
        EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::NOT_SUBSCRIBED_STATE);
    }

    void TearDown() override
    {
        // We call Unsubscribe in the tear down to make sure that the state machine is correctly cleaned up.
        // Specifically, it's important that the Unsubscribe is recorded so that when ~TransactionLogRegistrationGuard
        // unregisters the TransactionLog, there are no open transactions.
        state_machine_.UnsubscribeEvent();
        // Any event receive handlers created throughout a test gets removed again.
        event_receive_handlers_.clear();
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

    std::weak_ptr<ScopedEventReceiveHandler> CreateMockScopedEventReceiveHandler(
        StrictMock<MockFunction<void()>>& mock_function)
    {
        auto event_receive_handler =
            std::make_shared<ScopedEventReceiveHandler>(event_receive_handler_scope_, mock_function.AsStdFunction());
        event_receive_handlers_.push_back(event_receive_handler);
        return event_receive_handler;
    }

    const score::cpp::optional<SlotCollector>& GetConstSlotCollector() const
    {
        return state_machine_.GetSlotCollectorLockFree();
    }

    SubscriptionStateMachine state_machine_;
    pid_t new_event_source_pid_{kDummyPid + 1};
    safecpp::Scope<> event_receive_handler_scope_{};

    // we have a vector of std::shared_ptr<ScopedEventReceiveHandler>> as some test cases create multiple
    // receive-handlers, which have to survive the entire test.
    std::vector<std::shared_ptr<ScopedEventReceiveHandler>> event_receive_handlers_;
};

using StateMachineMethodsNotSubscribedStateFixture = StateMachineMethodsFixture;
TEST_F(StateMachineMethodsNotSubscribedStateFixture, CallingSubscribeSuccessfullyWillCreateSlotCollector)
{
    EXPECT_FALSE(state_machine_.GetSlotCollectorLockFree().has_value());
    EXPECT_TRUE(state_machine_.SubscribeEvent(max_num_slots_).has_value());
    EXPECT_TRUE(state_machine_.GetSlotCollectorLockFree().has_value());
}

TEST_F(StateMachineMethodsNotSubscribedStateFixture, CallingSubscribeUnsuccessfullyWillNotCreateSlotCollector)
{
    EXPECT_FALSE(state_machine_.GetSlotCollectorLockFree().has_value());
    EXPECT_FALSE(state_machine_.SubscribeEvent(max_num_slots_ + 1U).has_value());
    EXPECT_FALSE(state_machine_.GetSlotCollectorLockFree().has_value());
}

TEST_F(StateMachineMethodsNotSubscribedStateFixture, CallingSubscribeWillRegisterLatestReceiveHandler)
{
    StrictMock<MockFunction<void()>> first_receive_handler{};
    StrictMock<MockFunction<void()>> second_receive_handler{};

    // Expecting that the second receive handler will be called
    EXPECT_CALL(first_receive_handler, Call()).Times(0);
    EXPECT_CALL(second_receive_handler, Call());

    auto event_notification_handler_future = ExpectRegisterEventNotification();

    state_machine_.SetReceiveHandler(CreateMockScopedEventReceiveHandler(first_receive_handler));
    state_machine_.SetReceiveHandler(CreateMockScopedEventReceiveHandler(second_receive_handler));

    score::cpp::ignore = state_machine_.SubscribeEvent(max_num_slots_);

    // When the event notification handler is called.
    ASSERT_TRUE(event_notification_handler_future.valid());
    auto event_notification_handler = event_notification_handler_future.get();
    (*event_notification_handler)();
}

TEST_F(StateMachineMethodsNotSubscribedStateFixture,
       CallingSubscribeAfterReofferWillRegisterReceiveHandlerWithLatestPid)
{
    StrictMock<MockFunction<void()>> receive_handler{};

    // Expecting that the receive handler will be called
    EXPECT_CALL(receive_handler, Call());

    auto event_notification_handler_future = ExpectRegisterEventNotification(new_event_source_pid_);

    state_machine_.SetReceiveHandler(CreateMockScopedEventReceiveHandler(receive_handler));

    state_machine_.ReOfferEvent(new_event_source_pid_);

    score::cpp::ignore = state_machine_.SubscribeEvent(max_num_slots_);

    // When the event notification handler is called.
    ASSERT_TRUE(event_notification_handler_future.valid());
    auto event_notification_handler = event_notification_handler_future.get();
    (*event_notification_handler)();
}

TEST_F(StateMachineMethodsNotSubscribedStateFixture,
       CallingSubscribeAfterStopOfferAndReofferWillRegisterReceiveHandlerWithLatestPid)
{
    StrictMock<MockFunction<void()>> receive_handler{};

    // Expecting that the receive handler will be called
    EXPECT_CALL(receive_handler, Call());

    auto event_notification_handler_future = ExpectRegisterEventNotification(new_event_source_pid_);

    state_machine_.SetReceiveHandler(CreateMockScopedEventReceiveHandler(receive_handler));

    state_machine_.StopOfferEvent();
    state_machine_.ReOfferEvent(new_event_source_pid_);

    score::cpp::ignore = state_machine_.SubscribeEvent(max_num_slots_);

    // When the event notification handler is called.
    ASSERT_TRUE(event_notification_handler_future.valid());
    auto event_notification_handler = event_notification_handler_future.get();
    (*event_notification_handler)();
}

TEST_F(StateMachineMethodsNotSubscribedStateFixture, CallingSubscribeAfterUnsettingReceiveHandlerWillNotRegisterHandler)
{
    StrictMock<MockFunction<void()>> receive_handler{};

    EXPECT_CALL(*mock_service_,
                RegisterEventNotification(QualityType::kASIL_QM, element_fq_id_, ::testing::_, kDummyPid))
        .Times(0);

    state_machine_.SetReceiveHandler(CreateMockScopedEventReceiveHandler(receive_handler));
    state_machine_.UnsetReceiveHandler();

    score::cpp::ignore = state_machine_.SubscribeEvent(max_num_slots_);
}

TEST_F(StateMachineMethodsNotSubscribedStateFixture, CallingGetMaxSampleCountReturnsEmpty)
{
    EXPECT_FALSE(state_machine_.GetMaxSampleCount().has_value());
}

TEST_F(StateMachineMethodsNotSubscribedStateFixture, CallingGetSlotCollectorReturnsEmpty)
{
    // Given that the state machine is currently in not subscribed state

    // When calling GetSlotCollector
    auto& slot_collector_result = state_machine_.GetSlotCollectorLockFree();

    // Then an empty optional should be returned
    EXPECT_FALSE(slot_collector_result.has_value());
}

TEST_F(StateMachineMethodsNotSubscribedStateFixture, CallingGetConstSlotCollectorReturnsEmpty)
{
    // Given that the state machine is currently in not subscribed state

    // When calling GetSlotCollector in a const context
    const auto& slot_collector_result = GetConstSlotCollector();

    // Then an empty optional should be returned
    EXPECT_FALSE(slot_collector_result.has_value());
}

TEST_F(StateMachineMethodsNotSubscribedStateFixture, CallingGetTransactionLogIndexReturnsEmpty)
{
    // Given that the state machine is currently in not subscribed state

    // When calling GetTransactionLogIndex
    const auto transaction_log_index_result = state_machine_.GetTransactionLogIndex();

    // Then an empty optional should be returned
    EXPECT_FALSE(transaction_log_index_result.has_value());
}

using StateMachineMethodsSubscriptionPendingStateFixture = StateMachineMethodsFixture;
TEST_F(StateMachineMethodsSubscriptionPendingStateFixture, CallingUnsubscribeWillClearSlotCollector)
{
    EnterSubscriptionPending(max_num_slots_);
    EXPECT_TRUE(state_machine_.GetSlotCollectorLockFree().has_value());

    state_machine_.UnsubscribeEvent();
    EXPECT_FALSE(state_machine_.GetSlotCollectorLockFree().has_value());
}

TEST_F(StateMachineMethodsSubscriptionPendingStateFixture, CallingReofferWillReregisterExistingReceiveHandler)
{
    StrictMock<MockFunction<void()>> receive_handler{};

    // Expecting that the receive handler registered while in NotSubscribed state will be called
    EXPECT_CALL(receive_handler, Call());

    // and that the receive handler registered while in NotSubscribed will be registered with the first PID
    auto event_notification_handler_future = ExpectRegisterEventNotification();

    // and then it will be re-registered with the new PID
    ExpectReregisterEventNotification(new_event_source_pid_);

    state_machine_.SetReceiveHandler(CreateMockScopedEventReceiveHandler(receive_handler));

    // When we transition to the subscribed state and then to subscription pending
    EnterSubscriptionPending(max_num_slots_);

    // and then we transition back to Subscribed state
    state_machine_.ReOfferEvent(new_event_source_pid_);

    // and the event notification handler is called.
    ASSERT_TRUE(event_notification_handler_future.valid());
    auto event_notification_handler = event_notification_handler_future.get();
    (*event_notification_handler)();
}

TEST_F(StateMachineMethodsSubscriptionPendingStateFixture,
       CallingReofferWillRegisterReceiveHandlerSetInSubscriptionPending)
{
    StrictMock<MockFunction<void()>> receive_handler{};

    // Expecting that the receive handler registered while in SubsriptionPending state will be called
    EXPECT_CALL(receive_handler, Call());

    // and that the receive handler registered while in SubsriptionPending will be registered with the new PID
    auto event_notification_handler_future = ExpectRegisterEventNotification(new_event_source_pid_);

    EnterSubscriptionPending(max_num_slots_);

    state_machine_.SetReceiveHandler(CreateMockScopedEventReceiveHandler(receive_handler));

    // When we transition to Subscribed state
    state_machine_.ReOfferEvent(new_event_source_pid_);

    // and the event notification handler is called.
    ASSERT_TRUE(event_notification_handler_future.valid());
    auto event_notification_handler = event_notification_handler_future.get();
    (*event_notification_handler)();
}

TEST_F(StateMachineMethodsSubscriptionPendingStateFixture,
       CallingReofferWillOnlyRegisterTheSameHandlerOnceAndThenWillReregister)
{
    pid_t second_event_source_pid_{new_event_source_pid_ + 1};
    pid_t third_event_source_pid_{second_event_source_pid_ + 1};

    StrictMock<MockFunction<void()>> receive_handler{};

    // Expecting that the receive handler registered while in SubsriptionPending state will be called
    EXPECT_CALL(receive_handler, Call());

    // and that the receive handler registered while in SubsriptionPending will be registered with the new PID
    auto event_notification_handler_future = ExpectRegisterEventNotification(new_event_source_pid_);

    // and the second time we call ReOfferEvent, the handler will be reregistered
    ExpectReregisterEventNotification(second_event_source_pid_);
    ExpectReregisterEventNotification(third_event_source_pid_);

    EnterSubscriptionPending(max_num_slots_);

    state_machine_.SetReceiveHandler(CreateMockScopedEventReceiveHandler(receive_handler));

    // When we transition to Subscribed state
    state_machine_.ReOfferEvent(new_event_source_pid_);

    // and then transition to SubscriptionPending and Subscribed state
    state_machine_.StopOfferEvent();
    state_machine_.ReOfferEvent(second_event_source_pid_);

    // and then transition to SubscriptionPending and Subscribed state again
    state_machine_.StopOfferEvent();
    state_machine_.ReOfferEvent(third_event_source_pid_);

    // and the event notification handler is called.
    ASSERT_TRUE(event_notification_handler_future.valid());
    auto event_notification_handler = event_notification_handler_future.get();
    (*event_notification_handler)();
}

TEST_F(StateMachineMethodsSubscriptionPendingStateFixture, CallingReofferWillRegisterNewestReceiveHandler)
{
    StrictMock<MockFunction<void()>> receive_handler_not_subscribed{};
    StrictMock<MockFunction<void()>> receive_handler_subscription_pending{};

    // Expecting that the receive handler registered while in NotSubscribed state will never be called and the handler
    // registered while in SubscriptionPending state will be called
    EXPECT_CALL(receive_handler_not_subscribed, Call()).Times(0);
    EXPECT_CALL(receive_handler_subscription_pending, Call());

    // and that the receive handler registered while in NotSubscribed will be registered with the first PID
    score::cpp::ignore = ExpectRegisterEventNotification();

    // and that the receive handler registered while in SubscriptionPending will be registered with the new PID
    auto event_notification_handler_future = ExpectRegisterEventNotification(new_event_source_pid_);

    // and that ReregisterEventNotification will never be called
    EXPECT_CALL(*mock_service_, ReregisterEventNotification(_, _, _)).Times(0);

    state_machine_.SetReceiveHandler(CreateMockScopedEventReceiveHandler(receive_handler_not_subscribed));

    // When we transition to the subscribed state and then to subscription pending
    EnterSubscriptionPending(max_num_slots_);

    state_machine_.SetReceiveHandler(CreateMockScopedEventReceiveHandler(receive_handler_subscription_pending));

    // and then we transition back to Subscribed state
    state_machine_.ReOfferEvent(new_event_source_pid_);

    // and the event notification handler from the handler registered in SubscriptionPending is called.
    ASSERT_TRUE(event_notification_handler_future.valid());
    auto event_notification_handler = event_notification_handler_future.get();
    (*event_notification_handler)();
}

TEST_F(StateMachineMethodsSubscriptionPendingStateFixture, CallingSetReceiveHandlerDoesNotRegisterHandler)
{
    StrictMock<MockFunction<void()>> receive_handler{};

    // Expecting that the handler will never be Registered or Unregistered
    EXPECT_CALL(*mock_service_,
                RegisterEventNotification(QualityType::kASIL_QM, element_fq_id_, ::testing::_, kDummyPid))
        .Times(0);
    EXPECT_CALL(*mock_service_,
                UnregisterEventNotification(QualityType::kASIL_QM, element_fq_id_, ::testing::_, kDummyPid))
        .Times(0);

    // and that the receive handler registered while in SubsriptionPending state will never be called
    EXPECT_CALL(receive_handler, Call()).Times(0);

    // When we enter SubscriptionPending state
    EnterSubscriptionPending(max_num_slots_);

    // And then register the receive handler
    state_machine_.SetReceiveHandler(CreateMockScopedEventReceiveHandler(receive_handler));
}

TEST_F(StateMachineMethodsSubscriptionPendingStateFixture,
       RegisteredHandlerIsSavedAndRegisteredOnSuccessfulSubscription)
{
    StrictMock<MockFunction<void()>> receive_handler{};

    // Expecting that the handler will be Registered when we succesfully subscribe i.e. enter the
    // SubscribedState and Unregistered on destruction
    auto event_notification_handler_future = ExpectRegisterEventNotification();
    ExpectUnregisterEventNotification();

    // and that the receive handler registered while in SubsriptionPending state will be called
    EXPECT_CALL(receive_handler, Call());

    // When we enter SubscriptionPending state
    EnterSubscriptionPending(max_num_slots_);

    // And when we register the receive handler
    state_machine_.SetReceiveHandler(CreateMockScopedEventReceiveHandler(receive_handler));

    // Then the registration is not yet done
    EXPECT_THAT(event_notification_handler_future, Not(FutureValueIsSet()));

    // Then when we Unsubscribe
    state_machine_.UnsubscribeEvent();
    EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::NOT_SUBSCRIBED_STATE);

    // and then re-subscribe
    score::cpp::ignore = state_machine_.SubscribeEvent(max_num_slots_);
    EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::SUBSCRIPTION_PENDING_STATE);

    // Then the registration is done
    EXPECT_THAT(event_notification_handler_future, FutureValueIsSet());

    // and the event notification handler is called.
    ASSERT_TRUE(event_notification_handler_future.valid());
    auto event_notification_handler = event_notification_handler_future.get();
    (*event_notification_handler)();
}

TEST_F(StateMachineMethodsSubscriptionPendingStateFixture,
       RegisteredHandlerIsSavedAndRegisteredOnSuccessfulSubscriptionAfterReOfferEvent)
{
    StrictMock<MockFunction<void()>> receive_handler{};

    // Expecting that the handler will be Registered when we succesfully subscribe i.e. enter the
    // SubscribedState and Unregistered on destruction
    auto event_notification_handler_future = ExpectRegisterEventNotification(new_event_source_pid_);
    ExpectUnregisterEventNotification(new_event_source_pid_);

    // and that the receive handler registered while in SubsriptionPending state will be called
    EXPECT_CALL(receive_handler, Call());

    // When we enter SubscriptionPending state
    EnterSubscriptionPending(max_num_slots_);

    // And when we register the receive handler
    state_machine_.SetReceiveHandler(CreateMockScopedEventReceiveHandler(receive_handler));

    // Then the registration is not yet done
    EXPECT_THAT(event_notification_handler_future, Not(FutureValueIsSet()));

    // Then when we Unsubscribe
    state_machine_.UnsubscribeEvent();
    EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::NOT_SUBSCRIBED_STATE);

    // and then we get a reoffer event
    state_machine_.ReOfferEvent(new_event_source_pid_);
    EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::NOT_SUBSCRIBED_STATE);

    // and then re-subscribe
    score::cpp::ignore = state_machine_.SubscribeEvent(max_num_slots_);
    EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::SUBSCRIBED_STATE);

    // Then the registration is done
    EXPECT_THAT(event_notification_handler_future, FutureValueIsSet());

    // and the event notification handler is called.
    ASSERT_TRUE(event_notification_handler_future.valid());
    auto event_notification_handler = event_notification_handler_future.get();
    (*event_notification_handler)();
}

TEST_F(StateMachineMethodsSubscriptionPendingStateFixture,
       CallingReofferAfterUnsettingReceiveHandlerWillNotRegisterHandler)
{
    StrictMock<MockFunction<void()>> receive_handler{};

    // Expecting that the an event notification handler will never be registered
    EXPECT_CALL(*mock_service_,
                RegisterEventNotification(QualityType::kASIL_QM, element_fq_id_, ::testing::_, kDummyPid))
        .Times(0);

    EnterSubscriptionPending(max_num_slots_);

    // When we set and then unset the receive handler while in SubscriptionPending state
    state_machine_.SetReceiveHandler(CreateMockScopedEventReceiveHandler(receive_handler));
    state_machine_.UnsetReceiveHandler();

    // and we transition to Subscribed state
    state_machine_.ReOfferEvent(new_event_source_pid_);
}

TEST_F(StateMachineMethodsSubscriptionPendingStateFixture, CallingGetMaxSampleCountReturnsCorrectValue)
{
    EnterSubscriptionPending(max_num_slots_);

    const auto retrieved_max_sample_count = state_machine_.GetMaxSampleCount();
    ASSERT_TRUE(retrieved_max_sample_count.has_value());
    EXPECT_EQ(retrieved_max_sample_count.value(), max_num_slots_);
}

TEST_F(StateMachineMethodsSubscriptionPendingStateFixture, CallingGetSlotCollectorReturnsValidSlotCollector)
{
    // Given that the state machine is currently in subscription pending
    EnterSubscriptionPending(max_num_slots_);

    // When calling GetSlotCollector
    auto& slot_collector_result = state_machine_.GetSlotCollectorLockFree();

    // Then a valid slot collector should be returned
    EXPECT_TRUE(slot_collector_result.has_value());
}

TEST_F(StateMachineMethodsSubscriptionPendingStateFixture, CallingGetConstSlotCollectorReturnsValidSlotCollector)
{
    // Given that the state machine is currently in subscription pending
    EnterSubscriptionPending(max_num_slots_);

    // When calling GetSlotCollector in a const context
    const auto& slot_collector_result = GetConstSlotCollector();

    // Then a valid slot collector should be returned
    EXPECT_TRUE(slot_collector_result.has_value());
}

TEST_F(StateMachineMethodsSubscriptionPendingStateFixture, CallingGetTransactionLogIndexReturnsValidTransactionLogIndex)
{
    // Given that the state machine is currently in subscription pending
    EnterSubscriptionPending(max_num_slots_);

    // When calling GetTransactionLogIndex
    const auto transaction_log_index_result = state_machine_.GetTransactionLogIndex();

    // Then a valid transaction log index should be returned
    EXPECT_TRUE(transaction_log_index_result.has_value());
}

using StateMachineMethodsSubscribedStateFixture = StateMachineMethodsFixture;
TEST_F(StateMachineMethodsSubscribedStateFixture, CallingUnsubscribeWillClearSlotCollector)
{
    EnterSubscribed(max_num_slots_);
    EXPECT_TRUE(state_machine_.GetSlotCollectorLockFree().has_value());

    state_machine_.UnsubscribeEvent();
    EXPECT_FALSE(state_machine_.GetSlotCollectorLockFree().has_value());
}

TEST_F(StateMachineMethodsSubscribedStateFixture, CallingUnsubscribeWillUnregisterEventHandler)
{
    StrictMock<MockFunction<void()>> receive_handler{};

    // Given that we're in subscribed state
    EnterSubscribed(max_num_slots_);

    // Expecting that an event handler will be registered
    ExpectRegisterEventNotification();

    // and then unregistered
    ExpectUnregisterEventNotification();

    // When we set a receive handler
    state_machine_.SetReceiveHandler(CreateMockScopedEventReceiveHandler(receive_handler));

    // And then unsubscribe
    state_machine_.UnsubscribeEvent();
}

TEST_F(StateMachineMethodsSubscribedStateFixture, CallingSetReceiveHandlerRegistersHandler)
{
    StrictMock<MockFunction<void()>> receive_handler{};

    // Expecting that the handler will be Registered and Unregistered on destruction
    auto event_notification_handler_future = ExpectRegisterEventNotification();
    ExpectUnregisterEventNotification();

    // When we enter the subcribed state
    EnterSubscribed(max_num_slots_);

    // And then register the receive handler
    state_machine_.SetReceiveHandler(CreateMockScopedEventReceiveHandler(receive_handler));

    // Then the registration is done
    EXPECT_THAT(event_notification_handler_future, FutureValueIsSet());
}

TEST_F(StateMachineMethodsSubscribedStateFixture, CallingUnsetReceiveHandlerWhenSubscribedRemovesHandler)
{
    StrictMock<MockFunction<void()>> receive_handler{};

    ExpectRegisterEventNotification();
    ExpectUnregisterEventNotification();

    state_machine_.SetReceiveHandler(CreateMockScopedEventReceiveHandler(receive_handler));

    EnterSubscribed(max_num_slots_);

    state_machine_.UnsetReceiveHandler();
}

TEST_F(StateMachineMethodsSubscribedStateFixture, CallingGetMaxSampleCountReturnsCorrectValue)
{
    EnterSubscribed(max_num_slots_);

    const auto retrieved_max_sample_count = state_machine_.GetMaxSampleCount();
    ASSERT_TRUE(retrieved_max_sample_count.has_value());
    EXPECT_EQ(retrieved_max_sample_count.value(), max_num_slots_);
}

TEST_F(StateMachineMethodsSubscribedStateFixture, CallingGetSlotCollectorReturnsValidSlotCollector)
{
    // Given that the state machine is currently in subscribed state
    EnterSubscribed(max_num_slots_);

    // When calling GetSlotCollector
    auto& slot_collector_result = state_machine_.GetSlotCollectorLockFree();

    // Then a valid slot collector should be returned
    EXPECT_TRUE(slot_collector_result.has_value());
}

TEST_F(StateMachineMethodsSubscribedStateFixture, CallingGetConstSlotCollectorReturnsValidSlotCollector)
{
    // Given that the state machine is currently in subscribed state
    EnterSubscribed(max_num_slots_);

    // When calling GetSlotCollector in a const context
    const auto& slot_collector_result = GetConstSlotCollector();

    // Then a valid slot collector should be returned
    EXPECT_TRUE(slot_collector_result.has_value());
}

TEST_F(StateMachineMethodsSubscribedStateFixture, CallingGetTransactionLogIndexReturnsValidTransactionLogIndex)
{
    // Given that the state machine is currently in subscribed state
    EnterSubscribed(max_num_slots_);

    // When calling GetTransactionLogIndex
    const auto transaction_log_index_result = state_machine_.GetTransactionLogIndex();

    // Then a valid transaction log index should be returned
    EXPECT_TRUE(transaction_log_index_result.has_value());
}

}  // namespace
}  // namespace score::mw::com::impl::lola
