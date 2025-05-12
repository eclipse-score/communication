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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_SUBSCRIPTION_SUBSCRIPTION_STATE_BASE_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_SUBSCRIPTION_SUBSCRIPTION_STATE_BASE_H

#include "score/mw/com/impl/bindings/lola/messaging/i_message_passing_service.h"
#include "score/mw/com/impl/bindings/lola/slot_collector.h"
#include "score/mw/com/impl/bindings/lola/subscription_state_machine_states.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_set.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/proxy_event_binding.h"
#include "score/mw/com/impl/scoped_event_receive_handler.h"

#include "score/result/result.h"

#include <score/optional.hpp>

#include <cstddef>
#include <functional>
#include <mutex>

namespace score::mw::com::impl::lola
{
// Suppress "AUTOSAR C++14 M3-2-3", The rule states: "A type, object or function that is used in multiple translation
// units shall be declared in one and only one file."
// This is a forward class declaration.
// coverity[autosar_cpp14_m3_2_3_violation]
class SubscriptionStateMachine;

class SubscriptionStateBase
{
  public:
    explicit SubscriptionStateBase(SubscriptionStateMachine& subscription_event_state_machine) noexcept;
    virtual ~SubscriptionStateBase() noexcept = default;

    SubscriptionStateBase(const SubscriptionStateBase&) = delete;
    SubscriptionStateBase& operator=(const SubscriptionStateBase&) & = delete;
    SubscriptionStateBase(SubscriptionStateBase&&) noexcept = delete;
    SubscriptionStateBase& operator=(SubscriptionStateBase&&) & noexcept = delete;

    virtual ResultBlank SubscribeEvent(const std::size_t max_sample_count) noexcept = 0;
    virtual void UnsubscribeEvent() noexcept = 0;
    virtual void StopOfferEvent() noexcept = 0;
    virtual void ReOfferEvent(const pid_t new_event_source_pid) noexcept = 0;

    virtual void SetReceiveHandler(std::weak_ptr<ScopedEventReceiveHandler> handler) noexcept = 0;
    virtual void UnsetReceiveHandler() noexcept = 0;
    virtual std::optional<std::uint16_t> GetMaxSampleCount() const noexcept = 0;
    virtual score::cpp::optional<SlotCollector>& GetSlotCollector() noexcept = 0;
    virtual const score::cpp::optional<SlotCollector>& GetSlotCollector() const noexcept = 0;
    virtual score::cpp::optional<TransactionLogSet::TransactionLogIndex> GetTransactionLogIndex() const noexcept = 0;

    virtual void OnEntry() noexcept {};
    virtual void OnExit() noexcept {};

  protected:
    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private.". There are no class invariants to maintain so state_machine_ can be safely accessed directly
    // by child classes.
    // coverity[autosar_cpp14_m11_0_1_violation]
    SubscriptionStateMachine& state_machine_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SUBSCRIPTION_SUBSCRIPTION_STATE_BASE_H
