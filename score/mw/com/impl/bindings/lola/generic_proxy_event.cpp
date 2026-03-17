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
///
/// @file
/// @copyright Copyright (C) 2023, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
///

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
      meta_info_{parent.GetEventMetaInfo(element_fq_id)}
{
}

ResultBlank GenericProxyEvent::Subscribe(const std::size_t max_sample_count) noexcept
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

inline Result<std::size_t> GenericProxyEvent::GetNumNewSamplesAvailable() const noexcept
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

inline Result<std::size_t> GenericProxyEvent::GetNewSamples(Callback&& receiver, TrackerGuardFactory& tracker) noexcept
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
    return meta_info_.data_type_info_.size_of_;
}

bool GenericProxyEvent::HasSerializedFormat() const noexcept
{
    // our shared-memory based binding does no serialization at all!
    return false;
}

ResultBlank GenericProxyEvent::SetReceiveHandler(std::weak_ptr<ScopedEventReceiveHandler> handler) noexcept
{
    return proxy_event_common_.SetReceiveHandler(std::move(handler));
}

ResultBlank GenericProxyEvent::UnsetReceiveHandler() noexcept
{
    return proxy_event_common_.UnsetReceiveHandler();
}

pid_t GenericProxyEvent::GetEventSourcePid() const noexcept
{
    return proxy_event_common_.GetEventSourcePid();
}

ElementFqId GenericProxyEvent::GetElementFQId() const noexcept
{
    return proxy_event_common_.GetElementFQId();
}

std::optional<std::uint16_t> GenericProxyEvent::GetMaxSampleCount() const noexcept
{
    return proxy_event_common_.GetMaxSampleCount();
}

Result<std::size_t> GenericProxyEvent::GetNumNewSamplesAvailableImpl() const noexcept
{
    return proxy_event_common_.GetNumNewSamplesAvailable();
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from '.value()' in case it doesn't have a value but as we check
// before with 'has_value()' so no way for throwing std::bad_optional_access which leds to std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
Result<std::size_t> GenericProxyEvent::GetNewSamplesImpl(Callback&& receiver, TrackerGuardFactory& tracker) noexcept
{
    const auto max_sample_count = tracker.GetNumAvailableGuards();

    const auto slot_indicators = proxy_event_common_.GetNewSamplesSlotIndices(max_sample_count);

    auto& event_control = proxy_event_common_.GetEventControl();

    const std::size_t sample_size = meta_info_.data_type_info_.size_of_;
    const std::uint8_t sample_alignment = meta_info_.data_type_info_.align_of_;
    const std::size_t aligned_size =
        memory::shared::CalculateAlignedSize(sample_size, static_cast<std::size_t>(sample_alignment));

    auto transaction_log_index = proxy_event_common_.GetTransactionLogIndex();
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
        transaction_log_index.has_value(),
        "GetNewSamplesImpl should only be called after a TransactionLog has been registered.");

    const std::size_t max_number_of_sample_slots = event_control.data_control.GetMaxSampleSlots();
    const auto event_slots_raw_array_size = safe_math::Multiply(aligned_size, max_number_of_sample_slots);

    if (!event_slots_raw_array_size.has_value())
    {
        score::mw::log::LogFatal("lola") << "Could not calculate the event slots raw array size. Terminating.";
        std::terminate();
    }
    const void* const event_slots_raw_array = meta_info_.event_slots_raw_array_.get(event_slots_raw_array_size.value());

    // AMP assert that the event_slots_raw_array address is according to sample_alignment
    for (auto slot_indicator = slot_indicators.begin; slot_indicator != slot_indicators.end; ++slot_indicator)
    {
        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic) The pointer event_slots_raw_array points
        // to the memory managed by a DynamicArray which has been type erased. The DynamicArray wraps a regular pointer
        // array so elements may be accessed by using offsets to regular pointers to elements. Therefore, the pointer
        // arithmetic is being done on memory which can be treated as an array.
        // Suppress "AUTOSAR C++14 M5-2-8" rule: "An object with integer type or pointer to void type shall not be
        // converted to an object with pointer type.".
        // Casting to uint8_t pointer is as minimum byte size for pointer arithmetic to address a certain chunk of
        // memory.
        // coverity[autosar_cpp14_m5_2_8_violation]
        const auto* const event_slots_array = static_cast<const std::uint8_t*>(event_slots_raw_array);
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(nullptr != event_slots_array, "Null event slot array");
        // Suppress "AUTOSAR C++14 A5-3-2" rule finding. This rule states: "Null pointers shall not be dereferenced.".
        // Suppress "AUTOSAR C++14 M5-0-15" rule finding. This rule states: "Array indexing shall be the only form of
        // pointer arithmetic.".
        // Suppress "AUTOSAR C++14 A4-7-1" rule finding. This rule states: "An integer expression shall not lead to
        // data loss.". The result of the integer operation will be stored in a std::size_t which is the biggest integer
        // type.
        // coverity[autosar_cpp14_a5_3_2_violation] The check above ensures that array is not NULL
        // coverity[autosar_cpp14_m5_0_15_violation] False-positive, get access through array indexing
        // coverity[autosar_cpp14_a4_7_1_violation]
        const auto* const object_start_address = &event_slots_array[aligned_size * slot_indicator->GetIndex()];
        /* NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic) deviation ends here */

        const EventSlotStatus event_slot_status{slot_indicator->GetSlot().load()};
        const EventSlotStatus::EventTimeStamp sample_timestamp{event_slot_status.GetTimeStamp()};

        SamplePtr<void> sample{
            object_start_address, event_control.data_control, *slot_indicator, transaction_log_index.value()};

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

    const auto num_collected_slots =
        static_cast<std::size_t>(std::distance(slot_indicators.begin, slot_indicators.end));
    return num_collected_slots;
}

void GenericProxyEvent::NotifyServiceInstanceChangedAvailability(bool is_available, pid_t new_event_source_pid) noexcept
{
    proxy_event_common_.NotifyServiceInstanceChangedAvailability(is_available, new_event_source_pid);
}

}  // namespace score::mw::com::impl::lola
