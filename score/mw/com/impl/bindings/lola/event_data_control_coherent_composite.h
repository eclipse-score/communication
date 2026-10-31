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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_DATA_CONTROL_COHERENT_COMPOSITE_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_DATA_CONTROL_COHERENT_COMPOSITE_H

#include "score/mw/com/impl/bindings/lola/coherent_event_slot_status.h"
#include "score/mw/com/impl/bindings/lola/control_slot_types.h"
#include "score/mw/com/impl/bindings/lola/provider_event_data_control_coherent_local_view.h"

#include <functional>
#include <optional>

namespace score::mw::com::impl::lola
{

/// \brief Encapsulates one or two ProviderEventDataControlCoherentLocalView instances (QM only, or QM and ASIL-B).
///
/// \details This is class realizes for a CoherencyType::kCoherent event the equivalent composite pattern as
/// EventDataControlComposite does for CoherencyType::kNonCoherent event.
///
/// Unlike CoherencyType::kNonCoherent, a kCoherent consumer never writes to its control section at all (see
/// Discard()), so the only variable a QM consumer could possibly corrupt (by writing into shared memory it is only
/// supposed to read) is the QM control section's copy of newestIndex_/the per-slot timestamps. If an ASIL-B control
/// section is present, this class detects such a divergence between the QM and ASIL-B control sections (which, as
/// long as nothing misbehaves, should always be in lock-step, since only the trusted producer ever writes to
/// either) and "disconnects" the QM control section from that point onwards (no longer written to, no longer
/// trusted for reads), mirroring EventDataControlComposite's ignore_qm_control_ handling for
/// CoherencyType::kNonCoherent.
class EventDataControlCoherentComposite final
{
  public:
    /// \brief Constructs a composite which will only manage a single QM control (no ASIL-B use-case).
    explicit EventDataControlCoherentComposite(ProviderEventDataControlCoherentLocalView& qm_control_local) noexcept;

    /// \brief Constructs a composite which will manage QM and ASIL-B control at the same time.
    EventDataControlCoherentComposite(ProviderEventDataControlCoherentLocalView& qm_control_local,
                                      ProviderEventDataControlCoherentLocalView* const asil_b_control_local) noexcept;

    /// \brief SampleAllocateePtr stores a raw pointer to this object, so any copy or move would silently invalidate
    /// that pointer, causing undefined behaviour.
    EventDataControlCoherentComposite(const EventDataControlCoherentComposite&) = delete;
    EventDataControlCoherentComposite& operator=(const EventDataControlCoherentComposite&) = delete;
    EventDataControlCoherentComposite(EventDataControlCoherentComposite&&) = delete;
    EventDataControlCoherentComposite& operator=(EventDataControlCoherentComposite&&) = delete;
    ~EventDataControlCoherentComposite() noexcept = default;

    /// \brief Computes the next slot to be written to, in a ring-buffer fashion. Always succeeds, as there is no
    /// concurrent producer/consumer access to race against.
    ///
    /// \details If an ASIL-B control is present and not yet disconnected (see IsQmControlDisconnected()), this is
    /// computed from both the QM and the ASIL-B control and the two results are compared: a divergence is a sign of
    /// QM control section corruption (see class doc) and triggers disconnecting the QM control section from this
    /// point onwards.
    /// \return the index of the slot to write to next (valid for both the QM and, if present, ASIL-B control).
    /// \post EventReady() is invoked once writing has finished, to publish the slot in both control structures.
    SlotIndexType AllocateNextSlot() noexcept;

    /// \brief Publishes the given slot in the ASIL-B control structure (if present) and, unless the QM control
    /// section has been disconnected (see IsQmControlDisconnected()), the QM control structure.
    /// \pre AllocateNextSlot() was invoked to obtain the slot_index to write to.
    void EventReady(const SlotIndexType slot_index, const CoherentEventSlotStatus::EventTimeStamp time_stamp) noexcept;

    /// \brief No-op (see ProviderEventDataControlCoherentLocalView::Discard()).
    void Discard(const SlotIndexType slot_index) const noexcept;

    /// \brief Returns the latest timestamp of the slot which is currently marked as the most recently published one,
    /// or CoherentEventSlotStatus::INVALID_TIMESTAMP, if none has been published yet (delegates to the ASIL-B view,
    /// if present, to avoid trusting a potentially corrupted QM control section - see class doc; otherwise to the
    /// QM view).
    ///
    /// Used in the partial restart case, mirroring EventDataControlComposite::GetLatestTimestamp() for
    /// CoherencyType::kNonCoherent.
    CoherentEventSlotStatus::EventTimeStamp GetLatestTimestamp() const noexcept;

    /// \brief Returns whether the QM control section has been disconnected due to a detected divergence between the
    /// QM and ASIL-B control sections (always false if no ASIL-B control is present).
    bool IsQmControlDisconnected() const noexcept;

  private:
    std::reference_wrapper<ProviderEventDataControlCoherentLocalView> qm_control_local_;
    ProviderEventDataControlCoherentLocalView* asil_b_control_local_;
    bool ignore_qm_control_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_DATA_CONTROL_COHERENT_COMPOSITE_H
