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
#include "score/mw/com/impl/bindings/lola/proxy_event_common.h"

#include "score/mw/com/impl/bindings/lola/i_runtime.h"
#include "score/mw/com/impl/runtime.h"

#include <limits>
#include <sstream>

namespace score::mw::com::impl::lola
{

ProxyEventCommon::ProxyEventCommon(Proxy& parent, const ElementFqId element_fq_id, const std::string_view event_name)
    : test_slot_collector_{},
      parent_{parent},
      event_fq_id_{element_fq_id},
      event_name_{event_name},
      // The transaction log is identified by the application's unique identifier.
      transaction_log_id_{
          static_cast<TransactionLogId>(GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa).GetApplicationId())},
      event_data_control_local_{parent_.GetConsumerEventDataControlLocalView(event_fq_id_)},
      subscription_control_{parent_.GetEventSubscriptionControl(event_fq_id_)},
      transaction_log_set_{parent_.GetTransactionLogSet(event_fq_id_)},
      subscription_event_state_machine_{parent_.GetQualityType(),
                                        event_fq_id_,
                                        GetEventSourcePid(),
                                        event_data_control_local_,
                                        subscription_control_.get(),
                                        transaction_log_set_.get(),
                                        transaction_log_id_}
{
}

ProxyEventCommon::~ProxyEventCommon()
{
    Unsubscribe();
}

Result<void> ProxyEventCommon::Subscribe(const std::size_t max_sample_count)
{
    std::stringstream sstream{};
    sstream << "Max sample count of" << max_sample_count << "is too large: Lola only supports up to 255 samples.";
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(max_sample_count <= std::numeric_limits<std::uint8_t>::max(),
                                                sstream.str().c_str());
    return subscription_event_state_machine_.SubscribeEvent(max_sample_count);
}

void ProxyEventCommon::Unsubscribe()
{
    subscription_event_state_machine_.UnsubscribeEvent();
}

SubscriptionState ProxyEventCommon::GetSubscriptionState() const noexcept
{
    const auto current_state = subscription_event_state_machine_.GetCurrentState();
    return SubscriptionStateMachineStateToSubscriptionState(current_state);
}

Result<std::size_t> ProxyEventCommon::GetNumNewSamplesAvailable() const noexcept
{
    const auto& slot_collector = test_slot_collector_.has_value()
                                     ? test_slot_collector_
                                     : subscription_event_state_machine_.GetSlotCollectorLockFree();
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
        slot_collector.has_value(),
        "GetNumNewSamplesAvailable must be called after the slot collector is instantiated by calling Subscribe().");
    return slot_collector.value().GetNumNewSamplesAvailable();
}

SlotCollector::SlotIndices ProxyEventCommon::GetNewSamplesSlotIndices(const std::size_t max_count) noexcept
{
    auto& slot_collector = test_slot_collector_.has_value()
                               ? test_slot_collector_
                               : subscription_event_state_machine_.GetSlotCollectorLockFree();
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
        slot_collector.has_value(),
        "GetNewSamplesSlotIndices must be called after the slot collector is instantiated by calling Subscribe().");
    return slot_collector.value().GetNewSamplesSlotIndices(max_count);
}

Result<void> ProxyEventCommon::SetReceiveHandler(std::weak_ptr<ScopedEventReceiveHandler> handler)
{
    subscription_event_state_machine_.SetReceiveHandler(std::move(handler));
    return {};
}

Result<void> ProxyEventCommon::UnsetReceiveHandler()
{
    subscription_event_state_machine_.UnsetReceiveHandler();
    return {};
}

Result<void> ProxyEventCommon::SetSubscriptionStateChangeHandler(SubscriptionStateChangeHandler handler) noexcept
{
    subscription_event_state_machine_.SetSubscriptionStateChangeHandler(std::move(handler));
    return {};
}

Result<void> ProxyEventCommon::UnsetSubscriptionStateChangeHandler() noexcept
{
    subscription_event_state_machine_.UnsetSubscriptionStateChangeHandler();
    return {};
}

pid_t ProxyEventCommon::GetEventSourcePid() const noexcept
{
    return parent_.GetSourcePid();
}

std::optional<std::uint16_t> ProxyEventCommon::GetMaxSampleCount() const noexcept
{
    return subscription_event_state_machine_.GetMaxSampleCount();
}

void ProxyEventCommon::NotifyServiceInstanceChangedAvailability(const bool is_available,
                                                                const pid_t new_event_source_pid) noexcept
{
    if (is_available)
    {
        subscription_event_state_machine_.ReOfferEvent(new_event_source_pid);
    }
    else
    {
        subscription_event_state_machine_.StopOfferEvent();
    }
}

}  // namespace score::mw::com::impl::lola
