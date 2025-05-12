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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_SUBSCRIPTION_SUBSCRIPTION_PENDING_STATES_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_SUBSCRIPTION_SUBSCRIPTION_PENDING_STATES_H

#include "score/mw/com/impl/bindings/lola/messaging/i_message_passing_service.h"
#include "score/mw/com/impl/bindings/lola/subscription_state_base.h"
#include "score/mw/com/impl/scoped_event_receive_handler.h"

#include <score/optional.hpp>

#include <cstddef>
#include <mutex>

namespace score::mw::com::impl::lola
{

class SubscriptionStateMachine;

class SubscriptionPendingState final : public SubscriptionStateBase
{
  public:
    // Inherit parent class constructor
    using SubscriptionStateBase::SubscriptionStateBase;

    SubscriptionPendingState(const SubscriptionPendingState&) = delete;
    SubscriptionPendingState& operator=(const SubscriptionPendingState&) & = delete;
    SubscriptionPendingState(SubscriptionPendingState&&) = delete;
    SubscriptionPendingState& operator=(SubscriptionPendingState&&) & = delete;

    ~SubscriptionPendingState() noexcept override = default;

    ResultBlank SubscribeEvent(const std::size_t max_sample_count) noexcept override;
    void UnsubscribeEvent() noexcept override;
    void StopOfferEvent() noexcept override;
    void ReOfferEvent(const pid_t new_event_source_pid) noexcept override;

    void SetReceiveHandler(std::weak_ptr<ScopedEventReceiveHandler> handler) noexcept override;
    void UnsetReceiveHandler() noexcept override;
    std::optional<std::uint16_t> GetMaxSampleCount() const noexcept override;
    score::cpp::optional<SlotCollector>& GetSlotCollector() noexcept override;
    const score::cpp::optional<SlotCollector>& GetSlotCollector() const noexcept override;
    score::cpp::optional<TransactionLogSet::TransactionLogIndex> GetTransactionLogIndex() const noexcept override;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SUBSCRIPTION_SUBSCRIPTION_PENDING_STATES_H
