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

#include "score/mw/com/impl/bindings/lola/proxy.h"
#include "score/mw/com/impl/bindings/lola/slot_collector.h"

#include "score/mw/com/impl/bindings/lola/subscription_not_subscribed_states.h"
#include "score/mw/com/impl/bindings/lola/subscription_subscribed_states.h"
#include "score/mw/com/impl/bindings/lola/subscription_subscription_pending_states.h"

#include <utility>

namespace score::mw::com::impl::lola
{

SubscriptionStateMachine::SubscriptionStateMachine(const QualityType quality_type,
                                                   const ElementFqId element_fq_id,
                                                   const pid_t event_source_pid,
                                                   EventControl& event_control,
                                                   const TransactionLogId& transaction_log_id) noexcept
    : std::enable_shared_from_this<SubscriptionStateMachine>{},
      state_mutex_{},
      states_{std::make_unique<NotSubscribedState>(*this),
              std::make_unique<SubscriptionPendingState>(*this),
              std::make_unique<SubscribedState>(*this)},
      current_state_idx_{SubscriptionStateMachineState::NOT_SUBSCRIBED_STATE},
      subscription_data_{},
      event_receiver_handler_{},
      event_receive_handler_manager_{quality_type, element_fq_id, event_source_pid},
      event_control_{event_control},
      provider_service_instance_is_available_{true},
      transaction_log_id_{transaction_log_id},
      transaction_log_registration_guard_{},
      element_fq_id_{element_fq_id}
{
}

SubscriptionStateMachine::~SubscriptionStateMachine() noexcept = default;

SubscriptionStateMachineState SubscriptionStateMachine::GetCurrentState() const noexcept
{
    std::lock_guard<std::mutex> lock{state_mutex_};
    return GetCurrentStateNoLock();
}

SubscriptionStateMachineState SubscriptionStateMachine::GetCurrentStateNoLock() const noexcept
{
    return current_state_idx_;
}

SubscriptionStateBase& SubscriptionStateMachine::GetCurrentEventState() noexcept
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index): current_state_idx_ can't be const expression
    return *states_[static_cast<std::uint8_t>(current_state_idx_)];
}

const SubscriptionStateBase& SubscriptionStateMachine::GetCurrentEventState() const noexcept
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index): current_state_idx_ can't be const expression
    return *states_[static_cast<std::uint8_t>(current_state_idx_)];
}

ResultBlank SubscriptionStateMachine::SubscribeEvent(const std::size_t max_sample_count) noexcept
{
    std::lock_guard<std::mutex> lock{state_mutex_};
    return GetCurrentEventState().SubscribeEvent(max_sample_count);
}

void SubscriptionStateMachine::UnsubscribeEvent() noexcept
{
    std::lock_guard<std::mutex> lock{state_mutex_};
    GetCurrentEventState().UnsubscribeEvent();
}

void SubscriptionStateMachine::StopOfferEvent() noexcept
{
    std::lock_guard<std::mutex> lock{state_mutex_};
    GetCurrentEventState().StopOfferEvent();
}

void SubscriptionStateMachine::ReOfferEvent(const pid_t new_event_source_pid) noexcept
{
    std::lock_guard<std::mutex> lock{state_mutex_};
    GetCurrentEventState().ReOfferEvent(new_event_source_pid);
}

void SubscriptionStateMachine::SetReceiveHandler(std::weak_ptr<ScopedEventReceiveHandler> handler) noexcept
{
    std::lock_guard<std::mutex> lock{state_mutex_};
    GetCurrentEventState().SetReceiveHandler(std::move(handler));
}

void SubscriptionStateMachine::UnsetReceiveHandler() noexcept
{
    std::lock_guard<std::mutex> lock{state_mutex_};
    GetCurrentEventState().UnsetReceiveHandler();
}

std::optional<std::uint16_t> SubscriptionStateMachine::GetMaxSampleCount() const noexcept
{
    std::lock_guard<std::mutex> lock{state_mutex_};
    return GetCurrentEventState().GetMaxSampleCount();
}

score::cpp::optional<SlotCollector>& SubscriptionStateMachine::GetSlotCollectorLockFree() noexcept
{
    return GetCurrentEventState().GetSlotCollector();
}

const score::cpp::optional<SlotCollector>& SubscriptionStateMachine::GetSlotCollectorLockFree() const noexcept
{
    return GetCurrentEventState().GetSlotCollector();
}

score::cpp::optional<TransactionLogSet::TransactionLogIndex> SubscriptionStateMachine::GetTransactionLogIndex()
    const noexcept
{
    std::lock_guard<std::mutex> lock{state_mutex_};
    return GetCurrentEventState().GetTransactionLogIndex();
}

const ElementFqId& SubscriptionStateMachine::GetElementFqId() const noexcept
{
    return element_fq_id_;
}

void SubscriptionStateMachine::TransitionToState(const SubscriptionStateMachineState newState) noexcept
{
    GetCurrentEventState().OnExit();
    current_state_idx_ = newState;
    GetCurrentEventState().OnEntry();
}

}  // namespace score::mw::com::impl::lola
