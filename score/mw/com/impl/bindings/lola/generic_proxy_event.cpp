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

#include "score/mw/com/impl/bindings/lola/generic_proxy_event.h"

#include "score/language/safecpp/safe_math/safe_math.h"
#include "score/memory/shared/pointer_arithmetic_util.h"
#include "score/mw/log/logging.h"

#include <score/assert.hpp>

namespace score::mw::com::impl::lola
{

GenericProxyEvent::GenericProxyEvent(Proxy& parent, const ElementFqId element_fq_id, const std::string_view event_name)
    : GenericProxyEventBinding{},
      proxy_event_common_{parent, element_fq_id, event_name},
      meta_info_{parent.GetEventMetaInfo(element_fq_id)},
      event_data_storage_{parent.GetEventDataStorage(element_fq_id)}
{
    parent.RegisterEvent(event_name, *this);
}

Result<void> GenericProxyEvent::Subscribe(const std::size_t max_sample_count) noexcept
{
    return proxy_event_common_.Subscribe(max_sample_count);
}

void GenericProxyEvent::Unsubscribe() noexcept
{
    proxy_event_common_.Unsubscribe();
}

SubscriptionState GenericProxyEvent::GetSubscriptionState() const noexcept
{
    return proxy_event_common_.GetSubscriptionState();
}

inline Result<std::size_t> GenericProxyEvent::GetNumNewSamplesAvailable() const
{
    /// In case of LoLa binding we can also dispatch to GetNumNewSamplesAvailableImpl() in case of kSubscriptionPending!
    /// Because a pre-condition to kSubscriptionPending is that we once had a successful subscription... and then we can
    /// always access the samples even if the provider went down.
    const auto subscription_state = proxy_event_common_.GetSubscriptionState();
    if (subscription_state == SubscriptionState::kNotSubscribed)
    {
        return MakeUnexpected(ComErrc::kNotSubscribed,
                              "Attempt to call GetNumNewSamplesAvailable without successful subscription.");
    }
    return GetNumNewSamplesAvailableImpl();
}

inline Result<std::size_t> GenericProxyEvent::GetNewSamples(Callback&& receiver, TrackerGuardFactory& tracker)
{
    /// In case of LoLa binding we can also dispatch to GetNewSamplesImpl() in case of kSubscriptionPending!
    /// Because a pre-condition to kSubscriptionPending is that we once had a successful subscription... and then we can
    /// always access the samples even if the provider went down.
    const auto subscription_state = proxy_event_common_.GetSubscriptionState();
    if (subscription_state == SubscriptionState::kNotSubscribed)
    {
        return MakeUnexpected(ComErrc::kNotSubscribed,
                              "Attempt to call GetNewSamples without successful subscription.");
    }
    return GetNewSamplesImpl(std::move(receiver), tracker);
}

std::size_t GenericProxyEvent::GetSampleSize() const noexcept
{
    return meta_info_.data_type_info_.Size();
}

bool GenericProxyEvent::HasSerializedFormat() const noexcept
{
    // our shared-memory based binding does no serialization at all!
    return false;
}

Result<void> GenericProxyEvent::SetReceiveHandler(std::weak_ptr<ScopedEventReceiveHandler> handler) noexcept
{
    return proxy_event_common_.SetReceiveHandler(std::move(handler));
}

Result<void> GenericProxyEvent::UnsetReceiveHandler() noexcept
{
    return proxy_event_common_.UnsetReceiveHandler();
}

Result<void> GenericProxyEvent::SetSubscriptionStateChangeHandler(SubscriptionStateChangeHandler handler) noexcept
{
    return proxy_event_common_.SetSubscriptionStateChangeHandler(std::move(handler));
}

Result<void> GenericProxyEvent::UnsetSubscriptionStateChangeHandler() noexcept
{
    return proxy_event_common_.UnsetSubscriptionStateChangeHandler();
}

ElementFqId GenericProxyEvent::GetElementFQId() const noexcept
{
    return proxy_event_common_.GetElementFQId();
}

std::optional<std::uint16_t> GenericProxyEvent::GetMaxSampleCount() const noexcept
{
    return proxy_event_common_.GetMaxSampleCount();
}

Result<std::size_t> GenericProxyEvent::GetNumNewSamplesAvailableImpl() const
{
    return proxy_event_common_.GetNumNewSamplesAvailable();
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from '.value()' in case it doesn't have a value but as we check
// before with 'has_value()' so no way for throwing std::bad_optional_access which leds to std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
Result<std::size_t> GenericProxyEvent::GetNewSamplesImpl(Callback&& receiver, TrackerGuardFactory& tracker)
{
    const auto max_sample_count = tracker.GetNumAvailableGuards();

    const auto slot_indices = proxy_event_common_.GetNewSamplesSlotIndices(max_sample_count);

    auto& event_data_control_local = proxy_event_common_.GetConsumerEventDataControlLocal();

    const std::size_t sample_size = meta_info_.data_type_info_.Size();

    for (auto slot_it = slot_indices.begin; slot_it != slot_indices.end; ++slot_it)
    {
        const auto slot_index = *slot_it;

        const void* type_erased_sample_ptr = event_data_storage_.GetTypeErasedDataSlot(slot_index, sample_size);

        const EventSlotStatus event_slot_status{event_data_control_local[slot_index]};
        const EventSlotStatus::EventTimeStamp sample_timestamp{event_slot_status.GetTimeStamp()};

        SamplePtr<void> sample{type_erased_sample_ptr, event_data_control_local, slot_index};

        auto guard = std::move(*tracker.TakeGuard());
        auto sample_binding_independent = this->MakeSamplePtr(std::move(sample), std::move(guard));

        // Suppress "AUTOSAR C++14 A18-9-2" rule finding: "Forwarding values to other functions shall be done via:
        // (1) std::move if the value is an rvalue reference, (2) std::forward if the value is forwarding
        // reference".
        // First parameter is moved but moving the second one doesn't add any benefit as the copy-constructor is called
        // implicitly instead of move constructor.
        // Suppress "AUTOSAR C++14 A15-4-2" rule finding. This rule states: "I a function is declared to be
        // noexcept, noexcept(true) or noexcept(<true condition>), then it shall not exit with an exception"
        // we can't add noexcept to score::cpp::callback signature.
        // coverity[autosar_cpp14_a18_9_2_violation]
        // coverity[autosar_cpp14_a15_4_2_violation]
        receiver(std::move(sample_binding_independent), sample_timestamp);
    }

    const auto num_collected_slots = static_cast<std::size_t>(std::distance(slot_indices.begin, slot_indices.end));
    return num_collected_slots;
}

void GenericProxyEvent::NotifyServiceInstanceChangedAvailability(bool is_available, pid_t new_event_source_pid) noexcept
{
    proxy_event_common_.NotifyServiceInstanceChangedAvailability(is_available, new_event_source_pid);
}

}  // namespace score::mw::com::impl::lola
