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
#include "score/mw/com/impl/bindings/lola/subscription_state_machine.h"

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/event_data_control_mixed.h"
#include "score/mw/com/impl/bindings/lola/event_subscription_control.h"
#include "score/mw/com/impl/bindings/lola/subscription_state_machine_states.h"
#include "score/mw/com/impl/bindings/lola/test_doubles/fake_memory_resource.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_id.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_set.h"
#include "score/mw/com/impl/configuration/quality_type.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl::lola
{
namespace
{

constexpr SlotIndexType kMaxSlots{5U};
constexpr std::uint8_t kMaxSubscribers{5U};
const ElementFqId kElementFqId{1U, 2U, 3U, ServiceElementType::EVENT};
constexpr pid_t kDummyPid{42};
const TransactionLogId kTransactionLogId{1U};

/// Tests covering the CoherencyType::kCoherent-proxy-reading-a-kMixed-skeleton-event constructor overload of
/// SubscriptionStateMachine: verifies that Subscribe()/Unsubscribe() work correctly using a 0-slot TransactionLog,
/// which is only used for Subscribe/Unsubscribe transaction logging (crash-recovery of the subscriber count), not
/// for any per-slot reference-counting, just like the "pure" CoherencyType::kCoherent case (see
/// subscription_state_machine_coherent_test.cpp).
class StateMachineMixedCoherentFixture : public ::testing::Test
{
  protected:
    void TearDown() override
    {
        // We call Unsubscribe in the tear down to make sure that the state machine is correctly cleaned up.
        // Specifically, it's important that the Unsubscribe is recorded so that when ~TransactionLogRegistrationGuard
        // unregisters the TransactionLog, there are no open transactions (see NotSubscribedState::OnEntry()).
        state_machine_.UnsubscribeEvent();
    }

    FakeMemoryResource memory_{};
    EventDataControlMixed event_data_control_mixed_{kMaxSlots, memory_};
    ConsumerEventDataControlMixedCoherentLocalView consumer_event_data_control_local_view_{event_data_control_mixed_};
    EventSubscriptionControl<> subscription_control_{kMaxSlots, kMaxSubscribers, true};
    TransactionLogSet transaction_log_set_{kMaxSubscribers, 0U, memory_};
    SubscriptionStateMachine state_machine_{QualityType::kASIL_QM,
                                            kElementFqId,
                                            kDummyPid,
                                            consumer_event_data_control_local_view_,
                                            subscription_control_,
                                            transaction_log_set_,
                                            kTransactionLogId};
};

TEST_F(StateMachineMixedCoherentFixture, StartsInNotSubscribedState)
{
    EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::NOT_SUBSCRIBED_STATE);
}

TEST_F(StateMachineMixedCoherentFixture, SubscribeEventTransitionsToSubscribedState)
{
    // When subscribing
    const auto result = state_machine_.SubscribeEvent(3U);

    // Then the subscription succeeds and the state machine transitions to SUBSCRIBED_STATE
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::SUBSCRIBED_STATE);
}

TEST_F(StateMachineMixedCoherentFixture, SubscribeEventCreatesASlotCollector)
{
    // When subscribing
    static_cast<void>(state_machine_.SubscribeEvent(3U));

    // Then a SlotCollector has been created
    EXPECT_TRUE(state_machine_.GetSlotCollectorLockFree().has_value());
}

TEST_F(StateMachineMixedCoherentFixture, UnsubscribeEventTransitionsBackToNotSubscribedState)
{
    // Given a subscribed state machine
    static_cast<void>(state_machine_.SubscribeEvent(3U));

    // When unsubscribing
    state_machine_.UnsubscribeEvent();

    // Then the state machine transitions back to NOT_SUBSCRIBED_STATE and the SlotCollector is destroyed
    EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::NOT_SUBSCRIBED_STATE);
    EXPECT_FALSE(state_machine_.GetSlotCollectorLockFree().has_value());
}

TEST_F(StateMachineMixedCoherentFixture, CanResubscribeAfterUnsubscribe)
{
    // Given a subscribed and then unsubscribed state machine
    static_cast<void>(state_machine_.SubscribeEvent(3U));
    state_machine_.UnsubscribeEvent();

    // When subscribing again
    const auto result = state_machine_.SubscribeEvent(2U);

    // Then the subscription succeeds again (the TransactionLog slot used by the previous subscription was properly
    // unregistered and is free to be reused)
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::SUBSCRIBED_STATE);
}

}  // namespace
}  // namespace score::mw::com::impl::lola
