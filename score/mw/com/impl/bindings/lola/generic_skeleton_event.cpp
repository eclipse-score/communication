/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional information
 * regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/
#include "score/mw/com/impl/bindings/lola/generic_skeleton_event.h"
#include "score/memory/shared/pointer_arithmetic_util.h"
#include "score/mw/com/impl/bindings/lola/skeleton.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event_properties.h"
#include "score/mw/com/impl/runtime.h"

namespace score::mw::com::impl::lola
{
GenericSkeletonEvent::GenericSkeletonEvent(Skeleton& parent,
                                             const SkeletonEventProperties& event_properties,
                                             const ElementFqId& event_fqn,
                                             const DataTypeMetaInfo& size_info,
                                             impl::tracing::SkeletonEventTracingData tracing_data)
    : size_info_(size_info), 
      event_properties_(event_properties),
      event_shared_impl_(parent, event_fqn, control_, current_timestamp_, tracing_data)
{

}

ResultBlank GenericSkeletonEvent::PrepareOffer() noexcept
{
    std::tie(data_storage_, control_) =
        event_shared_impl_.GetParent().RegisterGeneric(event_shared_impl_.GetElementFQId(), event_properties_, size_info_.size, size_info_.alignment);
    event_shared_impl_.PrepareOfferCommon();

    return {};
}

Result<score::Blank> GenericSkeletonEvent::Send(score::mw::com::impl::SampleAllocateePtr<void> sample) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(control_.has_value());
    const impl::SampleAllocateePtrView<void> view{sample};
    auto ptr = view.template As<lola::SampleAllocateePtr<void>>();
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(nullptr != ptr);

    auto control_slot_indicator = ptr->GetReferencedSlot();
    control_.value().EventReady(control_slot_indicator, ++current_timestamp_);
 
    // Only call NotifyEvent if there are any registered receive handlers for each quality level (QM and ASIL-B).
    // This avoids the expensive lock operation in the common case where no handlers are registered.
    // Using memory_order_relaxed is safe here as this is an optimisation, if we miss a very recent
    // handler registration, the next Send() will pick it up.
    

    if (event_shared_impl_.IsQmNotificationsRegistered() && !qm_disconnect_)
    {
        GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa)
            .GetLolaMessaging()
            .NotifyEvent(QualityType::kASIL_QM, event_shared_impl_.GetElementFQId());
    }
    if (event_shared_impl_.IsAsilBNotificationsRegistered() && event_shared_impl_.GetParent().GetInstanceQualityType() == QualityType::kASIL_B)
    {
        GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa)
            .GetLolaMessaging()
            .NotifyEvent(QualityType::kASIL_B, event_shared_impl_.GetElementFQId());
    }

    return {};
}

Result<score::mw::com::impl::SampleAllocateePtr<void>> GenericSkeletonEvent::Allocate() noexcept
{
    if (!control_.has_value())
    {
        ::score::mw::log::LogError("lola") << "Tried to allocate event, but the EventDataControl does not exist!";
        return MakeUnexpected(ComErrc::kBindingFailure);
    }
    const auto slot = control_.value().AllocateNextSlot();

    if (!qm_disconnect_ && control_->GetAsilBEventDataControl().has_value() && !slot.IsValidQM())
    {
        qm_disconnect_ = true;
        score::mw::log::LogWarn("lola")
            << __func__ << __LINE__
            << "Disconnecting unsafe QM consumers as slot allocation failed on an ASIL-B enabled event: " << event_shared_impl_.GetElementFQId();
        event_shared_impl_.GetParent().DisconnectQmConsumers();
    }

    if (slot.IsValidQM() || slot.IsValidAsilB())
    {
        void* base_ptr = data_storage_.get<std::uint8_t>();
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(base_ptr != nullptr);

        const auto aligned_size = memory::shared::CalculateAlignedSize(size_info_.size, size_info_.alignment);
        std::uint8_t* byte_ptr = static_cast<std::uint8_t*>(base_ptr);
        std::uint64_t offset = static_cast<std::uint64_t>(slot.GetIndex()) * aligned_size;

        void* data_ptr = byte_ptr + offset;   
        auto lola_ptr = lola::SampleAllocateePtr<void>(data_ptr, control_.value(), slot);
        return impl::MakeSampleAllocateePtr(std::move(lola_ptr));
    }
    else
    {
        if (!event_properties_.enforce_max_samples)
        {
            ::score::mw::log::LogError("lola")
                << "GenericSkeletonEvent: Allocation of event slot failed. Hint: enforceMaxSamples was "
                   "disabled by config. Might be the root cause!";
        }
        return MakeUnexpected(ComErrc::kBindingFailure);
    }
}

std::pair<size_t, size_t> GenericSkeletonEvent::GetSizeInfo() const noexcept
{
    return {size_info_.size, size_info_.alignment};
}

void GenericSkeletonEvent::PrepareStopOffer() noexcept
{
    event_shared_impl_.PrepareStopOfferCommon();
    control_.reset();
    data_storage_ = nullptr;
}

BindingType GenericSkeletonEvent::GetBindingType() const noexcept
{
    return BindingType::kLoLa;
}
 

} // namespace score::mw::com::impl::lola