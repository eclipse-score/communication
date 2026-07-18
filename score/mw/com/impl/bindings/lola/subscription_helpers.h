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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_SUBSCRIPTION_HELPERS_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_SUBSCRIPTION_HELPERS_H

#include "score/mw/com/impl/bindings/lola/messaging/i_message_passing_service.h"
#include "score/mw/com/impl/bindings/lola/slot_collector.h"
#include "score/mw/com/impl/bindings/lola/subscription_state_machine_states.h"
#include "score/mw/com/impl/scoped_event_receive_handler.h"
#include "score/mw/com/impl/subscription_state.h"

#include <score/callback.hpp>

#include <score/assert.hpp>

#include <optional>
#include <string>

namespace score::mw::com::impl::lola
{

/**
 * Helper class to manage registering and deregistering Event Receive Handlers with the MessagePassingServiceInstance so
 * that the caller doesn't have to manually manage the registration number.
 *
 * Since only one Event Receive Handler can be registered at once, Register() will first Unregister any existing Event
 * Receive Handlers. Unregister() will unregister the most recently registered Event Receive Handler (registered with
 * the Register() call.
 */
class EventReceiveHandlerManager
{
  public:
    EventReceiveHandlerManager(const QualityType asil_level,
                               const ElementFqId element_fq_id,
                               const pid_t event_source_pid) noexcept
        : asil_level_{asil_level},
          element_fq_id_{element_fq_id},
          event_source_pid_{event_source_pid},
          registration_number_{}
    {
    }

    void Register(std::weak_ptr<ScopedEventReceiveHandler> handler);
    void Reregister(std::optional<std::weak_ptr<ScopedEventReceiveHandler>> new_event_receiver_handler);
    void Unregister() noexcept;

    void UpdatePid(pid_t new_event_source_pid) noexcept
    {
        event_source_pid_ = new_event_source_pid;
    }

  private:
    const QualityType asil_level_;
    const ElementFqId element_fq_id_;
    pid_t event_source_pid_;
    std::optional<IMessagePassingService::HandlerRegistrationNoType> registration_number_;
};

class SubscriptionData
{
  public:
    SubscriptionData() : max_sample_count_{}, slot_collector_{} {}

    void Clear()
    {
        max_sample_count_.reset();
        slot_collector_.reset();
    }

    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private.". There are no class invariants to maintain which could be violated by directly accessing member
    // variables.
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::optional<std::uint16_t> max_sample_count_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::optional<SlotCollector> slot_collector_;
};

std::string CreateLoggingString(std::string&& string,
                                const ElementFqId& element_fq_id,
                                const SubscriptionStateMachineState current_state);

// Hot-path optimization: defined inline so that ProxyEventCommon::GetSubscriptionState() (called on every
// GetNewSamples()/GetNumNewSamplesAvailable() invocation) can be fully inlined without an out-of-line call.
inline SubscriptionState SubscriptionStateMachineStateToSubscriptionState(
    const SubscriptionStateMachineState state) noexcept
{
    // Suppress "AUTOSAR C++14 M6-4-5" and "AUTOSAR C++14 M6-4-3", The rule states: "An unconditional throw or break
    // statement shall terminate every nonempty switch-clause." and "A switch statement shall be a well-formed
    // switch statement." respectively. There is return for every switch case, adding a break would be dead code.
    // coverity[autosar_cpp14_m6_4_3_violation]
    switch (state)
    {
        // coverity[autosar_cpp14_m6_4_5_violation]
        case SubscriptionStateMachineState::NOT_SUBSCRIBED_STATE:
            return SubscriptionState::kNotSubscribed;
        // coverity[autosar_cpp14_m6_4_5_violation]
        case SubscriptionStateMachineState::SUBSCRIBED_STATE:
            return SubscriptionState::kSubscribed;
        // coverity[autosar_cpp14_m6_4_5_violation]
        case SubscriptionStateMachineState::SUBSCRIPTION_PENDING_STATE:
            return SubscriptionState::kSubscriptionPending;
        // coverity[autosar_cpp14_m6_4_5_violation]
        case SubscriptionStateMachineState::STATE_COUNT:
        default:
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(false, "Invalid subscription state");
    }
}

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SUBSCRIPTION_HELPERS_H
