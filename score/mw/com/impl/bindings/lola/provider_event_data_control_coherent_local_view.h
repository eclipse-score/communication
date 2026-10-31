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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_PROVIDER_EVENT_DATA_CONTROL_COHERENT_LOCAL_VIEW_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_PROVIDER_EVENT_DATA_CONTROL_COHERENT_LOCAL_VIEW_H

#include "score/mw/com/impl/bindings/lola/coherent_event_slot_status.h"
#include "score/mw/com/impl/bindings/lola/control_slot_types.h"
#include "score/mw/com/impl/bindings/lola/event_data_control_coherent.h"

#include <score/span.hpp>

#include <cstddef>

namespace score::mw::com::impl::lola
{

/// \brief View class which provides functionality for interacting with EventDataControlCoherent.
///
/// \details This is the CoherencyType::kCoherent counterpart of ProviderEventDataControlLocalView. Since the caller
/// guarantees (e.g. via activity-chain scheduling) that the producer and all of its consumers of a kCoherent event
/// never execute concurrently, no retries / atomic compare-exchange are needed: AllocateNextSlot() simply computes the
/// next slot in a ring-buffer fashion (based on newest_index_), without claiming/mutating any state. Only EventReady()
/// actually writes the slot and advances newest_index_.
///
/// As with ProviderEventDataControlLocalView, the data in EventDataControlCoherent is created / opened once during
/// Skeleton creation, and is then accessed during runtime via this local view, to avoid repeated (performance
/// impacting) OffsetPtr dereferences.
class ProviderEventDataControlCoherentLocalView final
{
  public:
    using LocalEventControlSlots = score::cpp::span<CoherentControlSlotType>;

    explicit ProviderEventDataControlCoherentLocalView(EventDataControlCoherent& event_data_control) noexcept;

    ~ProviderEventDataControlCoherentLocalView() noexcept = default;

    ProviderEventDataControlCoherentLocalView(const ProviderEventDataControlCoherentLocalView&) = delete;
    ProviderEventDataControlCoherentLocalView& operator=(const ProviderEventDataControlCoherentLocalView&) = delete;
    ProviderEventDataControlCoherentLocalView(ProviderEventDataControlCoherentLocalView&&) noexcept = delete;
    ProviderEventDataControlCoherentLocalView& operator=(ProviderEventDataControlCoherentLocalView&& other) noexcept =
        delete;

    /// \brief Computes the next slot to be written to, in a ring-buffer fashion.
    ///
    /// \details Unlike ProviderEventDataControlLocalView::AllocateNextSlot(), this does not claim/mutate any state
    /// (no "in writing" marker exists for CoherencyType::kCoherent slots): the returned slot is simply the one
    /// following the slot referenced by newest_index_ (or slot 0, if no slot has been written yet). It is always
    /// guaranteed to succeed, as there is no concurrent producer/consumer access to race against.
    ///
    /// \return the index of the slot to write to next.
    /// \post EventReady() is invoked once writing has finished, to publish the slot.
    SlotIndexType AllocateNextSlot() const noexcept;

    /// \brief Publishes the given slot: writes the given timestamp and advances newest_index_ to point to it.
    /// \pre AllocateNextSlot() was invoked to obtain the slot_index to write to.
    void EventReady(const SlotIndexType slot_index, const CoherentEventSlotStatus::EventTimeStamp time_stamp) noexcept;

    /// \brief No-op: since AllocateNextSlot() does not claim/mutate any state, there is nothing to roll back if
    /// writing is aborted before EventReady() is called.
    void Discard(const SlotIndexType slot_index) const noexcept;

    /// \brief Directly access CoherentEventSlotStatus for one specific slot
    CoherentEventSlotStatus operator[](const SlotIndexType slot_index) const noexcept;

  private:
    LocalEventControlSlots state_slots_;

    /// \brief Pointer to the newest_index_ member of the (shared-memory resident) EventDataControlCoherent. Accessed
    /// directly (no further indirection necessary, as it is a plain member of an object which was already resolved
    /// once, via the event_data_control reference handed to the constructor).
    EventDataControlCoherent* event_data_control_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_PROVIDER_EVENT_DATA_CONTROL_COHERENT_LOCAL_VIEW_H
