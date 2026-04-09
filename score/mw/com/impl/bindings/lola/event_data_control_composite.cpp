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
#include "score/mw/com/impl/bindings/lola/control_slot_types.h"

#include <optional>

namespace score::mw::com::impl::lola
{

namespace
{

constexpr std::size_t MAX_MULTI_ALLOCATE_RETRY_COUNT{100U};

}

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): all members are initialized in the delegated constructor
template <template <class> class AtomicIndirectorType>
EventDataControlComposite<AtomicIndirectorType>::EventDataControlComposite(
    SkeletonEventDataControlLocalView<>& asil_qm_control_local,
    ProxyEventDataControlLocalView<>* const proxy_control_local)
    : EventDataControlComposite{asil_qm_control_local, nullptr, proxy_control_local}
{
}

template <template <class> class AtomicIndirectorType>
EventDataControlComposite<AtomicIndirectorType>::EventDataControlComposite(
    SkeletonEventDataControlLocalView<>& asil_qm_control_local,
    SkeletonEventDataControlLocalView<>* const asil_b_control_local,
    ProxyEventDataControlLocalView<>* const proxy_control_local)
    : asil_qm_control_local_{asil_qm_control_local},
      asil_b_control_local_{asil_b_control_local},
      proxy_control_local_{proxy_control_local},
      ignore_qm_control_{false}
{
}

template <template <class> class AtomicIndirectorType>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, all results which are accessed with '.value()' that could implicitly call
// 'std::terminate()' (in case it doesn't have value) has a check in advance using '.has_value()', so no way for
// throwing std::bad_optional_access which leds to std::terminate(). This suppression should be removed after fixing
// [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto EventDataControlComposite<AtomicIndirectorType>::GetNextFreeMultiSlot() const noexcept
    -> std::optional<SlotWithTimeStamp>
{
    EventSlotStatus::EventTimeStamp oldest_time_stamp{EventSlotStatus::TIMESTAMP_MAX};
    score::cpp::optional<SlotIndexType> possible_index{};
    SkeletonEventDataControlLocalView<>::LocalEventControlSlots::iterator qm_slot_ptr{};
    SkeletonEventDataControlLocalView<>::LocalEventControlSlots::iterator asil_b_slot_ptr{};

    for (SlotIndexType slot_index = 0U;
         // Suppress "AUTOSAR C++14 A4-7-1" rule finding. This rule states: "An integer expression shall not lead to
         // loss.". As the maximum number of slots is std::uint16_t, so there is no case for a data loss here.
         // coverity[autosar_cpp14_a4_7_1_violation]
         slot_index < static_cast<SlotIndexType>(asil_b_control_local_->state_slots_.size());
         ++slot_index)
    {
        const EventSlotStatus slot_qm{
            asil_qm_control_local_.get().state_slots_[slot_index].load(std::memory_order_acquire)};
        const EventSlotStatus slot_b{asil_b_control_local_->state_slots_[slot_index].load(std::memory_order_acquire)};
        if (slot_b.IsInvalid() || ((slot_qm.IsUsed() == false) && (slot_b.IsUsed() == false)))
        {
            const auto time_stamp = slot_b.GetTimeStamp();
            if (time_stamp < oldest_time_stamp)
            {
                possible_index = slot_index;
                oldest_time_stamp = time_stamp;
            }
        }
    }

    if (possible_index.has_value())
    {
        return {{possible_index.value(), oldest_time_stamp}};
    }
    return {};
}

template <template <class> class AtomicIndirectorType>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from 'state_slots_[]' which might leds to a segmentation fault
// in case the index goes outside the range. As we already do an index check before accessing, so no way for
// segmentation fault which leds to calling std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto EventDataControlComposite<AtomicIndirectorType>::TryLockSlot(
    const SlotWithTimeStamp expected_slot_with_timestamp) noexcept -> bool
{
    const auto [slot_index, expected_time_stamp] = expected_slot_with_timestamp;

    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(static_cast<std::size_t>(slot_index) <
                                        asil_qm_control_local_.get().state_slots_.size());
    auto& slot_value_qm_atomic = asil_qm_control_local_.get().state_slots_[slot_index];
    auto& slot_value_asil_b_atomic = asil_b_control_local_->state_slots_[slot_index];

    EventSlotStatus asil_qm_old{slot_value_qm_atomic.load(std::memory_order_acquire)};
    EventSlotStatus asil_b_old{slot_value_asil_b_atomic.load(std::memory_order_acquire)};

    if (asil_qm_old.IsUsed() || asil_b_old.IsUsed())
    {
        return false;
    }

    EventSlotStatus in_writing{};
    in_writing.MarkInWriting();

    auto asil_qm_old_value_type = static_cast<EventSlotStatus::value_type&>(asil_qm_old);
    auto in_writing_value_type = static_cast<EventSlotStatus::value_type&>(in_writing);
    if (AtomicIndirectorType<EventSlotStatus::value_type>::compare_exchange_strong(
            slot_value_qm_atomic, asil_qm_old_value_type, in_writing_value_type, std::memory_order_acq_rel) == false)
    {
        return false;
    }

    auto asil_b_old_value_type = static_cast<EventSlotStatus::value_type&>(asil_b_old);
    if (AtomicIndirectorType<EventSlotStatus::value_type>::compare_exchange_strong(
            slot_value_asil_b_atomic, asil_b_old_value_type, in_writing_value_type, std::memory_order_acq_rel) == false)
    {
        // Roll back write lock on asil qm since lock on b failed
        slot_value_qm_atomic.store(static_cast<EventSlotStatus::value_type>(asil_qm_old), std::memory_order_release);
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
auto EventDataControlComposite<AtomicIndirectorType>::AllocateNextMultiSlot() noexcept -> std::optional<SlotIndexType>
{
    // \todo we should also monitor retry counts in the multi-slot/EventDataControlComposite case, like we are doing in
    //  EventDataControl! Currently we are "blind", if we have retries, because ASIL-QM/ASIL-B consumers do influence
    // each others.
    for (std::size_t counter{0U}; counter < MAX_MULTI_ALLOCATE_RETRY_COUNT; ++counter)
    {
        const auto possible_slot = GetNextFreeMultiSlot();
        if (possible_slot.has_value())
        {
            if (TryLockSlot(possible_slot.value()))
            {
                return possible_slot->slot_index;
            }
        }
    }

    return {};
}

template <template <class> class AtomicIndirectorType>
auto EventDataControlComposite<AtomicIndirectorType>::AllocateNextSlot() noexcept -> AllocationResult
{
    if (asil_b_control_local_ == nullptr)
    {
        return {asil_qm_control_local_.get().AllocateNextSlot(), false};
    }

    if (ignore_qm_control_)
    {
        return {asil_b_control_local_->AllocateNextSlot(), true};
    }

    auto slot = AllocateNextMultiSlot();
    if (!(slot.has_value()))
    {
        // we failed to allocate a "multi-slot". This is per our definition a misbehaviour of the QM consumers.
        // Even if it could be, that the ASIL-B side (ASIL-B producer and ASIL-B consumers are occupying all slots!
        // From this point onwards, we ignore/dismiss the whole qm control section -> although it might NOT guarantee us
        // to be able to allocate a further slot ...
        ignore_qm_control_ = true;
        // fall back to allocation solely within asil_b control.
        slot = asil_b_control_local_->AllocateNextSlot();
    }
    return {slot, ignore_qm_control_};
}

template <template <class> class AtomicIndirectorType>
auto EventDataControlComposite<AtomicIndirectorType>::EventReady(const SlotIndexType slot_index,
                                                                 EventSlotStatus::EventTimeStamp time_stamp) noexcept
    -> void
{
    if (asil_b_control_local_ != nullptr)
    {
        asil_b_control_local_->EventReady(slot_index, time_stamp);
    }

    if (!ignore_qm_control_)
    {
        asil_qm_control_local_.get().EventReady(slot_index, time_stamp);
    }
}

template <template <class> class AtomicIndirectorType>
auto EventDataControlComposite<AtomicIndirectorType>::Discard(const SlotIndexType slot_index) -> void
{
    if (asil_b_control_local_ != nullptr)
    {
        asil_b_control_local_->Discard(slot_index);
    }

    if (!ignore_qm_control_)
    {
        asil_qm_control_local_.get().Discard(slot_index);
    }
}

template <template <class> class AtomicIndirectorType>
bool EventDataControlComposite<AtomicIndirectorType>::IsQmControlDisconnected() const noexcept
{
    return ignore_qm_control_;
}

template <template <class> class AtomicIndirectorType>
SkeletonEventDataControlLocalView<>& EventDataControlComposite<AtomicIndirectorType>::GetQmEventDataControlLocal()
    const noexcept
{
    return asil_qm_control_local_;
}

template <template <class> class AtomicIndirectorType>
SkeletonEventDataControlLocalView<>*
EventDataControlComposite<AtomicIndirectorType>::GetAsilBEventDataControlLocal() noexcept
{
    return asil_b_control_local_;
}

template <template <class> class AtomicIndirectorType>
ProxyEventDataControlLocalView<>&
EventDataControlComposite<AtomicIndirectorType>::GetProxyEventDataControlLocalView() noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(proxy_control_local_ != nullptr);
    return *proxy_control_local_;
}

template <template <class> class AtomicIndirectorType>
EventSlotStatus::EventTimeStamp EventDataControlComposite<AtomicIndirectorType>::GetEventSlotTimestamp(
    const SlotIndexType slot) const noexcept
{
    if (asil_b_control_local_ != nullptr)
    {
        const EventSlotStatus event_slot_status{(*asil_b_control_local_)[slot]};
        const EventSlotStatus::EventTimeStamp sample_timestamp{event_slot_status.GetTimeStamp()};
        return sample_timestamp;
    }
    else
    {
        const EventSlotStatus event_slot_status{asil_qm_control_local_.get()[slot]};
        const EventSlotStatus::EventTimeStamp sample_timestamp{event_slot_status.GetTimeStamp()};
        return sample_timestamp;
    }
}

template <template <class> class AtomicIndirectorType>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from 'state_slots_[]' which might leds to a segmentation fault
// in case the index goes outside the range. As we already do an index check before accessing, so no way for
// segmentation fault which leds to calling std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
EventSlotStatus::EventTimeStamp EventDataControlComposite<AtomicIndirectorType>::GetLatestTimestamp() const noexcept
{
    EventSlotStatus::EventTimeStamp latest_time_stamp{1U};
    SkeletonEventDataControlLocalView<>& control =
        (asil_b_control_local_ != nullptr) ? *asil_b_control_local_ : asil_qm_control_local_.get();
    for (SlotIndexType slot_index = 0U;
         // Suppress "AUTOSAR C++14 A4-7-1" rule finding. This rule states: "An integer expression shall not lead to
         // loss.". As the maximum number of slots is std::uint16_t, so there is no case for a data loss here.
         // coverity[autosar_cpp14_a4_7_1_violation]
         slot_index < static_cast<SlotIndexType>(control.state_slots_.size());
         ++slot_index)
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(static_cast<std::size_t>(slot_index) < control.state_slots_.size());
        const EventSlotStatus slot{control.state_slots_[slot_index].load(std::memory_order_acquire)};
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

template class EventDataControlComposite<memory::shared::AtomicIndirectorReal>;
template class EventDataControlComposite<memory::shared::AtomicIndirectorMock>;

}  // namespace score::mw::com::impl::lola
