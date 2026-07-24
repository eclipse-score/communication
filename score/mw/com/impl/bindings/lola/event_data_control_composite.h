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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_DATA_CONTROL_COMPOSITE_H_
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_DATA_CONTROL_COMPOSITE_H_

#include "score/mw/com/impl/bindings/lola/consumer_event_data_control_local_view.h"
#include "score/mw/com/impl/bindings/lola/control_slot_types.h"
#include "score/mw/com/impl/bindings/lola/event_slot_status.h"
#include "score/mw/com/impl/bindings/lola/provider_event_data_control_local_view.h"

#include "score/concurrency/atomic_indirector.h"

#include <functional>

namespace score::mw::com::impl::lola
{

class EventDataControlCompositeAttorney;

/// \brief Encapsulates multiple EventDataControl instances
///
/// \details Due to the fact that we have multiple EventDataControl instances (one for ASIL, one for QM) we need to
/// operate the control information on both instances. In order to be scalable and not clutter this information in the
/// whole codebase, we implemented this composite which takes care of setting the status correctly in all underlying
/// control structures. Please be aware that the control structures will live in different shared memory segments, thus
/// it is not possible to store them by value, but rather as pointer.
template <template <class> class AtomicIndirectorType = concurrency::AtomicIndirectorReal>
class EventDataControlComposite
{
    // Suppress "AUTOSAR C++14 A11-3-1", The rule declares: "Friend declarations shall not be used".
    // The "EventDataControlCompositeAttorney" class is a helper, which sets the internal state of
    // "EventDataControlComposite" accessing private members and used for testing purposes only.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class lola::EventDataControlCompositeAttorney;

  public:
    /// \brief Result returned by AllocateNextSlot()
    struct AllocationResult
    {
        // \brief the index of the slot reserved for writing (potentially in QM and ASIL-B control section) if a slot
        // could be found. Otherwise, an empty optional.
        std::optional<SlotIndexType> allocated_slot_index;

        // \brief A flag indicating whether QM consumers should be ignored due to misbehaviour. In this case, QM
        // consumers have been disconnected and therefore the QM related slots are ignored.
        bool qm_misbehaved;
    };

    /// \brief Constructs a composite which will only manage a single QM control (no ASIL use-case)
    explicit EventDataControlComposite(ProviderEventDataControlLocalView<AtomicIndirectorType>& asil_qm_control_local);

    /// \brief Constructs a composite which will manage QM and ASIL control at the same time
    explicit EventDataControlComposite(
        ProviderEventDataControlLocalView<AtomicIndirectorType>& asil_qm_control_local,
        ProviderEventDataControlLocalView<AtomicIndirectorType>* const asil_b_control_local);
    /// \brief SampleAllocateePtr stores a raw pointer to this object, so any copy or move would silently invalidate
    /// that pointer, causing undefined behaviour.
    EventDataControlComposite(const EventDataControlComposite&) = delete;
    EventDataControlComposite& operator=(const EventDataControlComposite&) = delete;
    EventDataControlComposite(EventDataControlComposite&&) = delete;
    EventDataControlComposite& operator=(EventDataControlComposite&&) = delete;
    /// \brief Checks for the oldest unused slot and acquires for writing (thread-safe, wait-free)
    ///
    /// \details This method will perform retries (bounded) on data-races. In order to ensure that _always_
    /// a slot is found, it needs to be ensured that:
    /// * enough slots are allocated (sum of all possible max allocations by consumer + 1)
    /// * enough retries are performed (currently max number of parallel actions is restricted to 50 (number of
    /// possible transactions (2) * number of parallel actions = number of retries))
    ///
    /// Note that this function will operate simultaneously on the QM and ASIL structure. If a data-race occurs,
    /// rollback mechanisms are in place. Thus, if this function returns positively, it is guaranteed that the slot has
    /// been allocated in all underlying control structures.
    ///
    /// \return Struct containing index of slot which was marked for writing if possible and a flag indicating whether a
    /// qm consumer misbehaved.
    /// \post EventReady() is invoked to withdraw write-ownership
    AllocationResult AllocateNextSlot() noexcept;

    /// \brief Indicates that a slot is ready for reading - writing has finished. (thread-safe, wait-free)
    /// \pre AllocateNextSlot() was invoked to obtain write-ownership
    void EventReady(const SlotIndexType slot_index, EventSlotStatus::EventTimeStamp time_stamp) noexcept;

    /// \brief Marks selected slot as invalid, if it was not yet marked as ready (thread-safe, wait-free)
    /// \pre AllocateNextSlot() was invoked to obtain write-ownership
    void Discard(const SlotIndexType slot_index);

    /// \brief Indicates, whether the QM control part of the composite has been disconnected due to QM consumer mis-
    ///        behaviour or not.
    /// \return _true_ if disconnected and the composite supports QM/ASIL parts, _false_ else.
    bool IsQmControlDisconnected() const noexcept;

    /// \brief Returns the (mandatory) ProviderEventDataControlLocalView for QM.
    ProviderEventDataControlLocalView<AtomicIndirectorType>& GetQmEventDataControlLocal() const& noexcept;

    /// \brief Returns a pointer to ProviderEventDataControlLocalView for ASIL-B
    /// \return a nullptr if no ASIL-B support, otherwise, a valid pointer to the ASIL-B EventDataControl.
    ProviderEventDataControlLocalView<AtomicIndirectorType>* GetAsilBEventDataControlLocal() noexcept;

    /// \brief Returns the timestamp of the provided slot index
    EventSlotStatus::EventTimeStamp GetEventSlotTimestamp(const SlotIndexType slot_index) const noexcept;

    /// \brief Returns the latest timestamp of all slots which are currently marked as ready (i.e. have been written to
    /// by a provider and can be consumet)
    ///
    /// This function is used in the partial restart case in which a skeleton instance dies and restarts while there are
    /// still proxies connected to the old shared memory region. In this case, the skeleton will open and continue to
    /// use the old region. We must make sure that the current timestamp of the new skeleton is updated based on the
    /// latest timestamp in the old shared memory region to ensure that the referencing semantics related to the
    /// timestamp remain correct.
    EventSlotStatus::EventTimeStamp GetLatestTimestamp() const noexcept;

  private:
    std::reference_wrapper<ProviderEventDataControlLocalView<AtomicIndirectorType>> asil_qm_control_local_;
    ProviderEventDataControlLocalView<AtomicIndirectorType>* asil_b_control_local_;

    /// \brief flag indicating, whether qm_control part shall be ignored in any public API (AllocateNextSlot(),
    /// EventReady(), Discard()()
    bool ignore_qm_control_;

    // Algorithms that operate on multiple control blocks
    // \post the returned selected free slot for qm and asil-b must contain the same index and slot value (therefore, we
    //       only return a single SlotInfo which represents both qm and asil-b slots).
    std::optional<typename ProviderEventDataControlLocalView<AtomicIndirectorType>::SlotInfo> GetNextFreeMultiSlot()
        const noexcept;

    std::optional<SlotIndexType> AllocateNextMultiSlot() noexcept;
    void CheckForValidDataControls() const noexcept;
};

template <template <class> class AtomicIndirectorType>
inline auto EventDataControlComposite<AtomicIndirectorType>::AllocateNextSlot() noexcept -> AllocationResult
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
inline ProviderEventDataControlLocalView<AtomicIndirectorType>*
EventDataControlComposite<AtomicIndirectorType>::GetAsilBEventDataControlLocal() noexcept
{
    return asil_b_control_local_;
}

template <template <class> class AtomicIndirectorType>
inline auto EventDataControlComposite<AtomicIndirectorType>::EventReady(
    const SlotIndexType slot_index,
    EventSlotStatus::EventTimeStamp time_stamp) noexcept -> void
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

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_DATA_CONTROL_COMPOSITE_H_
