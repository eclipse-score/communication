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
#include "score/mw/com/impl/bindings/lola/subscription_not_subscribed_states.h"
#include "score/mw/com/impl/bindings/lola/event_subscription_control.h"
#include "score/mw/com/impl/bindings/lola/subscription_helpers.h"
#include "score/mw/com/impl/bindings/lola/subscription_state_machine.h"
#include "score/mw/com/impl/bindings/lola/subscription_state_machine_states.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_registration_guard.h"
#include "score/mw/com/impl/com_error.h"

#include "score/result/result.h"
#include "score/mw/log/logging.h"

#include <score/utility.hpp>

#include <sstream>
#include <utility>

namespace score::mw::com::impl::lola
{

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from '.value()' in case it doesn't have value but as we check
// before with 'has_value()' so no way for throwing std::bad_optional_access which leds to std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
ResultBlank NotSubscribedState::SubscribeEvent(const std::size_t max_sample_count) noexcept
{
    auto transaction_log_registration_guard_result = TransactionLogRegistrationGuard::Create(
        state_machine_.event_control_.data_control, state_machine_.transaction_log_id_);
    if (!(transaction_log_registration_guard_result.has_value()))
    {
        std::stringstream ss{};
        ss << "Subscribe was rejected by skeleton. Could not Register TransactionLog due to "
           << transaction_log_registration_guard_result.error();
        ::score::mw::log::LogError("lola") << CreateLoggingString(
            ss.str(), state_machine_.GetElementFqId(), state_machine_.GetCurrentStateNoLock());
        return MakeUnexpected(ComErrc::kMaxSubscribersExceeded);
    }
    score::cpp::ignore = state_machine_.transaction_log_registration_guard_.emplace(
        std::move(transaction_log_registration_guard_result).value());

    auto transaction_log_index = state_machine_.transaction_log_registration_guard_->GetTransactionLogIndex();
    auto& transaction_log =
        state_machine_.event_control_.data_control.GetTransactionLogSet().GetTransactionLog(transaction_log_index);
    transaction_log.SubscribeTransactionBegin(max_sample_count);

    // Suppress "AUTOSAR C++14 A4-7-1" rule finding. This rule states: "An integer expression shall
    // not lead to data loss.".
    // This is an in purpose casting, as the max sample count could be hold by uint16.
    // coverity[autosar_cpp14_a4_7_1_violation]
    const auto max_sample_count_uint16 = static_cast<std::uint16_t>(max_sample_count);
    const auto subscription_result =
        state_machine_.event_control_.subscription_control.Subscribe(max_sample_count_uint16);
    if (subscription_result != SubscribeResult::kSuccess)
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(
            subscription_result != SubscribeResult::kMaxSubscribersOverflow,
            "TransactionLogRegistrationGuard::Create will return an error if we have a subscriber overflow.");
        transaction_log.SubscribeTransactionAbort();
        std::stringstream ss{};
        ss << "Subscribe was rejected by skeleton. Cannot complete SubscribeEvent() call due to "
           << ToString(subscription_result);
        ::score::mw::log::LogError("lola") << CreateLoggingString(
            ss.str(), state_machine_.GetElementFqId(), state_machine_.GetCurrentStateNoLock());
        state_machine_.transaction_log_registration_guard_.reset();
        return MakeUnexpected(ComErrc::kMaxSampleCountNotRealizable);
    }
    transaction_log.SubscribeTransactionCommit();

    SlotCollector slot_collector{
        state_machine_.event_control_.data_control, static_cast<std::size_t>(max_sample_count), transaction_log_index};
    if (state_machine_.event_receiver_handler_.has_value())
    {
        state_machine_.event_receive_handler_manager_.Register(
            std::move(state_machine_.event_receiver_handler_.value()));
        state_machine_.event_receiver_handler_.reset();
    }
    score::cpp::ignore = state_machine_.subscription_data_.slot_collector_.emplace(std::move(slot_collector));
    state_machine_.subscription_data_.max_sample_count_ = max_sample_count;

    if (state_machine_.provider_service_instance_is_available_)
    {
        state_machine_.TransitionToState(SubscriptionStateMachineState::SUBSCRIBED_STATE);
    }
    else
    {
        state_machine_.TransitionToState(SubscriptionStateMachineState::SUBSCRIPTION_PENDING_STATE);
    }
    return {};
}

void NotSubscribedState::UnsubscribeEvent() noexcept {}

void NotSubscribedState::StopOfferEvent() noexcept
{
    state_machine_.provider_service_instance_is_available_ = false;
}

void NotSubscribedState::ReOfferEvent(const pid_t new_event_source_pid) noexcept
{
    state_machine_.event_receive_handler_manager_.UpdatePid(new_event_source_pid);
    state_machine_.provider_service_instance_is_available_ = true;
}

void NotSubscribedState::SetReceiveHandler(std::weak_ptr<ScopedEventReceiveHandler> handler) noexcept
{
    state_machine_.event_receiver_handler_ = std::move(handler);
}

void NotSubscribedState::UnsetReceiveHandler() noexcept
{
    state_machine_.event_receiver_handler_ = score::cpp::nullopt;
}

std::optional<std::uint16_t> NotSubscribedState::GetMaxSampleCount() const noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(!state_machine_.subscription_data_.max_sample_count_.has_value(),
                       "Max sample count should not be set until Subscribe is called.");
    return {};
}

score::cpp::optional<SlotCollector>& NotSubscribedState::GetSlotCollector() noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(!state_machine_.subscription_data_.slot_collector_.has_value(),
                       "Slot collector should not be created until Subscribe is called.");
    return state_machine_.subscription_data_.slot_collector_;
}

const score::cpp::optional<SlotCollector>& NotSubscribedState::GetSlotCollector() const noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(!state_machine_.subscription_data_.slot_collector_.has_value(),
                       "Slot collector should not be created until Subscribe is called.");
    return state_machine_.subscription_data_.slot_collector_;
}

score::cpp::optional<TransactionLogSet::TransactionLogIndex> NotSubscribedState::GetTransactionLogIndex() const noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(!state_machine_.transaction_log_registration_guard_.has_value(),
                       "TransactionLogRegistrationGuard should not be set until Subscribe is called.");
    return {};
}

void NotSubscribedState::OnEntry() noexcept
{
    const auto transaction_log_index = state_machine_.transaction_log_registration_guard_->GetTransactionLogIndex();
    auto& transaction_log =
        state_machine_.event_control_.data_control.GetTransactionLogSet().GetTransactionLog(transaction_log_index);

    transaction_log.UnsubscribeTransactionBegin();
    state_machine_.event_control_.subscription_control.Unsubscribe(
        state_machine_.subscription_data_.max_sample_count_.value());
    transaction_log.UnsubscribeTransactionCommit();

    state_machine_.event_receive_handler_manager_.Unregister();
    state_machine_.subscription_data_.Clear();
    state_machine_.transaction_log_registration_guard_.reset();
}

}  // namespace score::mw::com::impl::lola
