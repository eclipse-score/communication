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

namespace score::mw::com::impl::lola
{

std::string MessageForSubscriptionState(const SubscriptionStateMachineState& state) noexcept
{
    using State = SubscriptionStateMachineState;
    // Suppress "AUTOSAR C++14 M6-4-3" rule finding. This rule declares: "A switch statement shall be
    // a well-formed switch statement".
    // We don't need a break statement at the end of default case as we use return.
    // coverity[autosar_cpp14_m6_4_3_violation]
    switch (state)
    {
        // coverity[autosar_cpp14_m6_4_5_violation] Return will terminate this switch clause.
        case State::NOT_SUBSCRIBED_STATE:
            return "Not Subscribed State.";
        // coverity[autosar_cpp14_m6_4_5_violation] Return will terminate this switch clause.
        case State::SUBSCRIPTION_PENDING_STATE:
            return "Subscription Pending State.";
        // coverity[autosar_cpp14_m6_4_5_violation] Return will terminate this switch clause.
        case State::SUBSCRIBED_STATE:
            return "Subscribed State.";
        // coverity[autosar_cpp14_m6_4_5_violation] Return will terminate this switch clause.
        case State::STATE_COUNT:
            return "Invalid state: State Count is not a valid state.";
        // coverity[autosar_cpp14_m6_4_5_violation] Return will terminate this switch clause.
        default:
            return "Unknown subscription state.";
    }
}

}  // namespace score::mw::com::impl::lola
