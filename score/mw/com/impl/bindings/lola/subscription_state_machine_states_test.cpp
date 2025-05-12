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
#include "score/mw/com/impl/bindings/lola/subscription_state_machine_states.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl::lola
{

TEST(SubscriptionStatesTest, MessageForNotSubscribedState)
{
    EXPECT_EQ(MessageForSubscriptionState(SubscriptionStateMachineState::NOT_SUBSCRIBED_STATE),
              "Not Subscribed State.");
}

TEST(SubscriptionStatesTest, MessageForSubscriptionPendingState)
{
    EXPECT_EQ(MessageForSubscriptionState(SubscriptionStateMachineState::SUBSCRIPTION_PENDING_STATE),
              "Subscription Pending State.");
}

TEST(SubscriptionStatesTest, MessageForSubscribedState)
{
    EXPECT_EQ(MessageForSubscriptionState(SubscriptionStateMachineState::SUBSCRIBED_STATE), "Subscribed State.");
}

TEST(SubscriptionStatesTest, MessageForStateCountInvalidState)
{
    EXPECT_EQ(MessageForSubscriptionState(SubscriptionStateMachineState::STATE_COUNT),
              "Invalid state: State Count is not a valid state.");
}

TEST(SubscriptionStatesTest, MessageForNotDefaultInvalidState)
{
    EXPECT_EQ(MessageForSubscriptionState(static_cast<SubscriptionStateMachineState>(100)),
              "Unknown subscription state.");
}

}  // namespace score::mw::com::impl::lola
