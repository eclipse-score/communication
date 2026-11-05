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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_DATA_CONTROL_MIXED_COMPOSITE_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_DATA_CONTROL_MIXED_COMPOSITE_H

#include "score/mw/com/impl/bindings/lola/control_slot_types.h"
#include "score/mw/com/impl/bindings/lola/event_data_control_composite.h"
#include "score/mw/com/impl/bindings/lola/event_data_control_mixed.h"
#include "score/mw/com/impl/bindings/lola/event_slot_status.h"

#include <functional>

namespace score::mw::com::impl::lola
{

/// \brief Owns the EventDataControlComposite<> for a CoherencyType::kMixed event's QM and (if present) ASIL-B
/// EventDataControlMixed instance(s).
///
/// \details For a CoherencyType::kMixed event, the CONTROL segment layout (and therefore the provider-side
/// slot-allocation algorithm) is identical to CoherencyType::kNonCoherent (see EventDataControlMixed), which is why
/// GetComposite() hands out the same EventDataControlComposite<> used for CoherencyType::kNonCoherent events, for
/// callers (e.g. SkeletonEvent::Allocate()) that only care about that shared behaviour.
///
/// The one thing that is specific to CoherencyType::kMixed is EventReady() also needing to update newest_index_ on
/// the given EventDataControlMixed instance(s), giving coherent consumers of a CoherencyType::kMixed event a
/// starting point for their GetNewSamples() search (see Nautilus_LoLa.md, "Mixed Intra/Inter-Chain" / "Publishing
/// Steps"). This class exists so that callers (e.g. SkeletonEvent::Send()) can invoke a single EventReady() without
/// having to separately remember to also update newest_index_.
class EventDataControlMixedComposite final
{
  public:
    /// \brief Constructs a composite which will only manage a single QM control (no ASIL-B use-case).
    explicit EventDataControlMixedComposite(EventDataControlMixed& qm_control) noexcept;

    /// \brief Constructs a composite which will manage QM and ASIL-B control at the same time.
    EventDataControlMixedComposite(EventDataControlMixed& qm_control,
                                   EventDataControlMixed* const asil_b_control) noexcept;

    /// \brief SampleAllocateePtr stores a raw pointer to the underlying EventDataControlComposite<>, so any copy or
    /// move would silently invalidate that pointer, causing undefined behaviour.
    EventDataControlMixedComposite(const EventDataControlMixedComposite&) = delete;
    EventDataControlMixedComposite& operator=(const EventDataControlMixedComposite&) = delete;
    EventDataControlMixedComposite(EventDataControlMixedComposite&&) = delete;
    EventDataControlMixedComposite& operator=(EventDataControlMixedComposite&&) = delete;
    ~EventDataControlMixedComposite() noexcept = default;

    /// \brief Publishes the given slot in the underlying EventDataControlComposite<> (see
    /// EventDataControlComposite<>::EventReady()) and additionally updates newest_index_ on the QM and (if present)
    /// ASIL-B EventDataControlMixed instance(s).
    void EventReady(const SlotIndexType slot_index, EventSlotStatus::EventTimeStamp time_stamp) noexcept;

    /// \brief Returns the underlying EventDataControlComposite<>, shared with CoherencyType::kNonCoherent for
    /// anything besides EventReady() (e.g. AllocateNextSlot(), Discard(), GetLatestTimestamp(), or storing it inside
    /// a SampleAllocateePtr).
    EventDataControlComposite<>& GetComposite() noexcept;

  private:
    EventDataControlComposite<> composite_;
    std::reference_wrapper<EventDataControlMixed> qm_control_;
    EventDataControlMixed* asil_b_control_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_DATA_CONTROL_MIXED_COMPOSITE_H
