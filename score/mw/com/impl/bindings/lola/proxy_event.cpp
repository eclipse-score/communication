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
#include "score/mw/com/impl/bindings/lola/proxy_event.h"

#include "score/mw/com/impl/tracing/i_tracing_runtime.h"

#include <score/assert.hpp>

#include <limits>
#include <sstream>

namespace score::mw::com::impl::lola
{

ProxyEvent::ProxyEvent(Proxy& parent, const ElementFqId element_fq_id, const std::string_view event_name)
    : GenericProxyEventBinding{},
      meta_info_{parent.GetEventMetaInfo(element_fq_id)},
      event_data_storage_{parent.GetEventDataStorage(element_fq_id)},
      test_slot_collector_{},
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
                                        parent_.GetSourcePid(),
                                        event_data_control_local_,
                                        subscription_control_.get(),
                                        transaction_log_set_.get(),
                                        transaction_log_id_}
{
    parent.RegisterEvent(event_name, *this);
}

Result<std::size_t> ProxyEvent::GetNumNewSamplesAvailable() const
{
    /// In case of LoLa binding we can also dispatch to GetNumNewSamplesAvailableImpl() in case of kSubscriptionPending!
    /// Because a pre-condition to kSubscriptionPending is that we once had a successful subscription... and then we can
    /// always access the samples even if the provider went down.
    const auto subscription_state = GetSubscriptionState();
    if (subscription_state == SubscriptionState::kNotSubscribed)
    {
        return MakeUnexpected(ComErrc::kNotSubscribed,
                              "Attempt to call GetNumNewSamplesAvailable without successful subscription.");
    }
    const auto& slot_collector = test_slot_collector_.has_value()
                                     ? test_slot_collector_
                                     : subscription_event_state_machine_.GetSlotCollectorLockFree();
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
        slot_collector.has_value(),
        "GetNumNewSamplesAvailable must be called after the slot collector is instantiated by calling Subscribe().");
    return slot_collector.value().GetNumNewSamplesAvailable();
}

Result<void> ProxyEvent::Subscribe(const std::size_t max_sample_count) noexcept
{
    std::stringstream sstream{};
    sstream << "Max sample count of" << max_sample_count << "is too large: Lola only supports up to 255 samples.";
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(max_sample_count <= std::numeric_limits<std::uint8_t>::max(),
                                                sstream.str().c_str());
    return subscription_event_state_machine_.SubscribeEvent(max_sample_count);
}

void ProxyEvent::Unsubscribe() noexcept
{
    subscription_event_state_machine_.UnsubscribeEvent();
}

SubscriptionState ProxyEvent::GetSubscriptionState() const noexcept
{
    const auto current_state = subscription_event_state_machine_.GetCurrentState();
    return SubscriptionStateMachineStateToSubscriptionState(current_state);
}

inline Result<std::size_t> ProxyEvent::GetNewSamples(Callback&& receiver, TrackerGuardFactory& tracker) noexcept
{
    /// In case of LoLa binding we can also dispatch to GetNewSamplesImpl() in case of kSubscriptionPending!
    /// Because a pre-condition to kSubscriptionPending is that we once had a successful subscription... and then we can
    /// always access the samples even if the provider went down.
    const auto subscription_state = GetSubscriptionState();
    if (subscription_state == SubscriptionState::kNotSubscribed)
    {
        return MakeUnexpected(ComErrc::kNotSubscribed,
                              "Attempt to call GetNewSamples without successful subscription.");
    }

    const auto max_sample_count = tracker.GetNumAvailableGuards();
    const auto slot_indices = GetNewSamplesSlotIndices(max_sample_count);

    for (auto slot_index_it = slot_indices.begin; slot_index_it != slot_indices.end; ++slot_index_it)
    {
        const void* type_erased_sample_ptr =
            event_data_storage_.GetTypeErasedDataSlot(*slot_index_it, meta_info_.data_type_info_.Size());

        const EventSlotStatus event_slot_status{event_data_control_local_[*slot_index_it]};
        const EventSlotStatus::EventTimeStamp sample_timestamp{event_slot_status.GetTimeStamp()};

        SamplePtr sample{type_erased_sample_ptr, event_data_control_local_, *slot_index_it};

        auto guard = std::move(*tracker.TakeGuard());
        auto sample_binding_independent = this->MakeSamplePtr(std::move(sample), std::move(guard));

        static_assert(
            sizeof(EventSlotStatus::EventTimeStamp) == sizeof(impl::tracing::ITracingRuntime::TracePointDataId),
            "Event timestamp is used for the trace point data id, therefore, the types should be the same.");
        // Suppress "AUTOSAR C++14 A15-4-2" rule finding. This rule states: "I a function is declared to be
        // noexcept, noexcept(true) or noexcept(<true condition>), then it shall not exit with an exception"
        // we can't add noexcept to score::cpp::callback signature.
        // coverity[autosar_cpp14_a15_4_2_violation]
        receiver(std::move(sample_binding_independent),
                 static_cast<impl::tracing::ITracingRuntime::TracePointDataId>(sample_timestamp));
    }

    const auto num_collected_slots = static_cast<std::size_t>(std::distance(slot_indices.begin, slot_indices.end));
    return num_collected_slots;
}

SlotCollector::SlotIndices ProxyEvent::GetNewSamplesSlotIndices(const std::size_t max_count)
{
    auto& slot_collector = test_slot_collector_.has_value()
                               ? test_slot_collector_
                               : subscription_event_state_machine_.GetSlotCollectorLockFree();
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
        slot_collector.has_value(),
        "GetNewSamplesSlotIndices must be called after the slot collector is instantiated by calling Subscribe().");
    return slot_collector.value().GetNewSamplesSlotIndices(max_count);
}

Result<void> ProxyEvent::SetReceiveHandler(std::weak_ptr<ScopedEventReceiveHandler> handler) noexcept
{
    subscription_event_state_machine_.SetReceiveHandler(std::move(handler));
    return {};
}

Result<void> ProxyEvent::UnsetReceiveHandler() noexcept
{
    subscription_event_state_machine_.UnsetReceiveHandler();
    return {};
}

Result<void> ProxyEvent::SetSubscriptionStateChangeHandler(SubscriptionStateChangeHandler handler) noexcept
{
    subscription_event_state_machine_.SetSubscriptionStateChangeHandler(std::move(handler));
    return {};
}

Result<void> ProxyEvent::UnsetSubscriptionStateChangeHandler() noexcept
{
    subscription_event_state_machine_.UnsetSubscriptionStateChangeHandler();
    return {};
}

std::optional<std::uint16_t> ProxyEvent::GetMaxSampleCount() const noexcept
{
    return subscription_event_state_machine_.GetMaxSampleCount();
}

void ProxyEvent::NotifyServiceInstanceChangedAvailability(const bool is_available,
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
