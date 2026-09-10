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
#include "score/mw/com/impl/bindings/lola/subscription_subscription_pending_states.h"
#include "score/mw/com/impl/bindings/lola/subscription_helpers.h"
#include "score/mw/com/impl/bindings/lola/subscription_state_machine.h"
#include "score/mw/com/impl/bindings/lola/subscription_state_machine_states.h"
#include "score/mw/com/impl/com_error.h"

#include "score/mw/log/logging.h"

#include <score/assert.hpp>

#include <exception>
#include <memory>
#include <optional>
#include <utility>

namespace score::mw::com::impl::lola
{

Result<void> SubscriptionPendingState::SubscribeEvent(const std::size_t max_sample_count)
{
    // Compare max_sample_count (std::size_t) directly against the stored max_sample_count_ (std::uint16_t),
    // widening the latter to std::size_t for the comparison. This is always lossless (every std::uint16_t
    // value fits in std::size_t) and avoids a signedness-changing promotion, since std::size_t already has
    // rank >= int. Note: this intentionally no longer truncates max_sample_count down to std::uint8_t first --
    // that previous truncation silently corrupted the comparison for any max_sample_count above 255 (the
    // stored max_sample_count_ can legitimately be up to 65535, per its std::uint16_t type), which would
    // have caused SubscribeEvent() to spuriously report "different max_sample_count" for a legitimate
    // resubscription with the same, larger count.
    if (max_sample_count == static_cast<std::size_t>(state_machine_.subscription_data_.max_sample_count_.value()))
    {
        ::score::mw::log::LogWarn("lola")
            << CreateLoggingString("Calling SubscribeEvent() while subscription is pending has no effect.",
                                   state_machine_.GetElementFqId(),
                                   state_machine_.GetCurrentStateNoLock());
        return {};
    }
    else
    {
        ::score::mw::log::LogError("lola") << CreateLoggingString(
            "Calling SubscribeEvent() with a different max_sample_count while subscription is pending is illegal.",
            state_machine_.GetElementFqId(),
            state_machine_.GetCurrentStateNoLock());
        return MakeUnexpected(ComErrc::kMaxSampleCountNotRealizable);
    }
}

void SubscriptionPendingState::UnsubscribeEvent()
{
    // Unsubscribe functionality will be done in NotSubscribedState::OnEntry() which will be called synchronously by
    // TransitionToState. We do this to avoid code duplication between SubscriptionPendingState::UnsubscribeEvent() and
    // SubscribedState::UnsubscribeEvent()
    state_machine_.TransitionToState(SubscriptionStateMachineState::NOT_SUBSCRIBED_STATE);
}

void SubscriptionPendingState::StopOfferEvent() noexcept
{
    ::score::mw::log::LogFatal("lola") << CreateLoggingString(
        "Service cannot be stop-offered while in subscription pending. Terminating",
        state_machine_.GetElementFqId(),
        state_machine_.GetCurrentStateNoLock());
    std::terminate();
}

void SubscriptionPendingState::ReOfferEvent(const pid_t new_event_source_pid)
{
    state_machine_.provider_service_instance_is_available_ = true;
    state_machine_.event_receive_handler_manager_.UpdatePid(new_event_source_pid);
    state_machine_.event_receive_handler_manager_.Reregister(state_machine_.event_receiver_handler_);
    state_machine_.event_receiver_handler_.reset();
    state_machine_.TransitionToState(SubscriptionStateMachineState::SUBSCRIBED_STATE);
}

void SubscriptionPendingState::SetReceiveHandler(std::weak_ptr<ScopedEventReceiveHandler> handler) noexcept
{
    state_machine_.event_receiver_handler_ = std::move(handler);
}

void SubscriptionPendingState::UnsetReceiveHandler()
{
    state_machine_.event_receiver_handler_ = std::nullopt;
}

std::optional<std::uint16_t> SubscriptionPendingState::GetMaxSampleCount() const
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(
        state_machine_.subscription_data_.max_sample_count_.has_value(),
        "The subscription data and the contained max sample count should be initialised on subscription.");
    return state_machine_.subscription_data_.max_sample_count_.value();
}

std::optional<SlotCollector>& SubscriptionPendingState::GetSlotCollector() & noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(
        state_machine_.subscription_data_.max_sample_count_.has_value(),
        "The subscription data and the contained slot collector should be initialised on subscription.");
    return state_machine_.subscription_data_.slot_collector_;
}

const std::optional<SlotCollector>& SubscriptionPendingState::GetSlotCollector() const& noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(
        state_machine_.subscription_data_.max_sample_count_.has_value(),
        "The subscription data and the contained slot collector should be initialised on subscription.");
    return state_machine_.subscription_data_.slot_collector_;
}

}  // namespace score::mw::com::impl::lola
