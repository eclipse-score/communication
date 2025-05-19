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
#include "score/mw/com/impl/bindings/lola/event_data_control_composite.h"
#include <utility>

namespace score::mw::com::impl::lola::detail_event_data_control_composite
{

namespace
{
constexpr std::size_t MAX_MULTI_ALLOCATE_RETRY_COUNT{100U};
}

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): all members are initialized in the delegated constructor
template <template <class> class AtomicIndirectorType>
EventDataControlCompositeImpl<AtomicIndirectorType>::EventDataControlCompositeImpl(
    EventDataControl* const asil_qm_control)
    : EventDataControlCompositeImpl{asil_qm_control, nullptr}
{
}

template <template <class> class AtomicIndirectorType>
EventDataControlCompositeImpl<AtomicIndirectorType>::EventDataControlCompositeImpl(
    EventDataControl* const asil_qm_control,
    EventDataControl* const asil_b_control)
    : asil_qm_control_{asil_qm_control}, asil_b_control_{asil_b_control}, ignore_qm_control_{false}
{
    CheckForValidDataControls();
}

template <template <class> class AtomicIndirectorType>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, all results which are accessed with '.value()' that could implicitly call
// 'std::terminate()' (in case it doesn't have value) has a check in advance using '.has_value()', so no way for
// throwing std::bad_optional_access which leds to std::terminate(). This suppression should be removed after fixing
// [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto EventDataControlCompositeImpl<AtomicIndirectorType>::GetNextFreeMultiSlot() const noexcept
    -> ControlSlotCompositeIndicator
{
    EventSlotStatus::EventTimeStamp oldest_time_stamp{EventSlotStatus::TIMESTSCORE_LANGUAGE_FUTURECPP_MAX};
    score::cpp::optional<SlotIndexType> possible_index{};
    EventDataControl::EventControlSlots::iterator qm_slot_ptr{nullptr};
    EventDataControl::EventControlSlots::iterator asil_b_slot_ptr{nullptr};

    // Here we are explicitly iterating via iterators/raw-pointers over the slots as it avoids unnecessary
    // bounds-checks, which index access would apply!
    // Suppress "AUTOSAR C++14 A5-2-2" finding rule. This rule states: "Traditional C-style casts shall not be used".
    // There is no C-style cast happening here! "current_index" and "it" are automatically deduced.
    // Suppress "AUTOSAR C++14 M6-5-5" finding rule. This rule states: "A loop-control-variable other than the
    // loop-counter shall not be modified within condition or expression".
    // Both variables (current_index, it) depict exactly the same element once via index addressing and once via
    // iterator/raw-pointer! The func has to return both, and we want both to be fully in sync, therefore we increment
    // them both symmetrically, since in this case, it is less error-prone!
    // Suppress "AUTOSAR C++14 M5-0-15". This rule states:"indexing shall be the only form of pointer arithmetic.".
    // Rationale: Tolerated due to containers providing pointer-like iterators.
    // The Coverity tool considers these iterators as raw pointers.
    // Suppress "AUTOSAR C++14 A12-8-3". This rule states:"Moved-from object shall not be read-accessed.".
    // Rationale: This is a false positive, object is not moved-from.
    // Suppress "AUTOSAR C++14 M5-2-10". This rule states: "The increment (++) and decrement (--) operators shall not be
    // mixed with other operators in an expression".
    // Rationale: We are only using increment operators here on separate variables! That we use it on separate variables
    // is explained above for M6-5-5.
    for (auto [current_index, it_slots_qm, it_slots_asil_b] = std::make_tuple(
             // coverity[autosar_cpp14_a5_2_2_violation]
             std::size_t{0U},
             asil_qm_control_->state_slots_.begin(),
             asil_b_control_->state_slots_.begin());
         current_index != asil_b_control_->state_slots_.size();
         // coverity[autosar_cpp14_m6_5_5_violation]
         // coverity[autosar_cpp14_m5_0_15_violation]
         // coverity[autosar_cpp14_a12_8_3_violation : FALSE]
         // coverity[autosar_cpp14_m5_2_10_violation]
         ++it_slots_qm,
                                           // coverity[autosar_cpp14_m5_0_15_violation]
                                           // coverity[autosar_cpp14_m6_5_5_violation]
         ++it_slots_asil_b,
                                           // coverity[autosar_cpp14_a12_8_3_violation : FALSE]
                                           // coverity[autosar_cpp14_m6_5_5_violation]
         ++current_index)
    {
        const EventSlotStatus slot_qm{it_slots_qm->load(std::memory_order_acquire)};
        const EventSlotStatus slot_b{it_slots_asil_b->load(std::memory_order_acquire)};
        if (slot_b.IsInvalid() || ((slot_qm.IsUsed() == false) && (slot_b.IsUsed() == false)))
        {
            const auto time_stamp = slot_b.GetTimeStamp();
            if (time_stamp < oldest_time_stamp)
            {
                possible_index = current_index;
                qm_slot_ptr = it_slots_qm;
                asil_b_slot_ptr = it_slots_asil_b;
                oldest_time_stamp = time_stamp;
            }
        }
    }

    if (possible_index.has_value())
    {
        return {possible_index.value(), *qm_slot_ptr, *asil_b_slot_ptr};
    }
    else
    {
        return {};
    }
}

template <template <class> class AtomicIndirectorType>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from 'state_slots_[]' which might leds to a segmentation fault
// in case the index goes outside the range. As we already do an index check before accessing, so no way for
// segmentation fault which leds to calling std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto EventDataControlCompositeImpl<AtomicIndirectorType>::TryLockSlot(
    ControlSlotCompositeIndicator slot_indicator) noexcept -> bool
{
    auto& slot_value_qm = slot_indicator.GetSlotQM();
    auto& slot_value_asil_b = slot_indicator.GetSlotAsilB();

    EventSlotStatus asil_qm_old{slot_value_qm.load(std::memory_order_acquire)};
    EventSlotStatus asil_b_old{slot_value_asil_b.load(std::memory_order_acquire)};

    if (asil_qm_old.IsUsed() || asil_b_old.IsUsed())
    {
        return false;
    }

    EventSlotStatus in_writing{};
    in_writing.MarkInWriting();

    auto asil_qm_old_value_type = static_cast<EventSlotStatus::value_type&>(asil_qm_old);
    auto in_writing_value_type = static_cast<EventSlotStatus::value_type&>(in_writing);
    if (AtomicIndirectorType<EventSlotStatus::value_type>::compare_exchange_strong(
            slot_value_qm, asil_qm_old_value_type, in_writing_value_type, std::memory_order_acq_rel) == false)
    {
        return false;
    }

    auto asil_b_old_value_type = static_cast<EventSlotStatus::value_type&>(asil_b_old);
    if (AtomicIndirectorType<EventSlotStatus::value_type>::compare_exchange_strong(
            slot_value_asil_b, asil_b_old_value_type, in_writing_value_type, std::memory_order_acq_rel) == false)
    {
        // Roll back write lock on asil qm since lock on b failed
        slot_value_qm.store(static_cast<EventSlotStatus::value_type>(asil_qm_old), std::memory_order_release);
        return false;
    }

    return true;
}

template <template <class> class AtomicIndirectorType>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from 'possible_slot.value()' in case the possible_slot doesn't
// have a value but as we check before with 'has_value()' so no way for throwing std::bad_optional_access which leds
// to std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto EventDataControlCompositeImpl<AtomicIndirectorType>::AllocateNextMultiSlot() noexcept
    -> ControlSlotCompositeIndicator
{
    // \todo we should also monitor retry counts in the multi-slot/EventDataControlComposite case, like we are doing in
    //  EventDataControl! Currently we are "blind", if we have retries, because ASIL-QM/ASIL-B consumers do influence
    // each others.
    for (std::size_t counter{0U}; counter < MAX_MULTI_ALLOCATE_RETRY_COUNT; ++counter)
    {
        const auto possible_slot = GetNextFreeMultiSlot();
        if (possible_slot.IsValidQmAndAsilB())
        {
            if (TryLockSlot(possible_slot))
            {
                return possible_slot;
            }
        }
    }

    return {};
}

template <template <class> class AtomicIndirectorType>
auto EventDataControlCompositeImpl<AtomicIndirectorType>::AllocateNextSlot() noexcept -> ControlSlotCompositeIndicator
{

    if (asil_b_control_ == nullptr)
    {
        auto qm_control_slot_indicator = asil_qm_control_->AllocateNextSlot();
        if (qm_control_slot_indicator.IsValid())
        {
            return {qm_control_slot_indicator.GetIndex(),
                    qm_control_slot_indicator.GetSlot(),
                    ControlSlotCompositeIndicator::CompositeSlotTagType::QM};
        }
        return {};
    }

    if (ignore_qm_control_)
    {
        auto asil_b_control_slot_indicator = asil_b_control_->AllocateNextSlot();
        if (asil_b_control_slot_indicator.IsValid())
        {
            return {asil_b_control_slot_indicator.GetIndex(),
                    asil_b_control_slot_indicator.GetSlot(),
                    ControlSlotCompositeIndicator::CompositeSlotTagType::ASIL_B};
        }
        return {};
    }

    auto slot = AllocateNextMultiSlot();
    if (!(slot.IsValidQmAndAsilB()))
    {
        // we failed to allocate a "multi-slot". This is per our definition a misbehaviour of the QM consumers.
        // Even if it could be, that the ASIL-B side (ASIL-B producer and ASIL-B consumers are occupying all slots!
        // From this point onwards, we ignore/dismiss the whole qm control section -> although it might NOT guarantee us
        // to be able to allocate a further slot ...
        ignore_qm_control_ = true;
        // fall back to allocation solely within asil_b control.
        auto asil_b_control_slot_indicator = asil_b_control_->AllocateNextSlot();
        if (asil_b_control_slot_indicator.IsValid())
        {
            return {asil_b_control_slot_indicator.GetIndex(),
                    asil_b_control_slot_indicator.GetSlot(),
                    ControlSlotCompositeIndicator::CompositeSlotTagType::ASIL_B};
        }
        return {};
    }
    return slot;
}

template <template <class> class AtomicIndirectorType>
auto EventDataControlCompositeImpl<AtomicIndirectorType>::EventReady(
    ControlSlotCompositeIndicator slot_indicator,
    EventSlotStatus::EventTimeStamp time_stamp) noexcept -> void
{
    if (asil_b_control_ != nullptr)
    {
        asil_b_control_->EventReady({slot_indicator.GetIndex(), slot_indicator.GetSlotAsilB()}, time_stamp);
    }

    if (!ignore_qm_control_)
    {
        asil_qm_control_->EventReady({slot_indicator.GetIndex(), slot_indicator.GetSlotQM()}, time_stamp);
    }
}

template <template <class> class AtomicIndirectorType>
auto EventDataControlCompositeImpl<AtomicIndirectorType>::Discard(ControlSlotCompositeIndicator slot_indicator) -> void
{
    if (asil_b_control_ != nullptr)
    {
        asil_b_control_->Discard({slot_indicator.GetIndex(), slot_indicator.GetSlotAsilB()});
    }

    if (!ignore_qm_control_)
    {
        asil_qm_control_->Discard({slot_indicator.GetIndex(), slot_indicator.GetSlotQM()});
    }
}

template <template <class> class AtomicIndirectorType>
bool EventDataControlCompositeImpl<AtomicIndirectorType>::IsQmControlDisconnected() const noexcept
{
    return ignore_qm_control_;
}

template <template <class> class AtomicIndirectorType>
EventDataControl& EventDataControlCompositeImpl<AtomicIndirectorType>::GetQmEventDataControl() const noexcept
{
    return *asil_qm_control_;
}

template <template <class> class AtomicIndirectorType>
std::optional<EventDataControl*>
EventDataControlCompositeImpl<AtomicIndirectorType>::GetAsilBEventDataControl() noexcept
{
    if (asil_b_control_ != nullptr)
    {
        return asil_b_control_;
    }
    return {};
}

template <template <class> class AtomicIndirectorType>
EventSlotStatus::EventTimeStamp EventDataControlCompositeImpl<AtomicIndirectorType>::GetEventSlotTimestamp(
    const SlotIndexType slot) const noexcept
{
    if (asil_b_control_ != nullptr)
    {
        const EventSlotStatus event_slot_status{(*asil_b_control_)[slot]};
        const EventSlotStatus::EventTimeStamp sample_timestamp{event_slot_status.GetTimeStamp()};
        return sample_timestamp;
    }
    else
    {
        const EventSlotStatus event_slot_status{(*asil_qm_control_)[slot]};
        const EventSlotStatus::EventTimeStamp sample_timestamp{event_slot_status.GetTimeStamp()};
        return sample_timestamp;
    }
}

template <template <class> class AtomicIndirectorType>
void EventDataControlCompositeImpl<AtomicIndirectorType>::CheckForValidDataControls() const noexcept
{
    if (asil_qm_control_ == nullptr)
    {
        std::terminate();
    }
}

template <template <class> class AtomicIndirectorType>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from 'state_slots_[]' which might leds to a segmentation fault
// in case the index goes outside the range. As we already do an index check before accessing, so no way for
// segmentation fault which leds to calling std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
EventSlotStatus::EventTimeStamp EventDataControlCompositeImpl<AtomicIndirectorType>::GetLatestTimestamp() const noexcept
{
    EventSlotStatus::EventTimeStamp latest_time_stamp{1U};
    EventDataControl* control = (asil_b_control_ != nullptr) ? asil_b_control_ : asil_qm_control_;
    for (SlotIndexType slot_index = 0U;
         // Suppress "AUTOSAR C++14 A4-7-1" rule finding. This rule states: "An integer expression shall not lead to
         // loss.". As the maximum number of slots is std::uint16_t, so there is no case for a data loss here.
         // coverity[autosar_cpp14_a4_7_1_violation]
         slot_index < static_cast<SlotIndexType>(control->state_slots_.size());
         ++slot_index)
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(static_cast<std::size_t>(slot_index) < control->state_slots_.size());
        const EventSlotStatus slot{control->state_slots_[slot_index].load(std::memory_order_acquire)};
        if (!slot.IsInvalid() && !slot.IsInWriting())
        {
            const auto slot_time_stamp = slot.GetTimeStamp();
            if (latest_time_stamp < slot_time_stamp)
            {
                latest_time_stamp = slot_time_stamp;
            }
        }
    }
    return latest_time_stamp;
}

template class EventDataControlCompositeImpl<memory::shared::AtomicIndirectorReal>;
template class EventDataControlCompositeImpl<memory::shared::AtomicIndirectorMock>;

}  // namespace score::mw::com::impl::lola::detail_event_data_control_composite
