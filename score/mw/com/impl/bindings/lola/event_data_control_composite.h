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
#ifndef DDAD_SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_DATA_CONTROL_COMPOSITE_H_
#define DDAD_SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_DATA_CONTROL_COMPOSITE_H_

#include "score/mw/com/impl/bindings/lola/control_slot_composite_indicator.h"
#include "score/mw/com/impl/bindings/lola/control_slot_types.h"
#include "score/mw/com/impl/bindings/lola/event_data_control.h"
#include "score/mw/com/impl/bindings/lola/event_slot_status.h"

#include "score/memory/shared/atomic_indirector.h"

#include <optional>
#include <tuple>
#include <utility>

namespace score::mw::com::impl::lola
{

class EventDataControlCompositeAttorney;

namespace detail_event_data_control_composite
{

/// \brief Encapsulates multiple EventDataControl instances
///
/// \details Due to the fact that we have multiple EventDataControl instances (one for ASIL, one for QM) we need to
/// operate the control information on both instances. In order to be scalable and not clutter this information in the
/// whole codebase, we implemented this composite which takes care of setting the status correctly in all underlying
/// control structures. Please be aware that the control structures will live in different shared memory segments, thus
/// it is not possible to store them by value, but rather as pointer.
template <template <class> class AtomicIndirectorType = memory::shared::AtomicIndirectorReal>
class EventDataControlCompositeImpl
{
    // Suppress "AUTOSAR C++14 A11-3-1", The rule declares: "Friend declarations shall not be used".
    // The "EventDataControlCompositeAttorney" class is a helper, which sets the internal state of
    // "EventDataControlCompositeImpl" accessing private members and used for testing purposes only.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class lola::EventDataControlCompositeAttorney;

  public:
    /// \brief Constructs a composite which will only manage a single QM control (no ASIL use-case)
    explicit EventDataControlCompositeImpl(EventDataControl* const asil_qm_control);

    /// \brief Constructs a composite which will manage QM and ASIL control at the same time
    explicit EventDataControlCompositeImpl(EventDataControl* const asil_qm_control,
                                           EventDataControl* const asil_b_control);

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
    /// \return A valid ControlSlotCompositeIndicator "pointing" to a reserved slot for writing (potentially in QM and
    ///         ASIL-B control section) if successful. If the underlying event was enabled for QM AND ASIL-B and the
    ///         returned ControlSlotCompositeIndicator has no valid QM pointer, it means, that QM consumers have been
    ///         disconnected and therefore the QM related slots are ignored.
    /// \post EventReady() is invoked to withdraw write-ownership
    ControlSlotCompositeIndicator AllocateNextSlot() noexcept;

    /// \brief Indicates that a slot is ready for reading - writing has finished. (thread-safe, wait-free)
    /// \pre AllocateNextSlot() was invoked to obtain write-ownership
    void EventReady(ControlSlotCompositeIndicator slot_indicator, EventSlotStatus::EventTimeStamp time_stamp) noexcept;

    /// \brief Marks selected slot as invalid, if it was not yet marked as ready (thread-safe, wait-free)
    /// \pre AllocateNextSlot() was invoked to obtain write-ownership
    void Discard(ControlSlotCompositeIndicator slot_indicator);

    /// \brief Indicates, whether the QM control part of the composite has been disconnected due to QM consumer mis-
    ///        behaviour or not.
    /// \return _true_ if disconnected and the composite supports QM/ASIL parts, _false_ else.
    bool IsQmControlDisconnected() const noexcept;

    /// \brief Returns the (mandatory) EventDataControl for QM.
    EventDataControl& GetQmEventDataControl() const noexcept;

    /// \brief Returns the optional EventDataControl for ASIL-B
    /// \return an empty optional if no ASIL-B support, otherwise, a valid pointer to the ASIL-B EventDataControl.
    std::optional<EventDataControl*> GetAsilBEventDataControl() noexcept;

    /// \brief Returns the timestamp of the provided slot index
    EventSlotStatus::EventTimeStamp GetEventSlotTimestamp(const SlotIndexType slot) const noexcept;

    EventSlotStatus::EventTimeStamp GetLatestTimestamp() const noexcept;

  private:
    EventDataControl* asil_qm_control_;
    EventDataControl* asil_b_control_;

    /// \brief flag indicating, whether qm_control part shall be ignored in any public API (AllocateNextSlot(),
    /// EventReady(), Discard()()
    bool ignore_qm_control_;

    // Algorithms that operate on multiple control blocks
    ControlSlotCompositeIndicator GetNextFreeMultiSlot() const noexcept;

    bool TryLockSlot(ControlSlotCompositeIndicator slot_indicator) noexcept;
    ControlSlotCompositeIndicator AllocateNextMultiSlot() noexcept;
    void CheckForValidDataControls() const noexcept;
};

}  // namespace detail_event_data_control_composite

using EventDataControlComposite = detail_event_data_control_composite::EventDataControlCompositeImpl<>;

}  // namespace score::mw::com::impl::lola
#endif  // DDAD_SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_DATA_CONTROL_COMPOSITE_H_
