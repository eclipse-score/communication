/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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
#include "score/mw/com/impl/bindings/someip/skeleton_event.h"

#include "score/mw/com/impl/bindings/someip/sample_allocatee_ptr.h"
#include "score/mw/com/impl/com_error.h"

#include "score/mw/log/logging.h"

#include <score/assert.hpp>
#include <score/utility.hpp>

#include <cstring>
#include <limits>
#include <utility>

namespace score::mw::com::impl::someip
{

SkeletonEvent::SkeletonEvent(Skeleton& parent,
                             const ElementFqId element_fq_id,
                             const std::string_view event_name,
                             const memory::DataTypeSizeInfo size_info,
                             const SkeletonEventProperties properties,
                             impl::tracing::SkeletonEventTracingData skeleton_event_tracing_data) noexcept
    : SkeletonEventBinding{},
      parent_{parent},
      event_name_{event_name},
      element_fq_id_{element_fq_id},
      event_data_storage_{nullptr},
      event_sample_size_info_{size_info},
      event_properties_{properties},
      slot_in_use_{},
      is_offered_{false},
      tracing_data_{skeleton_event_tracing_data},
      receive_handler_registration_changed_callback_{}
{
}

Result<void> SkeletonEvent::Send(const void* value_ptr,
                                 std::optional<SendTraceCallback> send_trace_callback,
                                 SampleAllocateeGuard guard) noexcept
{
    if (value_ptr == nullptr)
    {
        return MakeUnexpected(ComErrc::kBindingFailure, "Send called with a nullptr sample");
    }

    auto allocated_slot_result = Allocate(std::move(guard));
    if (!allocated_slot_result.has_value())
    {
        return MakeUnexpected(ComErrc::kSampleAllocationFailure, "Could not allocate slot");
    }
    auto allocated_slot = std::move(allocated_slot_result).value();
    std::memcpy(allocated_slot.Get(), value_ptr, event_sample_size_info_.Size());

    return Send(std::move(allocated_slot), std::move(send_trace_callback));
}

Result<void> SkeletonEvent::Send(impl::SampleAllocateePtr<void> sample,
                                 std::optional<SendTraceCallback> send_trace_callback) noexcept
{
    const impl::SampleAllocateePtrView<void> view{sample};
    const auto* ptr = view.template As<someip::SampleAllocateePtr>();
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(nullptr != ptr);
    // Suppress "AUTOSAR C++14 A5-3-2": "Null pointers shall not be dereferenced". The pointer is checked above.
    // coverity[autosar_cpp14_a5_3_2_violation]
    const auto* const slot_data = ptr->get();

    const auto send_result = parent_.GetTransport().SendEvent(
        element_fq_id_, slot_data, static_cast<std::size_t>(event_sample_size_info_.Size()));

    if (!send_result.has_value())
    {
        return send_result;
    }

    if (send_trace_callback.has_value())
    {
        (*send_trace_callback)(sample);
    }

    // The slot is not discarded explicitly here: `sample` owns it and returns it via DiscardSlot() when it goes out
    // of scope at the end of this function. That happens on the error path as well, so no slot can leak, and it
    // happens only after the trace callback has read the slot. Discarding here in addition would return the same
    // slot twice and would mark it reusable while it is still being read.
    return {};
}

Result<impl::SampleAllocateePtr<void>> SkeletonEvent::Allocate(SampleAllocateeGuard guard) noexcept
{
    if (event_data_storage_ == nullptr)
    {
        ::score::mw::log::LogError("someip")
            << "SkeletonEvent::Allocate failed as the event has not been offered:" << event_name_;
        return MakeUnexpected(ComErrc::kNotOffered);
    }

    const auto slot_index = AllocateSlot();
    if (!slot_index.has_value())
    {
        if (!event_properties_.enforce_max_samples)
        {
            ::score::mw::log::LogError("someip")
                << "SkeletonEvent: Allocation of event slot failed. Hint: enforceMaxSamples was "
                   "disabled by config. Might be the root cause!";
        }
        return MakeUnexpected(ComErrc::kBindingFailure);
    }

    return MakeSampleAllocateePtr(
        SampleAllocateePtr(
            event_data_storage_->GetTypeErasedDataSlot(*slot_index, event_sample_size_info_.Size()), *this, *slot_index),
        std::move(guard));
}

Result<impl::SamplePtr<void>> SkeletonEvent::GetLatestSample(QualityType quality_type)
{
    score::cpp::ignore = quality_type;
    ::score::mw::log::LogError("someip")
        << "SkeletonEvent::GetLatestSample is not supported by the SOME/IP binding:" << event_name_;
    return MakeUnexpected(ComErrc::kBindingFailure,
                          "GetLatestSample (field getter) is not supported by the SOME/IP binding");
}

Result<void> SkeletonEvent::PrepareOffer(
    const std::optional<InitializeSampleCallback>& initialize_sample_callback) noexcept
{
    const auto total_number_of_slots = event_properties_.GetTotalNumberOfSlots();
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
        total_number_of_slots <= std::numeric_limits<SlotIndexType>::max(),
        "Configured number of sample slots exceeds the maximum representable slot index.");

    const auto registration_result = parent_.Register(element_fq_id_,
                                                      static_cast<SlotIndexType>(total_number_of_slots),
                                                      event_sample_size_info_,
                                                      initialize_sample_callback);
    event_data_storage_ = &registration_result.event_data_storage;

    slot_in_use_.assign(total_number_of_slots, false);

    const auto offer_result = parent_.GetTransport().OfferEvent(element_fq_id_);
    if (!offer_result.has_value())
    {
        event_data_storage_ = nullptr;
        slot_in_use_.clear();
        return offer_result;
    }

    is_offered_ = true;
    return {};
}

void SkeletonEvent::PrepareStopOffer() noexcept
{
    if (!is_offered_)
    {
        return;
    }
    parent_.GetTransport().StopOfferEvent(element_fq_id_);
    is_offered_ = false;

    // The EventDataStorage itself stays alive in the parent Skeleton, so that a subsequent offer can reuse it without
    // re-initializing slots which a consumer of the previous offering could still be reading. This mirrors what
    // lola::Skeleton does with a re-opened shared-memory region.
    event_data_storage_ = nullptr;
    slot_in_use_.clear();
}

void SkeletonEvent::SetSkeletonEventTracingData(impl::tracing::SkeletonEventTracingData tracing_data) noexcept
{
    tracing_data_ = tracing_data;
}

Result<void> SkeletonEvent::Notify() noexcept
{
    if (!is_offered_)
    {
        return MakeUnexpected(ComErrc::kNotOffered, "Notify called on an event which is not offered");
    }
    return parent_.GetTransport().NotifyEvent(element_fq_id_);
}

Result<void> SkeletonEvent::SetReceiveHandlerRegistrationChangedHandler(
    ReceiveHandlerRegistrationChangedCallback callback) noexcept
{
    receive_handler_registration_changed_callback_ = std::move(callback);

    // Report the current state right away, so that the caller does not have to wait for the next change to learn
    // whether receive handlers are currently registered.
    (*receive_handler_registration_changed_callback_)(parent_.GetTransport().HasSubscribers(element_fq_id_));
    return {};
}

Result<void> SkeletonEvent::UnsetReceiveHandlerRegistrationChangedHandler() noexcept
{
    receive_handler_registration_changed_callback_.reset();
    return {};
}

void SkeletonEvent::DiscardSlot(const SlotIndexType slot_index) noexcept
{
    if (static_cast<std::size_t>(slot_index) >= slot_in_use_.size())
    {
        return;
    }
    slot_in_use_[slot_index] = false;
}

std::optional<SlotIndexType> SkeletonEvent::AllocateSlot() noexcept
{
    for (std::size_t slot_index = 0U; slot_index < slot_in_use_.size(); ++slot_index)
    {
        if (!slot_in_use_[slot_index])
        {
            slot_in_use_[slot_index] = true;
            return static_cast<SlotIndexType>(slot_index);
        }
    }
    return std::nullopt;
}

}  // namespace score::mw::com::impl::someip
