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

namespace
{
lola::IRuntime& GetBindingRuntime() noexcept
{
    auto* binding =
        dynamic_cast<lola::IRuntime*>(mw::com::impl::Runtime::getInstance().GetBindingRuntime(BindingType::kLoLa));
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(binding != nullptr, "Binding is null");

    return *binding;
}
}  // namespace

ProxyEventCommon::ProxyEventCommon(Proxy& parent, const ElementFqId element_fq_id, const std::string_view event_name)
    : test_slot_collector_{},
      parent_{parent},
      event_fq_id_{element_fq_id},
      event_name_{event_name},
      transaction_log_id_{GetBindingRuntime().GetUid()},
      event_control_{parent_.GetEventControl(event_fq_id_)},
      subscription_event_state_machine_{parent_.GetQualityType(),
                                        event_fq_id_,
                                        GetEventSourcePid(),
                                        event_control_,
                                        transaction_log_id_}
{
}

ProxyEventCommon::~ProxyEventCommon()
{
    Unsubscribe();
}

ResultBlank ProxyEventCommon::Subscribe(const std::size_t max_sample_count)
{
    std::stringstream sstream{};
    sstream << "Max sample count of" << max_sample_count << "is too large: Lola only supports up to 255 samples.";
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(max_sample_count <= std::numeric_limits<std::uint8_t>::max(), sstream.str().c_str());
    return subscription_event_state_machine_.SubscribeEvent(max_sample_count);
}

void ProxyEventCommon::Unsubscribe()
{
    subscription_event_state_machine_.UnsubscribeEvent();
}

SubscriptionState ProxyEventCommon::GetSubscriptionState() const noexcept
{
    const auto current_state = subscription_event_state_machine_.GetCurrentState();
    if (current_state == SubscriptionStateMachineState::NOT_SUBSCRIBED_STATE)
    {
        return SubscriptionState::kNotSubscribed;
    }
    else if (current_state == SubscriptionStateMachineState::SUBSCRIPTION_PENDING_STATE)
    {
        return SubscriptionState::kSubscriptionPending;
    }
    else
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(current_state == SubscriptionStateMachineState::SUBSCRIBED_STATE,
                               "Invalid subscription state machine state.");
        return SubscriptionState::kSubscribed;
    }
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

SlotCollector::SlotIndicators ProxyEventCommon::GetNewSamplesSlotIndices(const std::size_t max_count) noexcept
{
    auto& slot_collector = test_slot_collector_.has_value()
                               ? test_slot_collector_
                               : subscription_event_state_machine_.GetSlotCollectorLockFree();
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
        slot_collector.has_value(),
        "GetNewSamplesSlotIndices must be called after the slot collector is instantiated by calling Subscribe().");
    return slot_collector.value().GetNewSamplesSlotIndices(max_count);
}

ResultBlank ProxyEventCommon::SetReceiveHandler(std::weak_ptr<ScopedEventReceiveHandler> handler)
{
    subscription_event_state_machine_.SetReceiveHandler(std::move(handler));
    return {};
}

ResultBlank ProxyEventCommon::UnsetReceiveHandler()
{
    subscription_event_state_machine_.UnsetReceiveHandler();
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

score::cpp::optional<TransactionLogSet::TransactionLogIndex> ProxyEventCommon::GetTransactionLogIndex() const noexcept
{
    return subscription_event_state_machine_.GetTransactionLogIndex();
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
