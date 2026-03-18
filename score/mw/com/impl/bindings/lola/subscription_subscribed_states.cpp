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
#include "score/mw/com/impl/bindings/lola/subscription_subscribed_states.h"
#include "score/mw/com/impl/bindings/lola/subscription_helpers.h"
#include "score/mw/com/impl/bindings/lola/subscription_state_machine.h"
#include "score/mw/com/impl/bindings/lola/subscription_state_machine_states.h"
#include "score/mw/com/impl/com_error.h"

#include "score/mw/log/logging.h"

#include <score/assert.hpp>

#include <exception>
#include <utility>

namespace score::mw::com::impl::lola
{

ResultBlank SubscribedState::SubscribeEvent(const std::size_t max_sample_count) noexcept
{
    // Suppress "AUTOSAR C++14 A4-7-1" rule finding. This rule states: "An integer expression shall
    // not lead to data loss.".
    // This is an in purpose casting and below we do check on the max_sample_count and an error reported in case of
    // different max_sample_count.
    // coverity[autosar_cpp14_a4_7_1_violation]
    const auto max_sample_count_uint16 = static_cast<std::uint16_t>(max_sample_count);
    if (state_machine_.subscription_data_.max_sample_count_.value() == max_sample_count_uint16)
    {
        ::score::mw::log::LogWarn("lola")
            << CreateLoggingString("Calling SubscribeEvent() while already subscribed has no effect.",
                                   state_machine_.GetElementFqId(),
                                   state_machine_.GetCurrentStateNoLock());
        return {};
    }
    else
    {
        ::score::mw::log::LogError("lola") << CreateLoggingString(
            "Calling SubscribeEvent() while already subscribed with a different max_sample_count is illegal.",
            state_machine_.GetElementFqId(),
            state_machine_.GetCurrentStateNoLock());
        return MakeUnexpected(ComErrc::kMaxSampleCountNotRealizable);
    }
}

void SubscribedState::UnsubscribeEvent() noexcept
{
    // Unsubscribe functionality will be done in NotSubscribedState::OnEntry() which will be called synchronously by
    // TransitionToState. We do this to avoid code duplication between SubscriptionPendingState::UnsubscribeEvent() and
    // SubscribedState::UnsubscribeEvent()
    state_machine_.TransitionToState(SubscriptionStateMachineState::NOT_SUBSCRIBED_STATE);
}

void SubscribedState::StopOfferEvent() noexcept
{
    state_machine_.provider_service_instance_is_available_ = false;
    state_machine_.TransitionToState(SubscriptionStateMachineState::SUBSCRIPTION_PENDING_STATE);
}

void SubscribedState::ReOfferEvent(const pid_t) noexcept
{
    ::score::mw::log::LogWarn("lola") << CreateLoggingString("Service cannot be re-offered while already subscribed.",
                                                             state_machine_.GetElementFqId(),
                                                             state_machine_.GetCurrentStateNoLock());
}

void SubscribedState::SetReceiveHandler(std::weak_ptr<ScopedEventReceiveHandler> handler) noexcept
{
    state_machine_.event_receive_handler_manager_.Register(std::move(handler));
}

void SubscribedState::UnsetReceiveHandler() noexcept
{
    state_machine_.event_receive_handler_manager_.Unregister();
}

std::optional<std::uint16_t> SubscribedState::GetMaxSampleCount() const noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(
        state_machine_.subscription_data_.max_sample_count_.value() > 0U,
        "The subscription dispatch manager and the contained max sample count should be initialised on subscription.");
    return state_machine_.subscription_data_.max_sample_count_.value();
}

score::cpp::optional<SlotCollector>& SubscribedState::GetSlotCollector() noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(
        state_machine_.subscription_data_.max_sample_count_.has_value(),
        "The subscription data and the contained slot collector should be initialised on subscription.");
    return state_machine_.subscription_data_.slot_collector_;
}

const score::cpp::optional<SlotCollector>& SubscribedState::GetSlotCollector() const noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(
        state_machine_.subscription_data_.max_sample_count_.has_value(),
        "The subscription data and the contained slot collector should be initialised on subscription.");
    return state_machine_.subscription_data_.slot_collector_;
}

score::cpp::optional<TransactionLogSet::TransactionLogIndex> SubscribedState::GetTransactionLogIndex() const noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(state_machine_.transaction_log_registration_guard_.has_value(),
                                            "TransactionLogRegistrationGuard should be initialised on subscription.");
    return state_machine_.transaction_log_registration_guard_.value().GetTransactionLogIndex();
}

}  // namespace score::mw::com::impl::lola
