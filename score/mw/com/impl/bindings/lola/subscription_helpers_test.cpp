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
#include "score/mw/com/impl/bindings/lola/subscription_helpers.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl::lola
{

TEST(SubscriptionHelpersTest, MachineStateToSubscriptionState_NotSubscribed)
{
    EXPECT_EQ(SubscriptionStateMachineStateToSubscriptionState(SubscriptionStateMachineState::NOT_SUBSCRIBED_STATE),
              SubscriptionState::kNotSubscribed);
}

TEST(SubscriptionHelpersTest, MachineStateToSubscriptionState_Subscribed)
{
    EXPECT_EQ(SubscriptionStateMachineStateToSubscriptionState(SubscriptionStateMachineState::SUBSCRIBED_STATE),
              SubscriptionState::kSubscribed);
}

TEST(SubscriptionHelpersTest, MachineStateToSubscriptionState_SubscriptionPending)
{
    EXPECT_EQ(
        SubscriptionStateMachineStateToSubscriptionState(SubscriptionStateMachineState::SUBSCRIPTION_PENDING_STATE),
        SubscriptionState::kSubscriptionPending);
}

TEST(SubscriptionHelpersDeathTest, MachineStateToSubscriptionState_Other)
{
    ASSERT_DEATH(SubscriptionStateMachineStateToSubscriptionState(SubscriptionStateMachineState::STATE_COUNT), "");
}

}  // namespace score::mw::com::impl::lola
